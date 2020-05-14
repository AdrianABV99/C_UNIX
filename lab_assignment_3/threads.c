#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/stat.h>

#define BUFF 4096

typedef struct stats{
    volatile int lettersFreq[60]; // total letter freq array
    volatile int nrLetters; // total nr of letters
}st_data;

typedef struct thread_workload{
    char* data;
    int idx, size; // id and size of data;
}st_thread_data;

volatile st_data statistics; // global stats struct
volatile int nrOfThreads; // nr of running threads
pthread_mutex_t running_mutex; // mutex used for locking
int fdPipePW[2], fdPipePP[2]; // two pipes needed

void waitForChild(int pid) {
    pid_t c_id;
    int status;
    while((c_id = waitpid(pid, &status, 0)) > 0) {
        printf("CHILD with PID - %d ended normally with status code: %d\n", c_id, WEXITSTATUS(status));
    }
}

void* threadCode(void* arg) {
    // get thread
    st_thread_data curThreadData = *(st_thread_data*)arg;
    // parse on its size
    for(int i = 0; i < curThreadData.size; ++i) {
        if(isalpha(curThreadData.data[i])) {
            // lock mutex to increase letterFreq and number
            pthread_mutex_lock(&running_mutex);
            statistics.lettersFreq[curThreadData.data[i] - 'A']++;
            statistics.nrLetters++;
            pthread_mutex_unlock(&running_mutex);
        }
    }
    pthread_mutex_lock(&running_mutex);
    // decrease running threads number
    --nrOfThreads;
    pthread_mutex_unlock(&running_mutex);
    pthread_exit(NULL);
}

void writeToWriteChild() {
    char line[BUFF];
    FILE* redirectedStream = fdopen(fdPipePW[1], "w");
    if(redirectedStream == NULL) {
        perror("Error while redirecting stream (process child)\n");
        exit(-301);
    }
    for(int i = 'A'; i <= 'z'; ++i) {
        if(isalpha((char) i)) {
            float percentage = (float)statistics.lettersFreq[i - 'A'] / statistics.nrLetters * 100.0;
            snprintf(line, BUFF, "%c: %f%%\n", (char) i, percentage);
            fprintf(redirectedStream, "%s", line);
        }
    }
    snprintf(line, BUFF, "Total number of letters: %d\n", statistics.nrLetters);
    fprintf(redirectedStream, "%s", line);
    fclose(redirectedStream);
    close(fdPipePW[1]);
}


void writeToParent(int nrOfThredsUsed) {
    FILE* outstream = fdopen(fdPipePP[1], "w");
    if(outstream == NULL) {
        perror("Error while redirecting stream (parent)\n");
        exit(-301);
    }
    fprintf(outstream, "%d", nrOfThredsUsed);
    fclose(outstream);
    close(fdPipePP[1]);
}

void computingChildCode(char* inputPath, int maxNrOfThreads, int dataChunkSize) {
    int bytesRead, counterT = 0;
    struct stat fileStat;
    int requiredThreads = 0;
    int inputFile = open(inputPath, O_RDONLY);
    if(inputFile < 0) {
        perror("Error while opening input file\n");
        exit(-4);
    }
    if(pthread_mutex_init(&running_mutex, NULL) != 0) {
        fprintf(stderr, "Error while initing mutex\n");
        exit(-10);
    }
    // get file size
    if(stat(inputPath, &fileStat) == -1) {
        fprintf(stderr, "Error while getting stat\n");
        exit(-1000);
    }
    requiredThreads = fileStat.st_size / dataChunkSize + 1; // plus one for last thread free
    // init arrays
    st_thread_data thread_data[requiredThreads];
    pthread_t threads[requiredThreads];
    // allocate memory for data
    thread_data[counterT].data = malloc(sizeof(char) * dataChunkSize);
    bytesRead = read(inputFile, thread_data[counterT].data, dataChunkSize - 1);
    while(bytesRead > 0) {
        // we still didn't reach the limit
        pthread_mutex_lock(&running_mutex);
        if(nrOfThreads < maxNrOfThreads) {
            // init fields
            // put null at end of string
            thread_data[counterT].data[bytesRead] = '\0';
            thread_data[counterT].idx = counterT;
            thread_data[counterT].size = bytesRead;
            // create thread
            // stop child if error occurs
            if(pthread_create(&threads[counterT], NULL, threadCode, &thread_data[counterT]) != 0) {
                fprintf(stderr, "Error while creating thread! Terminating...\n");
                exit(-11);
            }
            ++nrOfThreads;
            pthread_mutex_unlock(&running_mutex);
            ++counterT;
            thread_data[counterT].data = malloc(sizeof(char) * dataChunkSize);
            bytesRead = read(inputFile, thread_data[counterT].data, dataChunkSize - 1);
        } else {
            pthread_mutex_unlock(&running_mutex);
            continue;
        }
    }
    for(int i = 0; i < counterT; ++i) {
        // wait for remaining threads to finish
        int res;
        if((res = pthread_join(threads[i], NULL)) != 0) {
            fprintf(stderr, "Error while joing!Err msg:%s \n", strerror(res));
            exit(-1000);
        }
        // free memory
        free(thread_data[i].data);
    }
    // free last thread. this was not created
    // but memory was allocated to it
    free(thread_data[counterT].data);

    //send data to second child
    writeToWriteChild();
    //send data to parent
    writeToParent(counterT);

    //destroy mutex
    pthread_mutex_destroy(&running_mutex);
    //close inputFile
    close(inputFile);
}


void writingChildCode(char* outPath) {
    printf("Writing child with PID: %d; beginning\n", getpid());

    int outputFile;
    FILE* inputStream = fdopen(fdPipePW[0], "r");

    if(inputStream == NULL) {
        perror("Error while redirecting stream (writing child)\n");
        exit(-301);
    }
    outputFile = open(outPath, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH);
    if(outputFile < 0) {
        perror("Error while opening output file\n");
        exit(-4);
    }
    char line[BUFF];
    while(fgets(line, BUFF, inputStream) != NULL) {
        if(write(outputFile, line, strlen(line)) == -1) {
            perror("Error while writing to output\n");
            exit(-400);
        }
    }
    close(outputFile);
    fclose(inputStream);
    close(fdPipePW[0]);
    printf("Writing child with PID: %d; ending\n", getpid());
}

void createWritingChild(char* outPath) {
    int pid_write;
    if((pid_write = fork()) < 0) {
        perror("Error while creating writing child\n");
        exit(-3);
    }
    if(pid_write == 0) {
        close(fdPipePP[0]);
        close(fdPipePP[1]); //close both ends for second pipe
        close(fdPipePW[1]); // close writing end for first pipe
        // write child code
        writingChildCode(outPath);
        exit(0);
    }
}
void createProcessingChild(char** argv) {
    int pid_comp;
    if((pid_comp = fork()) < 0) {
        perror("Error while creating computing child\n");
        exit(-2);
    }
    if(pid_comp == 0) {
        close(fdPipePW[0]); // close first pipe read
        close(fdPipePP[0]); // close second pipe write
        // computing child code
        computingChildCode(argv[1], atoi(argv[2]), atoi(argv[3]));
        exit(0);
    }
}

void createPipes() {
    /// pipe from process to write
    if(pipe(fdPipePW) == -1) {
        perror("Error while creating first pipe\n");
        exit(-200);
    }
    // pipe from process to parent
    if(pipe(fdPipePP) == -1) {
        perror("Error while creating first pipe\n");
        exit(-201);
    }
}

void readFromChild() {
    // redirect pipe to stream;
    int usedThreads;
    FILE* pipeStream = fdopen(fdPipePP[0], "r");
    if(pipeStream == NULL) {
        perror("Error while opening stream\n");
        exit(-300);
    }
    fscanf(pipeStream, "%d", &usedThreads);
    printf("Number of threads used: %d\n", usedThreads);
    
    fclose(pipeStream);
    close(fdPipePP[0]); // close read end after reading from pipe
}

int main(int argc, char** argv) {
    printf("Parent process with PID: %d; beginning\n", getpid());
    if(argc != 5) {
        fprintf(stderr, "Invalid number of arguments! Usage: ./exec <input_file_name> <max_nr_of_threads> <data_chunk_size> <histogram_output_text_file>\n");
        exit(-1);
    }
    createPipes();
    createProcessingChild(argv);
    createWritingChild(argv[4]);

    // close pipes
    close(fdPipePW[0]);
    close(fdPipePW[1]); // close both ends on first pipe
    close(fdPipePP[1]); // close write dn to second pipe

    readFromChild();
    waitForChild(-1);
    

    printf("Parent process with PID: %d; ending\n", getpid());
    return 0;
}