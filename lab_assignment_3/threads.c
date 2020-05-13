#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

typedef struct stats{
    volatile int lettersFreq[60]; // total letter freq array
    volatile int nrLetters; // total nr of letters
}st_data;

typedef struct thread_workload{
    char* data;
    int idx, size; // id and size of data;
    int running; //used to check if it is still running
}st_thread_data;

volatile st_data statistics; // global stats struct
volatile int nrOfThreads; // nr of running threads
volatile int idxT; // cur thread id
pthread_mutex_t running_mutex; // mutex used for locking

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
        if(curThreadData.data[i] >= 'A' && curThreadData.data[i] <= 'z') {
            // lock mutex to increase letterFreq and number
            pthread_mutex_lock(&running_mutex);
            statistics.lettersFreq[curThreadData.data[i] - 'A']++;
            statistics.nrLetters++;
            pthread_mutex_unlock(&running_mutex);
        }
    }
    pthread_mutex_lock(&running_mutex);
    // mark this id for finishing
    idxT = curThreadData.idx;
    // mark that thread is not running anymore
    curThreadData.running = 0; 
    // decrease running threads number
    --nrOfThreads;
    pthread_mutex_unlock(&running_mutex);
    pthread_exit(NULL);
}

void computingChildCode(char* inputPath, int maxNrOfThreads, int dataChunkSize) {
    st_thread_data thread_data[300];
    pthread_t threads[300];
    int bytesRead, counterT = 0;
    int inputFile = open(inputPath, O_RDONLY);
    if(inputFile < 0) {
        perror("Error while opening input file\n");
        exit(-4);
    }
    if(pthread_mutex_init(&running_mutex, NULL) != 0) {
        fprintf(stderr, "Error while initing mutex\n");
        exit(-10);
    }
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
        free(thread_data[i].data);
    }
    free(thread_data[counterT].data);
    // for(int i = 'A'; i <= 'z'; ++i) {
    //     printf("%c %d\n", (char) i,  statistics.lettersFreq[i - 'A']);
    // }
    // printf("%d\n", statistics.nrLetters);

    pthread_mutex_destroy(&running_mutex);
    close(inputFile);
}


void writingChildCode() {
    printf("Writing child with PID: %d; beginning\n", getpid());

    printf("Writing child with PID: %d; ending\n", getpid());
}

int main(int argc, char** argv) {
    printf("Parent process with PID: %d; beginning\n", getpid());
    int pid_comp, pid_write;
    if(argc != 5) {
        fprintf(stderr, "Invalid number of arguments! Usage: ./exec <input_file_name> <max_nr_of_threads> <data_chunk_size> <histogram_output_text_file>\n");
        exit(-1);
    }
    
    if((pid_comp = fork()) < 0) {
        perror("Error while creating computing child\n");
        exit(-2);
    }
    if(pid_comp == 0) {
        // computing child code
        computingChildCode(argv[1], atoi(argv[2]), atoi(argv[3]));
        exit(0);
    }

    if((pid_write = fork()) < 0) {
        perror("Error while creating writing child\n");
        exit(-3);
    }
    if(pid_write == 0) {
        // write child code
        writingChildCode();
        exit(0);
    }

    waitForChild(-1);

    printf("Parent process with PID: %d; ending\n", getpid());
    return 0;
}