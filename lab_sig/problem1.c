#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

int n, running = 1;

void handle_usr1(int nr) {
    ++n;
}

void handle_usr2(int nr) {
    alarm(5);
}

void handle_alarm(int nr) {
    ++n;
}

void handle_term(int nr) {
    running = 0;
    printf("Terminating gracefully!\n");
}

int main(void) {
    struct sigaction act;
    struct sigaction old;
    act.sa_handler = handle_usr1;

    if(sigaction(SIGUSR1, &act, &old) < 0) {
        perror("Error while dealing with signal SIGUSR1\n");
        exit(1);
    }
     act.sa_handler = handle_usr2;
    if(sigaction(SIGUSR2, &act, &old) < 0) {
        perror("Error while dealing with signal SIGUSR2\n");
        exit(1);
    }
     act.sa_handler = handle_alarm;
    if(sigaction(SIGALRM, &act, &old) < 0) {
        perror("Error while dealing with signal SIGALRM\n");
        exit(1);
    }
    act.sa_handler = handle_term;
    if(sigaction(SIGTERM, &act, &old) < 0) {
        perror("Error while dealing with signal SIGTERM\n");
        exit(1);
    }

    while(running) {
        printf("%d\n", n);
        usleep(500000);
    }
    return 0;
}