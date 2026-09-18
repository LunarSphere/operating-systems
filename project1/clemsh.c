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
// create the pipe before the child process. 


// you need to be able to redirect inputs into a pipe. 
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1
#define FIFO_FILE "/tmp/myfifo"

int main(int argc, char *argv[]){
    //interactive mode
    char input[9999];
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
            if (pipeline_parse(input, &pipeline, &error) != PARSE_SUCCESS){
                // handle parsing errors
                printf("Parse error at position %zu: %s\n", error.position, error.message);
                pipeline_free(&pipeline);
                continue; // next loop iteration
            }
            int pipe_fd[2]; // pipes create a read and write file descriptor
            pid_t pids[pipeline.command_count]; // need to support some number of commands
            int prev_fd = -1;
            for (size_t i = 0; i < pipeline.command_count; i++){
                // need to check and see if their is a next command
                if ( i < pipeline.command_count - 1 && pipe(pipe_fd) == -1){
                    perror("pipe");
                    return EXIT_FAILURE;
                }
                pids[i] = fork(); // creates a duplicate process | which is wiped by exec
                
                if (pids[i] == 0){
                    /* Child only reads from the pipe. */
                    //needs error handling 
                    if (prev_fd != -1){
                        dup2(prev_fd, STDIN_FILENO);
                        close(prev_fd);
                    }
                    // be very sure that something comes next
                    if (i < pipeline.command_count - 1){ 
                        dup2(pipe_fd[PIPE_WRITE_END], STDOUT_FILENO);
                        close(pipe_fd[PIPE_WRITE_END]);
                        close(pipe_fd[PIPE_READ_END]);
                    }
                    if (pipeline.commands[i].input.type == REDIRECT_FILE){
                        // open a file descriptor and duplicate an input to the 
                        int fd_in = open(pipeline.commands[i].input.path, O_RDONLY);
                        if (fd_in == -1){
                            perror("open");
                            exit(EXIT_FAILURE);
                        }
                        dup2(fd_in, STDIN_FILENO);
                        close(fd_in);
                    }
                    if (pipeline.commands[i].output.type == REDIRECT_FILE){
                        int fd_out = open(pipeline.commands[i].output.path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
                        if (fd_out == -1){
                            perror("open");
                            exit(EXIT_FAILURE);
                        }
                        dup2(fd_out, STDOUT_FILENO);
                        close(fd_out);
                    }
                    if (pipeline.commands[i].input.type == REDIRECT_TCP){
                        int sockfd;
                        // int sendbytes;
                        struct sockaddr_in servaddr;
                        // sockets are just file sescriptors and i can dup2 them like a file 
                        //instead of open do the client setup
                        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                            err_n_die("Error while creating the socket!");

                        // bzero(&servaddr, sizeof(servaddr));
                        memset(&servaddr, 0, sizeof(servaddr));
                        servaddr.sin_family = AF_INET;          // use IPv4
                        servaddr.sin_port = htons(pipeline.commands[i].output.port); /* the port my server is listening on */

                        if (inet_pton(AF_INET, pipeline.commands[i].output.path, &servaddr.sin_addr) <= 0)
                            err_n_die("inet_pton error for %s ", argv[1]);

                        if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0)
                            err_n_die("connect failed!");
                        //final part once socket is setup and connected
                        dup2(sockfd, STDIN_FILENO);
                        close(sockfd);
                    }
                    if (pipeline.commands[i].output.type == REDIRECT_TCP){
                        int sockfd;
                        // int sendbytes;
                        struct sockaddr_in servaddr;
                        // sockets are just file sescriptors and i can dup2 them like a file 
                        //instead of open do the client setup
                        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                            err_n_die("Error while creating the socket!");

                        // bzero(&servaddr, sizeof(servaddr));
                        memset(&servaddr, 0, sizeof(servaddr));
                        servaddr.sin_family = AF_INET;          // use IPv4
                        servaddr.sin_port = htons(pipeline.commands[i].output.port); /* the port my server is listening on */

                        if (inet_pton(AF_INET, pipeline.commands[i].output.path, &servaddr.sin_addr) <= 0)
                            err_n_die("inet_pton error for %s ", argv[1]);

                        if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) < 0)
                            err_n_die("connect failed!");
                        //final part once socket is setup and connected
                        dup2(sockfd, STDOUT_FILENO);
                        close(sockfd);
                    }

                    execvp(pipeline.commands[i].argv[0], pipeline.commands[i].argv);
                    return EXIT_SUCCESS;
                }
                /* Parent only writes to the pipe. */
                //this is wrong needs re evaluated
                if (prev_fd != -1){
                    close(prev_fd);
                }
                if (i < pipeline.command_count - 1){
                    close(pipe_fd[PIPE_WRITE_END]);
                    prev_fd = pipe_fd[PIPE_READ_END];
                }
            }  
            for (size_t i = 0; i < pipeline.command_count; i++){
                    wait(NULL);
            }
            pipeline_free(&pipeline);
        }
    }
    // // batch mode
    // if argc >= 1 {


    // }
    // else{
    //     printf("usage: %s <pipeline to parse>\n", argv[0]);
    //     return EXIT_FAILURE;
    // }
    return 0; 
}