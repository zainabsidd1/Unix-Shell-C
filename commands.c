#include <string.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "commands.h"
#include "io_helpers.h"
#include "builtins.h"


BgProcess bg_jobs[128]; // array storing bg processes
int bg_count = 0; // num of bg processess currently running
int job_count = 0; // used for numbering
pid_t server_pid = -1;


void bg_exec(char **tokens){
    pid_t pid = fork();
    if (pid==0){ //child - bg process
        // reset CTRL + C to kill bg process

        signal(SIGINT, SIG_DFL);
        int dev_null = open("/dev/null", O_RDONLY);
        dup2(dev_null, STDIN_FILENO); // bg process reads from empty input
        close(dev_null);

        bn_ptr builtin_fn = check_builtin(tokens[0]);
        if (builtin_fn != NULL){
            builtin_fn(tokens);
            exit(0);
        } else {
                // not a builtin, look inside /bin
                char buf[512];
                snprintf(buf, sizeof(buf), "/bin/%s", tokens[0]);
                execv(buf, tokens);

                // if /bin didn't work, look inside /usr/bin
                snprintf(buf, sizeof(buf), "/usr/bin/%s", tokens[0]);
                execv(buf, tokens);

                // neither path worked
                display_error("ERROR: Unknown command: ", tokens[0]);
                exit(1);
        }
    } else if (pid>0){//parent
        // store job info
        job_count++;
        bg_jobs[bg_count].pid = pid;
        bg_jobs[bg_count].num = job_count;

        // build command string
        char full_cmd[216] = "";
        for(int i=0; tokens[i] != NULL; i++){
            if(i != 0) strncat(full_cmd, " ", 125 - strlen(full_cmd));
            strncat(full_cmd, tokens[i], 125 - strlen(full_cmd));
        }
        strncpy(bg_jobs[bg_count].cmd, full_cmd, 127);
        bg_count++;

        char buf[216];
        snprintf(buf, sizeof(buf), "[%d] %d\n", job_count, pid);
        write(STDOUT_FILENO, buf, strlen(buf));

    }
}


void bg_checker(){
    for (int i=0; i<bg_count; i++){
        BgProcess job = bg_jobs[i];
        pid_t pid = waitpid(job.pid,NULL, WNOHANG);
        if (pid > 0){// process done
            char buf[216];
            snprintf(buf, sizeof(buf), "[%d]+ Done %s\n", job.num, job.cmd);
            write(STDOUT_FILENO, buf, strlen(buf));
            // remove job
            bg_jobs[i] = bg_jobs[bg_count-1];
            bg_count --;
            i--;
            if(bg_count == 0){
                job_count = 0;
            }
        }
    }
}

int pipe_helper(char **tokens, char ***cmds){
    int cmd_count = 0;
    cmds[cmd_count++] = tokens;
    for(int i=0; tokens[i]!=NULL; i++){
        if (strcmp(tokens[i], "|") == 0){
            tokens[i] = NULL;
            cmds[cmd_count++] = &tokens[i+1];
        }
    }
    return cmd_count;
}

void pipe_exec(char ***cmds, int cmd_count){
    if (cmd_count <= 1) return;
    // create pipes
    int all_pipes[cmd_count-1][2];
    for (int i=0; i<cmd_count-1; i++){
        if(pipe(all_pipes[i]) == -1){
            perror("pipe");
            exit(1);
        };
    }
    for (int j=0; j<cmd_count; j++){ // fork for each command
        pid_t pid = fork();
        if (pid<0){
            perror("fork");
            exit(1);
        }
        if(pid ==0){// child
            // reset CTRL+C to exit process
            signal(SIGINT, SIG_DFL);
                if(j!=0){ 
                    // if not first, must read from prev pip
                    // replace current pipes stdin with prev pipes read end
                    dup2(all_pipes[j-1][0],STDIN_FILENO);
                }
                if (j!=cmd_count-1){
                    // if not last, must write to current pipe
                    // replace curr childs stdout end with write end of curr pipe
                    dup2(all_pipes[j][1], STDOUT_FILENO);
                }

                // close all pipes
                for(int k=0; k<cmd_count - 1;k++){
                    close(all_pipes[k][0]);
                    close(all_pipes[k][1]);
                }
                // run cmd or builtin
                bn_ptr builtin_fn = check_builtin(cmds[j][0]);
                if (builtin_fn!= NULL){
                    ssize_t error = builtin_fn(cmds[j]);
                    // returns -2 when specific error was printed
                    if (error == -1){
                        display_error("ERROR: Builtin failed: ", cmds[j][0]);
                    } 
                    exit(0);
                } else {
                    // not a builtin, look inside /bin
                    char buf[512];
                    snprintf(buf, sizeof(buf), "/bin/%s", cmds[j][0]);
                    execv(buf, cmds[j]);

                    // if /bin didn't work, look inside /usr/bin
                    snprintf(buf, sizeof(buf), "/usr/bin/%s", cmds[j][0]);
                    execv(buf, cmds[j]);

                    // neither path worked
                    display_error("ERROR: Unknown command: ", cmds[j][0]);
                    exit(1);
                }
        } 
    }
    for(int k=0; k<cmd_count-1;k++){
        close(all_pipes[k][0]);
        close(all_pipes[k][1]);
    }

    for (int i=0; i<cmd_count; i++){
        wait(NULL);
    }
}

void single_exec(char **tokens){

    pid_t pid = fork();
    if (pid == 0){//child
        // Reset SIGINT in the child so foreground commands can be interrupted.
        // Without this, Ctrl+C would be ignored and commands like sleep would not stop.
        signal(SIGINT, SIG_DFL);

        char buf[512];
        strcpy(buf, "/bin/");
        strcat(buf, tokens[0]);
        execv(buf, tokens);

        // if /bin didn't work, look inside /usr/bin
        strcpy(buf, "/usr/bin/");
        strcat(buf, tokens[0]);
        execv(buf,tokens);

        // neither path worked
        display_error("ERROR: Unknown command: ", tokens[0]);
        exit(1);

    } else if (pid>0){//parent
        wait(NULL);
    }

}