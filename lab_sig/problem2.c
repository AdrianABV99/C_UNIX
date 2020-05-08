#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int running = 1, running_c = 1;
pid_t C_ID;
void handle_alarm(int nr) {
    kill(C_ID, SIGUSR1);
}

void handle_usr1(int nr) {
    running_c = 0;
    printf("Received SIGUSR1. Ending child\n");
}


void handle_child_end(int nr) {
    running = 0;
    printf("Received SIGCHLD. Ending parent\n");
}

int main(void) {
    struct sigaction act, old;
    memset(&act, 0, sizeof(act));

    if((C_ID = fork()) < 0) {
        perror("Error while making child\n");
        exit(1);
    }
    if(C_ID == 0) {
        act.sa_handler = SIG_IGN;
        if(sigaction(SIGTERM, &act, &old) < 0) {
            perror("Error while handling SIGTERM\n");
            exit(1);
        }
        act.sa_handler = handle_usr1;
        if(sigaction(SIGUSR1, &act, &old) < 0) {
            perror("Error while handling SIGUSR1\n");
            exit(1);
        }
        while(running_c) {
            printf("B\n");
        }
        exit(0);
    }
    act.sa_handler = handle_alarm;
    if(sigaction(SIGALRM, &act, &old) < 0) {
        perror("Error while handling SIGALRM\n");
        exit(1);
    }

    act.sa_handler = handle_child_end;
    if(sigaction(SIGCHLD, &act, &old) < 0) {
        perror("Error while handling SIGCHLD\n");
        exit(1);
    }

    alarm(7);
    while(running) {
        printf("A\n");
    }


    return 0;
}