#ifndef _CONFIG_H_
#define _CONFIG_H_

#define MAX_LINE_LENGTH 2048
#define EXEC_FAILURE 127
#define BUFFER_SIZE 2*(MAX_LINE_LENGTH+1)
#define BG_COUNTER_MAX MAX_LINE_LENGTH/2

#define SYNTAX_ERROR_STR "Syntax error.\n"
#define EACCES_STR ": permission denied\n"
#define ENOENT_STR ": no such file or directory\n"
#define EXEC_ERROR_STR ": exec error\n"
#define FORK_ERROR_STR "fork() FAILED. Exiting...\n"
#define PIPE_ERROR_STR "pipe() FAILED. Exiting...\n"
#define FSTAT_ERROR_STR "fstat() FAILED. Exiting...\n"
#define SETSID_ERROR_STR "setsid() FAILED. Exiting...\n"
#define PROMPT_STR "$ "

#define EXIT_STATUS_MY_WRITE 40
#define EXIT_STATUS_SETSID 41
#define EXIT_STATUS_WAITPID 42
#define EXIT_STATUS_READ 43
#define EXIT_STATUS_OPEN 44
#define EXTI_STATUS_OPEN 45
#define EXIT_STATUS_FORK 46
#define EXIT_STATUS_FSTAT 47
#define EXIT_STATUS_PIPE 48

#endif /* !_CONFIG_H_ */
