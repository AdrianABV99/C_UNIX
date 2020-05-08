#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char** argv) {
    int i, j, status;
    pid_t pid, c_pid;
    if(argc < 2) {
        printf("Error! Usage: ./name <file> n1 n2 ...\n");
        exit(1);
    }

    for(i = 2; i < argc; ++i) {
        if((pid = fork()) == -1) {
            printf("Erorr while creating child\n");
            exit(1);
        }

        if(pid == 0) {
            c_pid = getpid();
            for(j = 1; j <= atoi(argv[i]); j++) {
                printf("%d -> %d\n", c_pid, j);
            }

            exit(i + 1);
        }
    }


    while((c_pid = wait(&status)) != -1) {
        printf("Child with PID: %d ended with code %d\n", c_pid, status);
    }

    if((pid = fork()) < 0) {
        printf("Erorr while creating child\n");
        exit(1);
    }

    if(pid == 0) {
        execv("script.sh", argv);
        exit(0);
    }

    while((c_pid = wait(&status)) != -1) {
        printf("Child with PID: %d ended with code %d\n", c_pid, status);
    }
    return 0;
}