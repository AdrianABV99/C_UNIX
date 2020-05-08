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

#define BYTES 4096
#define MAX_CHILDREN 10000
#define EXEC 128
int numbers_less_found = 0, letter_count = 0;
int compiled_good = 0, compiled_bad = 0;
int handeled_parent_sigs = 0;
int children_pids[MAX_CHILDREN], counter = 0;
int outdesc;
int errCode = 10;
int running = 1;
char name[EXEC];
int limit;
char searched;

void writeChildInfoOutput(char* message1, int value1, char* message2, int value2) {
    char line[BYTES];
    snprintf(line, BYTES, "%s %d\t\t%s %d\n", message1, value1, message2, value2);

    int bytesWritten = write(outdesc, line, strlen(line));
    if(bytesWritten == 0) {
        perror("Error while writing to output in PARENT\n");
        exit(++errCode);
    }
}

void handle_alarm(int nr) {
    printf("PARENT with PID:%d received signal: SIGALRM\n", getpid());
    printf("PARENT with PID:%d - found letters %d - found numbers %d\n", getpid(), letter_count, numbers_less_found);
    alarm(3);
}

void handle_usr1(int nr) {
    printf("PARENT with PID:%d received signal: SIGUSR1\n", getpid());
    ++numbers_less_found;
}

void handle_usr2(int nr) {
    printf("PARENT with PID:%d received signal: SIGUSR2\n", getpid());
    ++letter_count;
}

void handle_term(int nr) {
    printf("PARENT with PID:%d received signal: SIGTERM\n", getpid());
    running = 0;
    for(int i = 0; i < counter; ++i) {
        kill(children_pids[i], SIGTERM);
    }
}

void child_handle_term(int nr) {
    running = 0;
}

void readBinaryFile(char* fileName) {
    int inputdesc = open(fileName, O_RDONLY);
    int i, len, j;
    int bytesRead;
    char text[5];
    char buff[BYTES];
    if(inputdesc < 0) {
        perror("Error while opening file in CHILD\n");
        exit(++errCode);
    }
    
    bytesRead = read(inputdesc, buff, BYTES);
    while(bytesRead > 0 && running) {
        len = strlen(buff);
        for(i = 0; i < len && running; i += 5) {
            unsigned int head = (unsigned char) buff[i];
            if(head == 0X55) {
                uint32_t val = (unsigned char) buff[i + 4] << 24 |   (unsigned char) buff[i + 3] << 16 |  (unsigned char) buff[i + 2] << 8 |  (unsigned char) buff[i + 1];
                if(val < limit) {
                    if(kill(getppid(), SIGUSR1) < 0) {
                        fprintf(stderr, "Error sending SIGUSR1 from CHILD: %d\n", getpid());
                        exit(-1);
                    }
                    /*
                     use usleep if we want to not get signals collision
                     for this, we need to wait for each child to finish
                     before making another one
                     usleep(100);
                     */
                }
            } else if(head == 0X98) {
                j = 0;
                while(j + 1 < 5) {
                    text[j] = buff[i + j + 1];
                    j++;
                }
                text[j] = '\0';
                if(strchr(text, searched) != NULL) {
                    //found letter;
                    if(kill(getppid(), SIGUSR2) < 0) {
                        fprintf(stderr, "Error sending SIGUSR2 from CHILD: %d\n", getpid());
                        exit(-1);
                    }
                    /*
                     use usleep if we want to not get signals collision
                     for this, we need to wait for each child to finish
                     before making another one
                     usleep(100);
                     */
                }
                
            }
        }

        bytesRead = read(inputdesc, buff, BYTES);
    }
    // printf("CHILD - %d; Numbers: %d; Letters %d; Parent: %d----------------------\n", getpid(), nr, let, getppid());
    close(inputdesc);
}


void getFileName(char* fileName) {
    int dotPos;
    int i;
    for(i = strlen(fileName) - 1; i >= 0 && running; --i) {
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
void handleChildSignals() {
    struct sigaction act, old;
    memset(&act, 0, sizeof(act));
    act.sa_handler = child_handle_term;
    if(sigaction(SIGTERM, &act, &old) < 0) {
        perror("Error while handling SIGALRM\n");
        exit(++errCode);
    }
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
}



void handleParentSignals() {
    struct sigaction act, old;
    memset(&act, 0, sizeof(act));

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
    handeled_parent_sigs = 1;
}


void childProcess(char* path, char* fileName) {
    char* extension = getFileExtension(fileName);
    if(strcmp(extension, "c") == 0) {
        char executable[EXEC];
        getFileName(path);
        snprintf(executable, EXEC, "%s.exe", name);
        // printf("CHILD FOR C -------------------%d \n", getpid());
        execlp("gcc", "gcc", "-Wall", "-o", executable, path, NULL);
        // if we reach this code, exec failed
        perror("Exec call error!");
        exit(++errCode);
    } else if(strcmp(extension, "dat") == 0) {
        readBinaryFile(path);
    }
    exit(++errCode);

}


void writeToOutput(char* path, char* type, int size) {
    char line[BYTES];
    snprintf(line, BYTES, "%s %s %d\n", path, type, size);

    int bytesWritten = write(outdesc, line, strlen(line));
    if(bytesWritten == 0) {
        perror("Error while writing to output in PARENT\n");
        exit(++errCode);
    }
}


void parseDirectory(char* path) {
    struct stat fileStat; // used for regular files & directories
    struct stat fileStatL; // used for symbolic links
    struct dirent *directory;
    int pid;
    DIR *dir = opendir(path);
    if(dir == NULL) {
        return;
    }

    /*  until we still have directories
        process files in the directories
    */
    while(running && (directory = readdir(dir)) != NULL) {
        char newPath[BYTES];
        sprintf(newPath, "%s/%s", path, directory -> d_name);
        // check for .. and . to avoid infinite loop
        if(strcmp(directory -> d_name, "..") != 0 && strcmp(directory -> d_name, ".") != 0 && stat(newPath, &fileStat) >= 0) {
            if(S_ISDIR(fileStat.st_mode)) {
                writeToOutput(newPath, "DIR", fileStat.st_size);
                parseDirectory(newPath);
            } else {
                if(lstat(newPath, &fileStatL) >= 0) {
                    if(S_ISLNK(fileStatL.st_mode)) {
                        writeToOutput(newPath, "LNK", fileStatL.st_size);
                    } else if(S_ISREG(fileStat.st_mode)) {
                        if(counter == MAX_CHILDREN) {
                            // got too many children.
                            // kill process
                            kill(getpid(), SIGTERM);
                        }
                        // regular file
                        if((pid = fork()) < 0) {
                            perror("Error while creating child \n");
                            exit(++errCode);
                        }
                        if(pid == 0) {
                            handleChildSignals();
                            // printf("Entering Child :%d\n", getpid());
                            childProcess(newPath, directory->d_name);
                        }
                        children_pids[counter++] = pid;
                        // can use sleep to test sigalrm or sigterm
                        // sleep(1);
                        writeToOutput(newPath, "REG", fileStat.st_size);
                    }
                }
            }
        }
    }

    closedir(dir);
}



int main(int argc, char** argv) {
    int status;
    pid_t c_id;
    if(argc != 5 || strlen(argv[2]) >= 2) {
        fprintf(stderr, "Invalid nr. of arguments or wrong argument. Usage: ./exec <directory> <letter> <number> <output_file_path>\n");
        exit(++errCode);
    }
    searched = argv[2][0];
    limit = atoi(argv[3]);

    outdesc = open(argv[4], O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH);
    if(outdesc < 0) {
        perror("Error while opening output file in PARENT \n");
        exit(++errCode);
    }
    handleParentSignals();
    alarm(3);
    parseDirectory(argv[1]);
    do {
        c_id = wait(&status);
        if(c_id == -1 && errno == EINTR) {
            // signal called
            c_id = 0;
            continue;
        }

        // check for -1 for last call of wait (-1 for no children left)
        if(WIFEXITED(status) && c_id != -1) {
            if(WEXITSTATUS(status) == 0) {
                ++compiled_good;
            } else if(WEXITSTATUS(status) == 1) {
                ++compiled_bad;
            }
            // this prints, sometimes, the first or second child twice
            // strangely, if output is redirected or grep CHILD is used
            // it doesn't appear twice
            printf("CHILD with pid %d died normally with return value: %d\n", c_id, WEXITSTATUS(status));
        }
    } while(c_id != -1);

    writeChildInfoOutput("Total letters: ", letter_count, "Total numbers: ", numbers_less_found);
    writeChildInfoOutput("Compiled ok: ", compiled_good, "Compiled not ok: ", compiled_bad);

    close(outdesc);
    return 0;
}