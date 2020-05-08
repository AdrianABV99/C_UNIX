#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define BUFF 4095
#define EXEC 128
char name[EXEC];
int numbers_less_found = 0, letter_count = 0;
int compiled_good = 0, compiled_bad = 0;
int handeled_parent_sigs = 0;
int outdesc;
int errCode = 10;
int running = 1;
char name[EXEC];
int limit;
char searched = 'T';



void writeChildInfoOutput(char* message, int value) {
    char line[1000];
    snprintf(line, 1000, "%s %d\n", message, value);

    printf("%s\n", line);
}

void child_handle_term(int nr) {
    running = 0;
}
void handle_alarm(int nr) {
    printf("PARENT: received signal: SIGALRM\n");
    printf("PARENT with PID %d - found letters %d - found numbers %d\n", getppid(), letter_count, numbers_less_found);
    alarm(3);
}

void handle_usr1(int nr) {
    printf("PARENT: received signal: SIGUSR1\n");
    ++numbers_less_found;
}

void handle_usr2(int nr) {
    printf("PARENT: received signal: SIGUSR2\n");
    ++letter_count;
}

void handle_term(int nr) {
    printf("PARENT: received signal: SIGTERM\n");
    running = 0;
    writeChildInfoOutput("Total letters: ", letter_count);
    writeChildInfoOutput("Total numbers: ", numbers_less_found);
    writeChildInfoOutput("Compiled ok: ", compiled_good);
    writeChildInfoOutput("Compiled not ok: ", compiled_bad);
    exit(0);
}

void handleChildSignals() {
    struct sigaction act, old;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    if(sigaction(SIGUSR1, &act, &old) < 0) {
        perror("Error while handling SIGUSR1\n");
        exit(++errCode);
    }
    act.sa_handler = SIG_IGN;
    if(sigaction(SIGUSR2, &act, &old) < 0) {
        perror("Error while handling SIGUSR2\n");
        exit(++errCode);
    }
    act.sa_handler = SIG_IGN;
    if(sigaction(SIGALRM, &act, &old) < 0) {
        perror("Error while handling SIGALRM\n");
        exit(++errCode);
    }
    act.sa_handler = child_handle_term;
    if(sigaction(SIGTERM, &act, &old) < 0) {
        perror("Error while handling SIGALRM\n");
        exit(++errCode);
    }
}



void handleParentSignals() {
    struct sigaction act, old;
    memset(&act, 0x00, sizeof(struct sigaction));

    act.sa_handler = handle_alarm;
    if(sigaction(SIGALRM, &act, &old) < 0) {
        perror("Error while handling SIGALRM \n");
        exit(++errCode);
    }
    act.sa_handler = handle_usr1;
    if(sigaction(SIGUSR1, &act, &old) < 0) {
        perror("Error while handling SIGUSR1 \n");
        exit(++errCode);
    }
    act.sa_handler = handle_usr2;
    if(sigaction(SIGUSR2, &act, &old) < 0) {
        perror("Error while handling SIGUSR2 \n");
        exit(++errCode);
    }
    act.sa_handler = handle_term;
    if(sigaction(SIGTERM, &act, &old) < 0) {
        perror("Error while handling SIGTERM \n");
        exit(++errCode);
    }
}

void getFileName(char* fileName) {
    int dotPos;
    int i;
    for(i = strlen(fileName) - 1; i >= 0; --i) {
        if(fileName[i] == '.') {
            dotPos = i;
            break;
        }
    }
    memcpy(name, fileName, dotPos);
}

char* getFileExtension(char* fileName) {
    char* dotPos = strchr(fileName, '.');
    return dotPos + 1;
}

void childProcess(char* path, char* fileName) {
    char* extension = getFileExtension(fileName);
    if(strcmp(extension, "c") == 0) {
        char executable[EXEC];
        getFileName(path);
        printf("%s\n", name);
        snprintf(executable, EXEC, "%s.exe", name);
        // printf("CHILD FOR C -------------------%d \n", getpid());
        execlp("gcc", "gcc", "-Wall", "-o", executable, path, NULL);
        // if we reach this code, exec failed
        perror("Exec call error!");
    }

}



void readBinaryFile(char* fileName) {
    int inputdesc = open(fileName, O_RDONLY);
    int i, len, j;
    int bytesRead;
    char text[6];
    char buff[BUFF];
    int nr = 0, let = 0;
    if(inputdesc < 0) {
        perror("Error while opening file in CHILD\n");
        exit(3);
    }
    
    bytesRead = read(inputdesc, buff, BUFF);
    while(bytesRead > 0) {
        len = strlen(buff);
        for(i = 0; i < len; i += 5) {
            unsigned int head = (unsigned char) buff[i];
            if(head == 0X55) {
                unsigned int val = (unsigned char) buff[i + 4] << 24 |   (unsigned char) buff[i + 3] << 16 |  (unsigned char) buff[i + 2] << 8 |  (unsigned char) buff[i + 1];
                if(val < 12462) {
                    if(kill(getppid(), SIGUSR1) < 0) {
                        perror("Error sending signal SIGUSR1 to parent\n");
                        exit(4);
                    }
                    ++nr;
                }
            } else if(head == 0X98) {
                j = 0;
                while(j < 5) {
                    text[j] = buff[i + j];
                    j++;
                }
                text[j] = '\0';
                if(strchr(text, searched) != NULL) {
                    if(kill(getppid(), SIGUSR2) < 0) {
                        perror("Error sending signal SIGUSR2 to parent\n");
                        exit(4);
                    }
                    ++let;
                }
            }
        }

        bytesRead = read(inputdesc, buff, BUFF);
    }
    close(inputdesc);
}
// void defaultSignals() {
//     struct sigaction action_ignore;
//     memset(&action_ignore, 0x00, sizeof(struct sigaction));
//     action_ignore.sa_handler = SIG_IGN;

//     if (sigaction(SIGUSR1, &action_ignore, NULL) < 0)
//     {
//         perror("sigaction SIGUSR1 ignore");
//         exit(-1);	     
//     }
//     if (sigaction(SIGUSR2, &action_ignore, NULL) < 0)
//     {
//         perror("sigaction SIGUSR1 ignore");
//         exit(-1);	     
//     }
//     if (sigaction(SIGALRM, &action_ignore, NULL) < 0)
//     {
//         perror("sigaction SIGUSR1 ignore");
//         exit(-1);	     
//     }
// }

int main() {
    int status = 0;
    pid_t c_id = 0;
    int pid = 0;
    // childProcess("testdir1/prog_c_ok.c", "prog_c_not_ok.c");
    handleParentSignals();
    if((pid = fork()) < 0) {
        perror("Error while creating child \n");
        exit(++errCode);
    }
    if(pid != 0) {
        do {
            c_id = wait(&status);
            if(c_id == -1 && errno == EINTR) {
                c_id = 0;
                continue;
            }
            if(WIFEXITED(status) && c_id != -1) {
                if(WEXITSTATUS(status) == 0) {
                    ++compiled_good;
                } else if(WEXITSTATUS(status) == 1) {
                    ++compiled_bad;
                }
                printf("CHILD with pid %d died normally with return status: %d\n", c_id, WEXITSTATUS(status));
            }
        } while(c_id != -1);
    } else {
        int cnt = 0;
        handleChildSignals();
        for(int i = 0; i < 100; ++i) {
            if(kill(getppid(), SIGUSR2) != 0) {
                perror("Error!");
                exit(-1);
            }
            usleep(100);
            ++cnt;
        }
        printf("%d CNT---\n", cnt);
        exit(0);
    }
    printf("Here %d --- %d\n", c_id, errno);
    //  writeChildInfoOutput("Total letters: ", letter_count);
    // writeChildInfoOutput("Total numbers: ", numbers_less_found);
    // writeChildInfoOutput("Compiled ok: ", compiled_good);
    // writeChildInfoOutput("Compiled not ok: ", compiled_bad);

    return 0;
    // readBinaryFile("vtuQLQbKFVeJPkA.dat");
}