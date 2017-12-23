#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include "config.h"
#include "builtins.h"

void my_write(int fd, const void* buf, size_t nbyte) {
    int saved_errno = errno;
    size_t processed_chars = 0;
    while(processed_chars<nbyte) {
        size_t n = write(fd, buf+processed_chars, nbyte-processed_chars);
        if (n==-1) {
           if (errno==EINTR) {
              continue; 
            }
            exit(EXIT_STATUS_MY_WRITE);
        }
        processed_chars+=n;
    }
    errno = saved_errno;
}

void write_builtin_error(const void* program_name) { 
    my_write(STDERR_FILENO, "Builtin ", strlen("Builtin ")); 
    my_write(STDERR_FILENO, program_name, strlen(program_name));
    my_write(STDERR_FILENO,  " error.\n", strlen(" error.\n"));
}

int my_exit(char* argv[]) {
    exit(0);
	return 0;
}

int lcd(char* argv[])
{
    int n=0;
    while(argv[n]!=NULL) {
        n++;
    }
    if (n == 1) {
        if(chdir(getenv("HOME"))) {
            write_builtin_error(argv[0]);
            return BUILTIN_ERROR;
        }
        return 0;
    }
    if (n == 2) {
        if(chdir(argv[1])) {
            write_builtin_error(argv[0]);
            return BUILTIN_ERROR;
        }
        return 0;
    }
    write_builtin_error(argv[0]);
    return BUILTIN_ERROR;
}

int lkill(char* argv[]) {
    int n=0;
    while(argv[n]!=NULL) {
        n++;
    }
    if (n != 2 && n!= 3) {
        write_builtin_error(argv[0]);
        return BUILTIN_ERROR;
    }
    char* check;
    int signal_number = SIGTERM;
    if(n == 3) {
        signal_number = strtol(argv[1], &check, 10);
        if (*check != '\0') {
            write_builtin_error(argv[0]);
            return BUILTIN_ERROR;
        }
        signal_number *= -1;
    }
    int pid = strtol(argv[n-1], &check, 10); 
    if (*check != '\0') {
        write_builtin_error(argv[0]);
            return BUILTIN_ERROR;
    }
    if(kill(pid, signal_number)) {
        write_builtin_error(argv[0]);
            return BUILTIN_ERROR;
    }
    return 0;
}

int lls(char* argv[]) {
    int n=0;
    while(argv[n]!=NULL) {
        n++;
    }
    if (n!=1) {
        write_builtin_error(argv[0]);
            return BUILTIN_ERROR;
    }
    DIR* dir = opendir(".");
    if(dir == NULL) {
        write_builtin_error(argv[0]);
            return BUILTIN_ERROR;
    }
    struct dirent* dir_entry;
    while((dir_entry=readdir(dir)) != NULL) {
        if (dir_entry->d_name[0]=='.') {
            continue;
        }
        write(STDOUT_FILENO, dir_entry->d_name, strlen(dir_entry->d_name));    
        write(STDOUT_FILENO, "\n", 1);
    }
    return 0;
}

builtin_pair builtins_table[]={
	{"exit",	&my_exit},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,NULL}
};

