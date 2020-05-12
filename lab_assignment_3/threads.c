#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

typedef struct {
    int lettersFreq[53];
    int nrLetters;
} st_data;

st_data statistics;
volatile int nrOfThreads;
pthread_mutex_t running_mutex;

void waitForChild(int pid) {
    pid_t c_id;
    int status;
    while((c_id = waitpid(pid, &status, 0)) > 0) {
        printf("CHILD with PID - %d ended normally with status code: %d\n", c_id, WEXITSTATUS(status));
    }
}

void* threadCode(void* arg) {
    char* buffer = (char*) arg;
    int length = strlen(buffer);
    for(int i = 0; i < length; ++i) {
        if(buffer[i] >= 'A' && buffer[i] <= 'z') {
            // lock mutex to increase letterFreq and number
            pthread_mutex_lock(&running_mutex);
            statistics.lettersFreq[buffer[i] - 'A']++;
            statistics.nrLetters++;
            pthread_mutex_unlock(&running_mutex);
        }
    }
    // lock mutex to decrease nr. of threads
    pthread_mutex_lock(&running_mutex);
    --nrOfThreads;
    pthread_mutex_unlock(&running_mutex);
    return NULL;
}

void computingChildCode(char* inputPath, int maxNrOfThreads, int dataChunkSize) {
    char buffer[300][dataChunkSize];
    pthread_attr_t attr;
    int bytesRead, counter = 0;;
    int inputFile = open(inputPath, O_RDONLY);
    if(pthread_attr_init(&attr) != 0) {
        fprintf(stderr, "Error while creating detach attribute\n");
        exit(-11);
    }
    if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        fprintf(stderr, "Error while creating detach attribute\n");
        exit(-12);
    }
    if(inputFile < 0) {
        perror("Error while opening input file\n");
        exit(-4);
    }
    if(pthread_mutex_init(&running_mutex, NULL) != 0) {
        fprintf(stderr, "Error while initing mutex\n");
        exit(-10);
    }
    bytesRead = read(inputFile, buffer[counter], dataChunkSize - 1);
    while(bytesRead > 0) {
        buffer[counter][bytesRead] = '\0';
        // we still didn't reach the limit
        pthread_mutex_lock(&running_mutex);
        if(nrOfThreads < maxNrOfThreads) {
            pthread_mutex_unlock(&running_mutex);
            pthread_t thread;
            // create thread
            // stop child if error occurs
            if(pthread_create(&thread, &attr, threadCode, &buffer[counter]) != 0) {
                fprintf(stderr, "Error while creating thread! Terminating...\n");
                exit(-11);
            }
            // lock mutex to increase nr. of threads
            pthread_mutex_lock(&running_mutex);
            ++nrOfThreads;
            pthread_mutex_unlock(&running_mutex);
            bytesRead = read(inputFile, buffer[counter++], dataChunkSize - 1);

        } else {
            //unlock mutex
            pthread_mutex_unlock(&running_mutex);
            continue;
        }
    }
    // wait for all threads to finish
    while(1) {
        pthread_mutex_lock(&running_mutex);
        if(nrOfThreads == 0) {
            pthread_mutex_unlock(&running_mutex);
            break;
        }
        pthread_mutex_unlock(&running_mutex);

    }
    for(int i = 'A'; i <= 'z'; ++i) {
        printf("%c %d\n", (char) i,  statistics.lettersFreq[i - 'A']);
    }
    printf("%d\n", statistics.nrLetters);
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