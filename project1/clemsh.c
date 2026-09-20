#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
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
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
// create the pipe before the child process. 

// you need to be able to redirect inputs into a pipe. 
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1



// SHIM INTEGRATION: set paths
const char *log_path = "clemshlog.txt";
const char *shim_path = "./envshim.so";


// SHIM INTEGRATION: file for logging commands and env reads/writes
FILE* createlog(){
    // wipe old long file if it exists
    FILE *log = fopen(log_path, "w");
    if (log == NULL){
        perror("fopen");
        return NULL;
    }
    fclose(log);
    // open log file for appending
    log = fopen(log_path, "a");
    if (log == NULL){
        perror("fopen");
        return NULL;
    }
    return log;
}

//SHELL EXECUTION: execute a pipeline of commands
void execute_pipeline(Pipeline *pipeline, FILE *log, int pipeline_number){
    // a file descriptor is a interger that a c can use to access a file or socket.  
    int pipe_fd[2]; // pipes create a read and write file descriptor
    pid_t pids[pipeline->command_count]; // need to support some number of commands
    int prev_fd = -1; // -1 indicates file descriptor doesnt exist or isnt set. 
    struct timespec start_times[pipeline->command_count]; // array representing the start time of each command 
    for (size_t i = 0; i < pipeline->command_count; i++){
        // create pipe if we are not last command | avoids unecessary pipe
        if ( i < pipeline->command_count - 1 && pipe(pipe_fd) == -1){
            perror("pipe");
            close(prev_fd);
            return;
        }
        // SHIM INTEGRATION:  log the start of the command
        //START < TAB > PIPELINE : STAGE < TAB >< COMMAND >
        if (log != NULL){
            fprintf(log, "START\t%d:%zu\t", pipeline_number, i);
            // add arguments after command name 
            for (size_t j = 0; j < pipeline->commands[i].argc; j++){
                if (j == 0){
                    fprintf(log, "%s", pipeline->commands[i].argv[j]);
                }
                else{
                    fprintf(log, " %s", pipeline->commands[i].argv[j]);
                }
            }
            fprintf(log, "\n");
            fflush(log);
        }
        clock_gettime(CLOCK_MONOTONIC, &start_times[i]); // store start start time 
        pids[i] = fork(); // creates a duplicate process | which is wiped by exec
        if (pids[i] == -1){
            perror("fork");
            close(prev_fd);
            return;
        }

        if (pids[i] == 0){
            /* Child only reads from the pipe. */
            if (prev_fd != -1){
                // dup2 duplicattes a file descriptor and assigns it a user defined number 
                if (dup2(prev_fd, STDIN_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(prev_fd);
            }
            // connect write end of pipe to std out
            if (i < pipeline->command_count - 1){ 
                if (dup2(pipe_fd[PIPE_WRITE_END], STDOUT_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(pipe_fd[PIPE_WRITE_END]);
                close(pipe_fd[PIPE_READ_END]);
            }
            //SHIM INTEGRATION: set env vars
            char pipeline_str[50];
            char stage_str[50];
            snprintf(pipeline_str, sizeof(pipeline_str), "%d", pipeline_number);
            snprintf(stage_str, sizeof(stage_str), "%zu", i);
            setenv("CLEMSH_PIPELINE", pipeline_str, 1);
            setenv("CLEMSH_STAGE", stage_str, 1);
            if (log != NULL){
                setenv("CLEMSHLOG_PATH", log_path, 1); 
                setenv("LD_PRELOAD", shim_path, 1);
            }
            //FILES handle input redirection specifically ">" "<"
            if (pipeline->commands[i].input.type == REDIRECT_FILE){
                // open a file for reading
                int fd_in = open(pipeline->commands[i].input.path, O_RDONLY);
                if (fd_in == -1){
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                // if possible duplicate its contnet to STDIN
                if (dup2(fd_in, STDIN_FILENO) == -1){
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd_in);
            }
            // same thing but it writes the stdout in terminal to a file
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
        // SOCKETS Conceptually the same as above but you have to jump through more hoops to write to a socket
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
    // wait for processes to finish
    for (size_t i = 0; i < pipeline->command_count; i++){
        //SHIM INTEGRATION
        int state; // to store info about state change
        struct timespec end;
        struct rusage usage;
        wait4(pids[i], &state, 0, &usage); 
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (log != NULL){
            //use clock gettime to get real time
            double real_time = (end.tv_sec - start_times[i].tv_sec) + (end.tv_nsec - start_times[i].tv_nsec) / 1000000000.0;
            double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
            double system_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
            // two exit options
            // EXIT < TAB > PIPELINE : STAGE < TAB >< REALTIMEinMS >< TAB >< SYSTIMEinMS >< TAB >< USERTIMEinMS >< TAB > NORMAL < TAB >< TAB >< STATUS >
            // EXIT < TAB > PIPELINE : STAGE < TAB >< REALTIMEinMS >< TAB >< SYSTIMEinMS >< TAB >< USERTIMEinMS >< TAB > CRASH < TAB >< TAB >< STATUS >
            // WIFEXITED returns true if a child exited normally
            if (WIFEXITED(state)){
                fprintf(log, "EXIT\t%d:%zu\t%.6f\t%.6f\t%.6f\tNORMAL\t\t%d\n", pipeline_number, i, real_time * 1000, system_time * 1000, user_time * 1000, WEXITSTATUS(state));
            }
            // WIFSIGNALED returns true if a child process crashed
            else if (WIFSIGNALED(state)){
                fprintf(log, "EXIT\t%d:%zu\t%.6f\t%.6f\t%.6f\tCRASH\t\t%d\n", pipeline_number, i, real_time * 1000, system_time * 1000, user_time * 1000, WTERMSIG(state));
            }
            fflush(log);
        }
    }
}


// this c thing is pretty cool
int main(int argc, char *argv[]){
    //interactive mode
    char input[9999];
    char *log_env = getenv("CLEMSHLOG");
    FILE *log = NULL;
    if (log_env != NULL){
        log = createlog();
    }
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
                fprintf(stderr, "Error reading input\n");
                return EXIT_FAILURE;
            }
            if (strlen(input) > 1025){
                fprintf(stderr, "inputs must be < 1024 chars");
                continue;
            }
            if (pipeline_parse(input, &pipeline, &error) != PARSE_SUCCESS){
                // handle parsing errors
                fprintf(stderr, "Parse error at position %zu: %s\n", error.position, error.message);
                pipeline_free(&pipeline);
                continue; // next loop iteration
            }

            execute_pipeline(&pipeline, log, pipeline_number);
            pipeline_free(&pipeline);
            pipeline_number++;
            
        }
        if (log != NULL){
            fclose(log);
        }
        return EXIT_SUCCESS;
    }
    // batch mode
    else if (argc == 2) {
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
            if (strlen(input) > 1024){
                fprintf(stderr, "inputs must be < 1024 chars");
                continue;
            }
            if (pipeline_parse(input, &pipeline, &error) != PARSE_SUCCESS){
                // handle parsing errors
                fprintf(stderr, "Parse error at position %zu: %s\n", error.position, error.message);
                pipeline_free(&pipeline);
                continue; // next loop iteration
            }
        execute_pipeline(&pipeline, log, pipeline_number);
        pipeline_free(&pipeline);
        pipeline_number++;
        }
        fclose(batch);
        if (log != NULL){
            fclose(log);
        }
        return EXIT_SUCCESS;
    }
    else{
        fprintf(stderr, "usage: %s <pipeline to parse>\n", argv[0]);
        if (log != NULL){
            fclose(log);
        }
        return EXIT_FAILURE;
    }
    return 0;
}