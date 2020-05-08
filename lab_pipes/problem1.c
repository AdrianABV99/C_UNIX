#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>   
#include <unistd.h>
#include <string.h>
 #include <errno.h>
#define BUFF_SIZE 4096

int main(void) {
    int c_pid = 0;
    int pfd[2], pfd2[2], pfd3[2];

    if(pipe(pfd) < 0) {
        perror("Pipe creation error\n");
        exit(1);
    }
    
    if(pipe(pfd2) < 0) {
        perror("Pipe creation error\n");
        exit(1);
    }

    if(pipe(pfd3) < 0) {
        perror("Pipe creation error\n");
        exit(1);
    }

    if((c_pid = fork()) < 0) {
        perror("Error while creating child process\n");
        exit(1);
    }
    if(c_pid == 0) {
        close(pfd[1]);
        close(pfd2[0]);
        close(pfd3[0]);
        close(pfd3[1]);
        char buff[BUFF_SIZE], letters[BUFF_SIZE];
        int cnt = 0;

        while(read(pfd[0], buff, BUFF_SIZE) > 0) {
            for(int i = 0; i < strlen(buff); ++i) {
                if(buff[i] >= 'a' && buff[i] <= 'z') {
                    letters[cnt++] = buff[i];
                }
            }
            letters[cnt] = '\0';
            write(pfd2[1], letters, cnt);
            cnt = 0;
        }
        close(pfd[0]);
        close(pfd2[1]);
        exit(0);
    }
    if((c_pid = fork()) < 0) {
        perror("Error while creating child process 2\n");
        exit(1);
    }
    if(c_pid == 0) {
        close(pfd[1]);
        close(pfd[0]);
        close(pfd2[1]);
        close(pfd3[0]);

        char buff[BUFF_SIZE], line[BUFF_SIZE];
        int letters[26];
        int distLetters = 0;
        int filedesc = open("statistic.txt", O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH);
        if(filedesc < 0) {
            perror("Error while opening file\n");
            exit(1);
        }

        while(read(pfd2[0], buff, BUFF_SIZE) > 0) {
            for(int i = 0; i < strlen(buff); ++i) {
                letters[buff[i] - 'a']++;
            }
        }
        close(pfd2[0]);
        
        for(int i = 0; i < 26; ++i) {
            sprintf(line, "%c frequency: %d\n", 'a' + i, letters[i]);
            write(filedesc, line, strlen(line));
            if(letters[i] > 0) {
                distLetters++;
            }
        }
        write(pfd3[1], &distLetters, sizeof(int));
        close(pfd3[1]);

        exit(0);
    }
    close(pfd[0]);
    close(pfd2[0]);
    close(pfd2[1]);
    close(pfd3[1]);

    int filedesc = open("data.txt", O_RDONLY);
    int distLetters = 0;
    char buff[BUFF_SIZE];
    if(filedesc < 0) {
        perror("Error while opening file\n");
        exit(1);
    }

    while(read(filedesc, buff, BUFF_SIZE) > 0) {
        write(pfd[1], buff, BUFF_SIZE);
    }

    close(pfd[1]);
    read(pfd3[0], &distLetters, sizeof(int));
    close(pfd3[0]);

    printf("%d\n", distLetters);
    return 0;
}