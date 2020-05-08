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
#define RES_SIZE 100
int filedesc1[2]; // data-control child -> parent
int filedesc2[2]; // data-control child <- child
int filedesc3[2]; // parent -> data-control child

void writeToOutput(int descriptor, char* fileName, char* hash) {
    char line[BUFF];
    snprintf(line, BUFF, "%s = %s\n", fileName, hash);
    if(write(descriptor, line, strlen(line)) < 0) {
        perror("Error while writing to output in data-control\n");
        exit(1);
    }
}

void writeToOutputWithStatus(int descriptor, char* fileName, char* hash, char* status) {
    char line[BUFF];
    snprintf(line, BUFF, "%s = %s %s\n", fileName, hash, status);
    if(write(descriptor, line, strlen(line)) < 0) {
        perror("Error while writing to output in data-control\n");
        exit(1);
    }
}

void dataControlChildCode(char* pathOutput) {
    char buffer[BUFF], line[2][BUFF];
    int outputFile = open(pathOutput, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH);
    int nrFiles = 0, counter = 0;
    FILE* inputStream;
    if(outputFile < 0 ){
        perror("Error while opening file in data-control \n");
        exit(-1);
    }
    inputStream = fdopen(filedesc2[0], "r");
    while(fscanf(inputStream, "%s", buffer) != EOF) {
        if(strstr(buffer, "/") != NULL && counter == 0) {
            // file not found
            ++nrFiles; //nr files tested increases
            writeToOutput(outputFile, buffer, "");
            continue;
        }
        if(!counter) {
            // line and buffer are of same size
            // no need for strncpy
            
            // save hash (comes first)
            strcpy(line[1], buffer);
            ++counter;
        } else if(counter) {
            // save path
            strcpy(line[0], buffer);
            counter = 0;
            ++nrFiles;
            writeToOutput(outputFile, line[0], line[1]);
        }
        
    }
    close(filedesc2[0]);
    if(write(filedesc1[1], &nrFiles, sizeof(nrFiles)) < 0) {
        perror("Error while writing to parent pipie from data-control\n");
        exit(-2);
    }

    close(filedesc1[1]);
    fclose(inputStream);
    // nothing to lose if we close it
    close(outputFile);
}

void md5ChildCode(char* line) {
    // child for calling md5
    close(filedesc1[0]); // close reading pipe for first pipe
    // open dev Null to redirect stderr of exec

    // **** inspired by the discussion from Q&A *****

    int devNull = open("/dev/null", O_WRONLY);
    dup2(filedesc2[1], 1); // redirect stdout to pipe2
    dup2(devNull, 2);
    execlp("md5sum", "md5sum", line, NULL);
    perror("Error while executing exec\n");
    exit(-1);
}

void writeToStatistics(int nrFiles) {
    int statisticFile;
    char result[RES_SIZE];
    statisticFile = open("statistics.txt", O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH);
    // create pretty line for displaying the result
    snprintf(result, RES_SIZE, "Nr. of computed md5sum for files: %d\n", nrFiles);
    if(write(statisticFile, result, strlen(result)) < 0) {
        perror("Error while writing to statistics \n");
        return;
    }
    close(statisticFile);
}

int fileExists(char* path) {
    struct stat stat_file;
    if(stat(path, &stat_file) == 0) {
        return 1;
    } else {
        return 0;
    }
}

void createFile(char* pathInput, char* pathOutput) {
    int pid;
    if(pipe(filedesc1) < 0 || pipe(filedesc2) < 0) {
        perror("Error while creating pipe\n");
        exit(-100);
    }
    if((pid = fork()) < 0) {
        perror("Error while creating child\n");
        exit(-120);
    }
    if(pid == 0) {
        // data-control child code
        close(filedesc1[0]); // close first pipe read
        close(filedesc2[1]); // close second pipe write

        dataControlChildCode(pathOutput);
        
        exit(0);
    }
    //parent code
    close(filedesc1[1]); // close first pipe write
    close(filedesc2[0]); // close reading end of second pipe

    int inputFile = open(pathInput, O_RDONLY);
    if(inputFile < 0) {
        perror("Error while opening file in parent \n");
        return;
    }
    FILE* inputStream;
    int nrFiles;
    char line[BUFF];
    inputStream = fdopen(inputFile, "r");
    while(fscanf(inputStream, "%s", line) != EOF) {
        if((pid = fork()) < 0) {
            perror("Error while creating child in parent\n");
            return;
        }
        if(pid == 0) {
            md5ChildCode(line);
        }
        // check if file exists.
        // if it doesn't, exec failed
        // so write to pipe2 the path
        // to print with =
        if(!fileExists(line)) {
            line[strlen(line) + 1] = '\0';
            line[strlen(line)] = ' ';
            if(write(filedesc2[1], line, strlen(line)) < 0) {
                perror("Error while writing to data-child no file\n");
                exit(-100);
            }
        }
    }
    close(filedesc2[1]); // close write end of second pipe
    if(read(filedesc1[0], &nrFiles, sizeof(int)) < 0) {
        perror("Error while reading from pipe in parent\n");
        return;
    }
    close(filedesc1[0]); // close read end of first pipe
    writeToStatistics(nrFiles);

    fclose(inputStream);
    close(inputFile);
}

void dataControlChildCodeCheck(char* pathOutput) {
    char buffer[BUFF], bufferHash[BUFF], line[2][BUFF];
    int outputFile = open(pathOutput, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IROTH | S_IWOTH | S_IXOTH);
    int nrFiles = 0, counter = 0;
    FILE* inputStream1, *inputStream2;
    if(outputFile < 0 ){
        perror("Error while opening file in data-control \n");
        exit(-1);
    }
    inputStream1 = fdopen(filedesc2[0], "r");
    inputStream2 = fdopen(filedesc3[0], "r");

    while(fscanf(inputStream2, "%s", bufferHash) != EOF) {
        counter = 0;
        ++nrFiles;
        // use fgets because md5 sends a line
        if(fgets(buffer, BUFF, inputStream1) != NULL) {
            // md5 returned a valid hash
            ++nrFiles;
            while(strstr(bufferHash, "/") != NULL) {
                // These are files that do no exist
                writeToOutputWithStatus(outputFile, bufferHash, "", "NOT OK");
                if(fscanf(inputStream2, "%s", bufferHash) == EOF) {
                    break;
                }
                ++nrFiles;
            }
            // we should have the same files now with hashes.

            // remove 1 from nrFiles because we have counted twice
            --nrFiles;
            // remove newline from buffer
            buffer[strlen(buffer) - 1] = '\0';
                        printf("%s ---------> %s\n", buffer, bufferHash);
            // get hash and file
            char* p = strtok(buffer, " ");
            while(p != NULL) {
                strcpy(line[counter++], p);
                if(counter == 2) {
                    // compare them
                    if(strcmp(bufferHash, line[0]) == 0) {
                        // OK
                        writeToOutputWithStatus(outputFile, line[1], line[0], "OK");
                    } else {
                        // NOT OK
                        writeToOutputWithStatus(outputFile, line[1], line[0], "NOT OK");
                    }
                    counter = 0;
                }
                p = strtok(NULL, " ");
            }
        }
    }
    close(filedesc2[0]);
    if(write(filedesc1[1], &nrFiles, sizeof(nrFiles)) < 0) {
        perror("Error while writing to parent pipie from data-control\n");
        exit(-2);
    }

    close(filedesc1[1]);
    fclose(inputStream1);
    close(outputFile);
}

void waitForChild(int pid) {
    pid_t c_id;
    int status;
    while((c_id = waitpid(pid, &status, 0)) > 0) {
        // printf("CHILD with PID - %d ended normally with status code: %d\n", c_id, WEXITSTATUS(status));
    }
}

void checkFile(char* pathInput, char* pathOutput) {
    int pid;
    if(pipe(filedesc1) < 0 || pipe(filedesc2) < 0 || pipe(filedesc3) < 0) {
        perror("Error while creating pipe\n");
        exit(-101);
    }
    if((pid = fork()) < 0) {
        perror("Error while creating child\n");
        exit(-121);
    }
    if(pid == 0) {
        // data-control child code
        close(filedesc1[0]); // close first pipe read
        close(filedesc2[1]); // close second pipe write
        close(filedesc3[1]); // close third pipe write
        dataControlChildCodeCheck(pathOutput);
        
        exit(0);
    }
    //parent code
    close(filedesc1[1]); // close first pipe write
    close(filedesc2[0]); // close reading end of second pipe
    close(filedesc3[0]); // close reading end of third pipe

    int inputFile = open(pathInput, O_RDONLY);
    if(inputFile < 0) {
        perror("Error while opening file in parent \n");
        return;
    }
    FILE* inputStream;
    int nrFiles = 0, counter = 0;
    char line[BUFF];
    inputStream = fdopen(inputFile, "r");
    /* read with fgets because there can be more
       information on a line
       e.g. path = hash
            path =
    */
    while(fgets(line, BUFF, inputStream)) {
        char lineInfo[3][BUFF];
        counter = 0;
        line[strlen(line) - 1] = '\0'; // remove newline
        char* p = strtok(line, " "); // if they are separated by one space
        while(p != NULL) {
            strcpy(lineInfo[counter++], p);
            p = strtok(NULL, " ");
        }
        if((pid = fork()) < 0) {
            perror("Error while creating child in parent\n");
            return;
        }
        if(pid == 0) {
            close(filedesc3[1]); // close reading end of third pipe
            md5ChildCode(lineInfo[0]);
        }
        // if counter is just 2, we found only path and =
        // no need to check if file exists anymore
        if(counter == 2) {
            // no hash
            // add space
            int length = strlen(lineInfo[0]);
            lineInfo[0][length + 1] = '\0';
            lineInfo[0][length] = ' ';
            // send path
            if(write(filedesc3[1], lineInfo[0], length + 1) < 0) {
                perror("Error while sending noHash to child\n");
                return;
            }
        } else {
            // send hash from input file for comparisson
            // add space
            int length = strlen(lineInfo[counter - 1]);
            lineInfo[counter - 1][length + 1] = '\0';
            lineInfo[counter - 1][length] = ' ';
            if(write(filedesc3[1], lineInfo[counter - 1], length + 1) < 0) {
                perror("Error while sending hash to child\n");
                return;
            }
        }
        
    }
    close(filedesc3[1]); // close write end on third pipe
    close(filedesc2[1]); // close write end on second pipe
    if(read(filedesc1[0], &nrFiles, sizeof(nrFiles)) < 0) {
        perror("Error while reading from pipe in parent\n");
        return;
    }
    close(filedesc1[0]); // close read end

    writeToStatistics(nrFiles);

    fclose(inputStream);
    close(inputFile);
}

int main(int argc, char** argv) {
    
    if(argc != 4 || strlen(argv[1]) > 1) {
        perror("Invalid number of arguments! Usage: ./executable [v or c] <input_text_file> <output_text_file>");
        exit(1);
    }

    if(argv[1][0] == 'c') {
        // create file
        createFile(argv[2], argv[3]);
    } else if(argv[1][0] == 'v') {
        // check file
        checkFile(argv[2], argv[3]);
    } else {
        perror("Invalid argument for create or check!\n");
        exit(-1000);
    }

    waitForChild(-1);

    return 0;
}