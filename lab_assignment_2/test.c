#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define BUFF 4096
void waitForChild(int pid) {
    pid_t c_id;
    int status;
    while((c_id = waitpid(pid, &status, 0)) > 0) {
        printf("CHILD with PID - %d ended normally with status code: %d\n", c_id, WEXITSTATUS(status));
    }
}

int main() {
    int pid;
    char* pathInput = "input1.txt";
    char line[BUFF];
    int inputFile = open(pathInput, O_RDONLY);
    FILE* inputStream = fdopen(inputFile, "r");
    while(fgets(line, BUFF, inputStream) != NULL) {
        line[strlen(line) - 1] = '\0'; // remove newline
        if((pid = fork()) < 0) {
            perror("Error while creating child in parent\n");
            return 0;
        }
        if(pid == 0) {
            execlp("md5sum", "md5sum", line, NULL);
            perror("Error while executing exec\n");
            exit(-1);
        }
            waitForChild(-1);
    }

    fclose(inputStream);
    return 0;
}