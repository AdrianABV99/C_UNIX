#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int i, j, status;
    pid_t p_id, c_id;
    for(i = 1; i < 20; ++i) {
        int pid;
        if((pid = fork()) < 0 ) {
            printf("Error\n");
            exit(1);
        }
        if(pid == 0) {
            p_id = getppid();
            c_id = getpid();
            for(j = 0; j < 10; ++j) {
                printf("Child proccess %d. PID: %d; Parent PID: %d\n", j, c_id, p_id);
            }
            exit(i + 1);
        }
    }
    p_id = getppid();
    c_id = getpid();
    for(j = 0; j < 10; ++j) {
        printf("Parent proccess. PID: %d; Parent PID: %d\n", c_id, p_id);
    }

    while( (c_id = wait(&status)) != -1) {
        printf("Child terminated. PID: %d; Return: %d\n", c_id, status);
    }
    return 0;
}