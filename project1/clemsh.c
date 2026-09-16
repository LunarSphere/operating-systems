#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include "parser.h"
#include <unistd.h>
// create the pipe before the child process. 

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
                pipeline_print(&pipeline);
                pipeline_free(&pipeline);
                continue; // next loop iteration
            }
            // we can run commands
            for (size_t i = 0; i < pipeline.command_count; i++){
                execvp(pipeline.commands[i].argv[0], pipeline.commands[i].argv);
            }
            pipeline_print(&pipeline);
            pipeline_free(&pipeline);
        }

    }
    //batch mode
    // if argc >= 1 {

    // }
    return 0; 
}