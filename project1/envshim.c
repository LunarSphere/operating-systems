#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define _GNU_SOURCE

// a shim is a piece of code that sits between a a program and the libraries it uses 
// shim intercepts calls and can monitor and change behavior. 


void log_event(char *event, char *name, char *value){
    
}


char *getenv(const char *name){
    //call real get env using dlsym
    char *(original_getenv)(void) = dlsym(RTLD_NEXT, "getenv");
    char *value = original_getenv(name); 

    // log helper

    //return result of getenv
    return value;
}

int setenv(const char *name, const char *value, int overwrite){
    //call real set env using dlsym
    int (original_setenv)(void) = dlsym(RTLD_NEXT, "setenv");
    int value = original_setenv(name, value, overwrite); 

    // log getenv
    log helper

    //return result of setenv
    return value;
}

int putenv(const char *string){
    //call real put env using dlsym
    int (original_putenv)(void) = dlsym(RTLD_NEXT, "putenv");
    int value = original_putenv(string); 

    // log helper

    //return result of putenv
    return value;
}

int unsetenv(const char *name){
    //call real unset env using dlsym
    int (original_unsetenv)(void) = dlsym(RTLD_NEXT, "unsetenv");
    int value = original_unsetenv(name); 

    // log helper

    //return result of unsetenv
    return value;
}