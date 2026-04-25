#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <unistd.h>

int pipe_helper(char **tokens, char ***cmds);

void pipe_exec(char ***cmds, int cmd_count);

void single_exec(char **tokens);

void bg_checker();

void bg_exec(char **tokens);

char *expand_token(const char *token);


typedef struct {
    pid_t pid;
    int num;
    char cmd[128];
} BgProcess;

extern BgProcess bg_jobs[];
extern int bg_count;
extern pid_t server_pid;

#endif