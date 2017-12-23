#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "config.h"
#include "builtins.h"
#include "siparse.h"
#include "utils.h"

// Foreground variables
int fg_pids[MAX_LINE_LENGTH/2];
volatile int fg_children_alive = 0;
int fg_spawned_processes;

// Background variables
volatile int bg_counter = 0;
int finished_bg_processes[BG_COUNTER_MAX][2];
bool background_process;

struct sigaction sa_chld;
sigset_t mask_only_chld;
sigset_t mask_empty;

void prepare_process() {
    signal(SIGINT, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    if(background_process) {
        if (setsid() == -1) {
            my_write(STDERR_FILENO, SETSID_ERROR_STR, strlen(SETSID_ERROR_STR));
            exit(EXIT_STATUS_SETSID);
        }
    }
}

void print_terminated_bg_children_info() {
    static char buffer[100];
    sigprocmask(SIG_BLOCK, &mask_only_chld, NULL); 
    for(int i=0; i<bg_counter; i++) {
        int status = finished_bg_processes[i][1];
        if (WIFEXITED(status)) {
            int n = sprintf(buffer, "Background process(%d) terminated. (exited with status %d)\n", finished_bg_processes[i][0], status);
            my_write(STDOUT_FILENO, buffer, n);
        }
        else if (WIFSIGNALED(status)) { 
            int n = sprintf(buffer, "Background process(%d) terminated. (killed by signal %d)\n", finished_bg_processes[i][0], status);
            my_write(STDOUT_FILENO, buffer, n);
        }
    }
    bg_counter = 0;
    sigprocmask(SIG_UNBLOCK, &mask_only_chld, NULL); 
}

bool is_foreground_process(int pid) {
    for(int i=0; i<fg_spawned_processes; i++) {
        if(fg_pids[i]==pid) {
            return true;
        }
    }
    return false;
}

void sigchld_handler(int sig_nb) {
    int saved_errno = errno;
    int status;
    int pid;
    while((pid=waitpid(-1, &status, WNOHANG))) {
        if (pid == -1) {
            if (errno == ECHILD) { // no child to check
                break;
            }
            exit(EXIT_STATUS_WAITPID);
        }
        if (is_foreground_process(pid)) {
           fg_children_alive--;
        }
        else if (bg_counter<BG_COUNTER_MAX) {
            finished_bg_processes[bg_counter][0] = pid;
            finished_bg_processes[bg_counter][1] = status;
            bg_counter++;
        }
    }
    errno = saved_errno;
}

void prepare_sigs() {
    sigemptyset(&mask_only_chld);
    sigaddset(&mask_only_chld, SIGCHLD);
    sigemptyset(&mask_empty);
    sa_chld.sa_handler = sigchld_handler;
    sigfillset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, NULL);
    signal(SIGINT, SIG_IGN);
}

void handle_errno(command* com, char* str) {
    my_write(STDERR_FILENO, str, strlen(str));
    if (errno == ENOENT) {
        my_write(STDERR_FILENO, ENOENT_STR, strlen(ENOENT_STR));
    }
    else if (errno == EACCES) {
        my_write(STDERR_FILENO, EACCES_STR, strlen(EACCES_STR));
    }
    else {
        my_write(STDERR_FILENO, EXEC_ERROR_STR, strlen(EXEC_ERROR_STR));
    }
}

char* load_next_line() {
	static char buffer[BUFFER_SIZE];
	static int beg = 0; //index of command beginning
	static int pos = 0; // index of current position
	static int bytes_in_buffer=0;
	while (true)  {
		for (; pos<bytes_in_buffer; pos++) {
			if (pos-beg>MAX_LINE_LENGTH) { // line too long
			my_write(STDERR_FILENO, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR)); 
			bool found_end = false;	
				while(found_end==false) {
					for(; pos<bytes_in_buffer; pos++) {
						if (buffer[pos] == '\n') {
							buffer[pos] = 0;
							pos++;
							beg = pos;
							found_end = true;
							break;	
						}
					}
					if (found_end==false) {
						bytes_in_buffer = read(STDIN_FILENO, buffer, MAX_LINE_LENGTH);
						if (bytes_in_buffer == 0) {
							return NULL;
						}
						pos = 0;
					}	
				}
			}
			if (buffer[pos] == '\n') {
				buffer[pos]=0;
				pos++;
				int old_beg = beg;
				beg = pos;
				return (buffer+old_beg);
			 }
		}
		if (beg > MAX_LINE_LENGTH) {
			memcpy(buffer, buffer+beg, (bytes_in_buffer-beg));
			bytes_in_buffer = (bytes_in_buffer-beg); 
			pos = pos - beg;
			beg=0;
		} 
		int read_val = read(STDIN_FILENO, buffer+pos, BUFFER_SIZE - pos - 1);
        if (read_val == -1) {
            if (errno == EINTR) { // no data read because of signal
                continue;
            }
            exit(EXIT_STATUS_READ);
        }
		if (read_val == 0) {
			if (beg==pos) {
				return NULL; // nothing more to execute
			}
			buffer[pos] = '\n';		
			
		}
		bytes_in_buffer += read_val;
	} 
}

void exec_command(command* com) {
    prepare_process();
    if (com->redirs) {
        for(redirection** r= com->redirs; *r; r++) {
            if(IS_RIN((*r)->flags)) {
                int fd = open((*r)->filename, O_RDONLY);
                if (fd == -1) { 
                    handle_errno(com, (*r)->filename);
                    exit(EXIT_STATUS_OPEN);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            else if (IS_ROUT((*r)->flags)) {
                int fd = open((*r)->filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
                if (fd == -1) {
                    handle_errno(com, (*r)->filename);
                    exit(EXIT_STATUS_OPEN);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else if(IS_RAPPEND((*r)->flags)) {
                int fd = open((*r)->filename, O_WRONLY | O_APPEND | O_CREAT, 0666);
                if (fd == -1) {
                    handle_errno(com, (*r)->filename);
                    exit(EXIT_STATUS_OPEN);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }
    }
    int execret = execvp(com->argv[0], com->argv);
    if (execret == -1) { // redundant check, there MUST have been an error if we've reached this line
        handle_errno(com, com->argv[0]);
        exit(EXEC_FAILURE);
    }
}

void single_command(command* com) {
        if (com==NULL || com->argv == NULL || com->argv[0] == NULL) {
            return;
        }
        for(builtin_pair* i = builtins_table; i->name != NULL; i++) {
            if (strcmp(com->argv[0], i->name) == 0) {
                i->fun(com->argv);
                return;
            }
        }
        int chpid = fork();
        if (chpid<0) {
            my_write(STDERR_FILENO, FORK_ERROR_STR, strlen(FORK_ERROR_STR));
            exit(EXIT_STATUS_FORK);
        }
        else if (chpid == 0) {
            exec_command(com);
        }
        else {
            if (background_process==false) {
                fg_pids[fg_children_alive] = chpid;
                fg_children_alive++; // =1
                fg_spawned_processes = fg_children_alive;
           }
        }
}

void exec_pipeline(pipeline* pl) {
    int cmd_counter = 0;
    for(command** cmd=*pl; *cmd; cmd++, cmd_counter++);
    if (cmd_counter == 1) {
        single_command(**pl);
        return;
    }
    for(command** cmd=*pl; *cmd; cmd++) {
        if ((*cmd)->argv == NULL || (*cmd)->argv[0] == NULL) {
            my_write(STDERR_FILENO, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR));
            return;
        }
    }
    int pipefd[2][2]; 
    pipefd[0][0] = pipefd[0][1] = pipefd[1][0] = pipefd[1][1] = -1; // failed closes are acceptable
    command** cmd=*pl;
    for(int i=0; i<cmd_counter; i++, cmd++) {
        if (i>1) {
            close(pipefd[i%2][0]);
            close(pipefd[i%2][1]);
        }
        if ((pipe(pipefd[i%2])) == -1) {
          my_write(STDERR_FILENO, PIPE_ERROR_STR, strlen(PIPE_ERROR_STR));
          exit(EXIT_STATUS_PIPE); 
        }
        int chpid = fork();
        if (chpid<0) {
            my_write(STDERR_FILENO, FORK_ERROR_STR, strlen(FORK_ERROR_STR));
            exit(EXIT_STATUS_FORK);
        }
        else if (chpid == 0) {
            if (i != 0) {
                dup2(pipefd[(i+1)%2][0], STDIN_FILENO);
            }
            if (i != cmd_counter-1) {
                dup2(pipefd[i%2][1], STDOUT_FILENO);
            }
            close(pipefd[i%2][0]);
            close(pipefd[i%2][1]);
            close(pipefd[(i+1)%2][0]);
            close(pipefd[(i+1)%2][1]);
            exec_command(*cmd);
        }
        else {
            if(background_process==false) {
                fg_pids[i] = chpid;
            }
        }
    }
    close(pipefd[0][0]);
    close(pipefd[0][1]);
    close(pipefd[1][0]);
    close(pipefd[1][1]);
    if (background_process==false) {
        fg_children_alive = cmd_counter;
        fg_spawned_processes = fg_children_alive;
    }
}

void exec_line(line* ln) {
    background_process = ln->flags&LINBACKGROUND;
    for(pipeline* pl = ln->pipelines; *pl; pl++) {
        sigprocmask(SIG_BLOCK, &mask_only_chld, NULL); // both sigprocmask inside loop in case some bg_processes need to be handled (unzombied)
        exec_pipeline(pl);
        while(fg_children_alive>0) {
            sigsuspend(&mask_empty);
        }
        fg_spawned_processes=0;
        sigprocmask(SIG_UNBLOCK, &mask_only_chld, NULL);
    }
}

int main(int argc, char *argv[]) {
    prepare_sigs();
    struct stat finfo;
	bool printPrompt = false;
	if (fstat(STDIN_FILENO, &finfo) != 0) {
		my_write(STDERR_FILENO, FSTAT_ERROR_STR, strlen(FSTAT_ERROR_STR));
		exit(EXIT_STATUS_FSTAT);
	}
	if(S_ISCHR(finfo.st_mode)) {
		printPrompt = true;
	}
    while(true) {
        if (printPrompt) {
            print_terminated_bg_children_info();
            my_write(STDOUT_FILENO, PROMPT_STR, strlen(PROMPT_STR));
        }
        char* next_line = load_next_line();
        if (next_line == NULL) {
            break;	
        }
        line* ln = parseline(next_line);
        if (ln == NULL) {
            my_write(STDERR_FILENO, SYNTAX_ERROR_STR, strlen(SYNTAX_ERROR_STR));
            continue;
        }
        exec_line(ln);
    }
	return 0;
}
