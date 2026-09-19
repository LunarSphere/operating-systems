// 9/19/2026 error handling version just in case i dont finish shims by twelve | kevius tribble

#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "common.h"
#include <sys/socket.h>
#include <netdb.h>
#include <err.h>
#include <sys/shm.h>
// create the pipe before the child process. 


// you need to be able to redirect inputs into a pipe. 
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1
#define SHM_SIZE
// #define FIFO_FILE "/tmp/myfifo" //not necessary




FILE* createlog(){
    FILE *fptr = fopen("clemshlog.txt", "w");
    if (fptr == NULL) {
        perror("Error opening clemshlog.txt");
        return NULL;
    }
    return fptr;
}

void execute_pipeline(Pipeline *pipeline, FILE *log, int pipeline_number){
    int pipe_fd[2]; // pipes create a read and write file descriptor
    pid_t pids[pipeline->command_count]; // need to support some number of commands
    int prev_fd = -1;
    for (size_t i = 0; i < pipeline->command_count; i++){
        // need to check and see if their is a next command
        if ( i < pipeline->command_count - 1 && pipe(pipe_fd) == -1){
            perror("pipe");
            return;
        }
        pids[i] = fork(); // creates a duplicate process | which is wiped by exec
        if (pids[i] == -1){
            perror("fork");
            return;
        }
        fprintf(log, "START    %zu:%d    %s", i, pipeline_number, pipeline->commands[i].argv[0]);
        fflush(log);
        
        if (pids[i] == 0){
            /* Child only reads from the pipe. */
            //needs error handling 
            if (prev_fd != -1){
                if (dup2(prev_fd, STDIN_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(prev_fd);
            }
            // be very sure that something comes next
            if (i < pipeline->command_count - 1){ 
                if (dup2(pipe_fd[PIPE_WRITE_END], STDOUT_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(pipe_fd[PIPE_WRITE_END]);
                close(pipe_fd[PIPE_READ_END]);
            }
            //FILES
            if (pipeline->commands[i].input.type == REDIRECT_FILE){
                // open a file descriptor and duplicate an input to the 
                int fd_in = open(pipeline->commands[i].input.path, O_RDONLY);
                if (fd_in == -1){
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_in, STDIN_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd_in);
            }
            if (pipeline->commands[i].output.type == REDIRECT_FILE){
                int fd_out = open(pipeline->commands[i].output.path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
                if (fd_out == -1){
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_out, STDOUT_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd_out);
            }
        // SOCKETS
        if (pipeline->commands[i].input.type == REDIRECT_TCP){
                int sockfd;
                struct addrinfo hints, *pfirstResult;
                memset(&hints, 0, sizeof(hints));
                hints.ai_socktype = SOCK_STREAM;
                // sockets are just file sescriptors and i can dup2 them like a file 
                //instead of open do the client setup
                char port_str[6];
                snprintf(port_str, sizeof(port_str), "%u", pipeline->commands[i].input.port); //string port must be a string 
                int error = getaddrinfo(pipeline->commands[i].input.host, port_str, &hints, &pfirstResult);
                if(error){
                    errx(1, "%s", gai_strerror(error));
                }
                struct addrinfo *curaddr;
                    for (curaddr = pfirstResult; curaddr; curaddr = curaddr->ai_next){
                    sockfd=socket(curaddr->ai_family, curaddr->ai_socktype, curaddr->ai_protocol);
                    if (sockfd==-1) continue;
                    if (connect(sockfd, curaddr->ai_addr, curaddr->ai_addrlen) == 0){
                        break;
                    }
                    close(sockfd);
                    sockfd = -1;
                    }
                    freeaddrinfo(pfirstResult);
                if (sockfd == -1){
                    err_n_die("connect failed!");
                }
                //final part once socket is setup and connected
                if (dup2(sockfd, STDIN_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(sockfd);
                
            }
        if (pipeline->commands[i].output.type == REDIRECT_TCP){
                int sockfd;
                struct addrinfo hints, *pfirstResult;
                memset(&hints, 0, sizeof(hints));
                hints.ai_socktype = SOCK_STREAM;
                // sockets are just file sescriptors and i can dup2 them like a file 
                //instead of open do the client setup
                char port_str[6];
                snprintf(port_str, sizeof(port_str), "%u", pipeline->commands[i].output.port);
                int error = getaddrinfo(pipeline->commands[i].output.host, port_str, &hints, &pfirstResult);
                if(error){
                    errx(1, "%s", gai_strerror(error));
                }
                struct addrinfo *curaddr;
                    for (curaddr = pfirstResult; curaddr; curaddr = curaddr->ai_next){
                    sockfd=socket(curaddr->ai_family, curaddr->ai_socktype, curaddr->ai_protocol);
                    if (sockfd==-1) continue;
                    if (connect(sockfd, curaddr->ai_addr, curaddr->ai_addrlen) == 0){
                        break;
                    }
                    close(sockfd);
                    sockfd = -1;
                    }
                    freeaddrinfo(pfirstResult);
                if (sockfd == -1){
                    err_n_die("connect failed!");
                }
                //final part once socket is setup and connected
                if (dup2(sockfd, STDOUT_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(sockfd);

            }
            execvp(pipeline->commands[i].argv[0], pipeline->commands[i].argv);
            perror("execvp");
            exit(EXIT_FAILURE); 
        }
        
        /* Parent only writes to the pipe. */
        if (prev_fd != -1){
            close(prev_fd);
        }
        if (i < pipeline->command_count - 1){
            close(pipe_fd[PIPE_WRITE_END]);
            prev_fd = pipe_fd[PIPE_READ_END];
        }
    }  
    for (size_t i = 0; i < pipeline->command_count; i++){
            int state; // to store info about state change
            waitpid(pids[i], &state, 0); 
            // fprintf(log, "EXIT    %s:%s    %s    NORMAL    %s", i, pipeline_number, pipeline.commands[i].argv, state);

    }
}

int main(int argc, char *argv[]){
    // beejs shim example
    // key_t key;
    // int shimid;
    // char *data;
    // /* make the key: */
    // if ((key = ftok("shmem2.c", 'R')) == -1) {
    //     perror("ftok");
    //     exit(1);
    // }

    // /* connect to (and possibly create) the segment: */
    // if ((shmid = shmget(key, SHM_SIZE, 0644 | IPC_CREAT)) == -1) {
    //     perror("shmget");
    //     exit(1);
    // }

    // /* attach to the segment to get a pointer to it: */
    // data = shmat(shmid, (void *)0, 0);
    // if (data == (char *)(-1)) {
    //     perror("shmat");
    //     exit(1);
    // }

    //interactive mode
    char input[9999];

    FILE *log = createlog();
    int pipeline_number = 0;
    if (argc == 1) {
        while(1){
            
            // create pointers for pipeline and error
            Pipeline pipeline;
            ParseError error;
            printf("> ");
            fflush(stdout); // we use this to immediately write to the buffer
            // error checking for fgets
            if (fgets(input, sizeof(input), stdin) == NULL) {
                // if read error from fgets
                printf("\n");
                break;
            }
            if (strlen(input) > 1024){
                fprintf(stderr, "inputs must be < 1024 chars");
                continue;
            }

            if (pipeline_parse(input, &pipeline, &error) != PARSE_SUCCESS){
                // handle parsing errors
                printf("Parse error at position %zu: %s\n", error.position, error.message);
                pipeline_free(&pipeline);
                continue; // next loop iteration
            }

            execute_pipeline(&pipeline, log, pipeline_number);
            pipeline_free(&pipeline);
            pipeline_number++;
        }
    }
    // batch mode
    if (argc == 2) {
        FILE *batch = fopen(argv[1], "r");
        if (batch == NULL) {
            perror("fopen");
            return EXIT_FAILURE;
        }
        int pipeline_number = 0;
        // create pointers for pipeline and error
        while (fgets(input, sizeof(input), batch) != NULL){
            Pipeline pipeline;
            ParseError error;
            if (pipeline_parse(input, &pipeline, &error) != PARSE_SUCCESS){
                // handle parsing errors
                printf("Parse error at position %zu: %s\n", error.position, error.message);
                pipeline_free(&pipeline);
                continue; // next loop iteration
            }
        execute_pipeline(&pipeline, log, pipeline_number);
        pipeline_free(&pipeline);
        pipeline_number++;
        }
        fclose(batch);
    }
    fclose(log);
    return 0;
}