#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "commands.h"


static int process_line(char *line);
char *expand_token(const char *token);
static void set_var_checked(const char *name, const char *val);
void signal_helper(int sig);
static void exit_server(void);
char *expand_token_server(const char *token, int *overflow);


// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc, 
         __attribute__((unused)) char* argv[]) {
    char *prompt = "mysh$ ";
    //buffer to hold user input
   char input_buf[MAX_STR_LEN + 1];
   input_buf[MAX_STR_LEN] = '\0';

   signal(SIGINT, signal_helper); 
   // CTRL+C - runs signal_helper

   while (1) {
        // check for finished bg jobs
        bg_checker();
       // Prompt and input tokenization
       // Display the prompt via the display_message function.
       display_message(prompt);
      
       int ret = get_input(input_buf);
       if (ret == 0){  // EOF/input closed
        exit_server();
        break; }
       if (ret==-1){continue;}

       char *line_ptr;
       char *line = strtok_r(input_buf, "\n",&line_ptr); // split by newlines

       while(line != NULL){

        if(line[0] != '\0'){
            int flag = process_line(line);
            if (!flag) {
                exit_server();
                return 0;
            }
        }
        line = strtok_r(NULL, "\n", &line_ptr); // get the next line
       }

   }
   exit_server();
  
   return 0;
}

static int process_line(char *line){
    // returns 0 on exit shell, 1 otherwise
    
    char *tokens[MAX_STR_LEN]={NULL}; // array of char pointers
    size_t token_count = tokenize_input(line, tokens);
    if(token_count == 0){
        return 1;
    }
    // exit
    if((strcmp(tokens[0],"exit")==0)){
        if(token_count<=2){
            return 0;
        } else{
            display_error("ERROR: Builtin failed: ", tokens[0]);
            return 1;
        }
    }

    // background process detection
    int process_flag = 0;
    if (strcmp((tokens[token_count - 1]), "&")==0){
        process_flag = 1;
        tokens[token_count - 1] = NULL; // replace & with NULL
        token_count = token_count - 1;
    }

    // assignment statement / variable storage
    if(token_count==1 && (strchr(tokens[0], '=') != NULL)){

        if(tokens[0][0] == '='){ // edge case
            return 1; 
        } 
        if(tokens[0][0] != '$'){ 
            // expanded text should execute builtins 
            // but not become an assignment statement
            char *equal = strchr(tokens[0],'='); // points to char after =

            if(*(equal+1) == '\0'){ 
                *equal = '\0';
                set_var_checked(tokens[0], "");
                return 1;
            }

            *equal = '\0';
            char *name = tokens[0]; // points to start of left substring
            char *val = equal+1; // points to char after =

            char *val_expanded = expand_token(val); // handle expansion inside variable assigment
            if(val_expanded){ 
                set_var_checked(name, val_expanded);
                free(val_expanded); 
            } else {
                set_var_checked(name, val);
            }
            return 1;
        }
    }

    // expansion
    // e.g: echo $name hi
    char *expanded_token[MAX_STR_LEN] = {NULL}; 
    int free_vars[MAX_STR_LEN] = {0}; // free_var[i] = 1 : malloc'd, must be freed.

    for(size_t i=0; tokens[i] != NULL; i++){
        char *expanded = expand_token(tokens[i]);
        // try expanding every token
        if (expanded){
            expanded_token[i] = expanded; 
            free_vars[i] = 1; 
        } else{ 
            expanded_token[i] = tokens[i];
            free_vars[i] = 0;
        }
    }

    expanded_token[token_count] = NULL;

    // detect exit command after expansion
    if(expanded_token[0]!= NULL && strcmp(expanded_token[0], "exit")==0){
        for(size_t i = 0; expanded_token[i] != NULL; i++){
            if (free_vars[i]){
                free(expanded_token[i]);
            }
        }
        return 0;
    }

    if (process_flag == 1){
        bg_exec(expanded_token);
        return 1;
    }

    // detect pipe
    char **cmds[MAX_STR_LEN]; // pointers to token arrays
    int num_cmds = pipe_helper(expanded_token, cmds);
    if (num_cmds >1){
        pipe_exec(cmds,num_cmds);
    } else {

        // determine if the command was a built in
        // Builtins that must run in the shell process (cd, start-server, etc.)
        // also inherit signal_helper, so Ctrl+C won't kill them either.
        bn_ptr builtin_fn = check_builtin(expanded_token[0]);
        if (builtin_fn != NULL){ 
            if (strcmp(expanded_token[0], "cd")==0 || strcmp(expanded_token[0], "start-server")==0 
            || strcmp(expanded_token[0], "close-server")==0 || strcmp(expanded_token[0], "start-client")==0 ){
                ssize_t error = builtin_fn(expanded_token);
                // returns -2 when specific error was printed
                if (error == -1){
                    display_error("ERROR: Builtin failed: ", expanded_token[0]);
                } 
            } else{
                pid_t pid = fork();
                if (pid<0){
                    perror("fork");
                    return 1;
                }
                if (pid ==0){ //child 
                    // All other builtins are forked into a child process, which resets
                    // SIGINT to SIG_DFL before running. Ctrl+C will terminate child processes normally.
                    // parent shell ignores CTRL+C
                    signal(SIGINT, SIG_DFL);
                    ssize_t error = builtin_fn(expanded_token);
                    if (error == -1){
                        display_error("ERROR: Builtin failed: ", expanded_token[0]);
                    } 
                    exit(0);
                } else if (pid>0) {//parent
                    waitpid(pid,NULL,0);
                }
            }
        } else{ 
            // not a built in, look inside /bin or /usr/bin
            single_exec(expanded_token);
        }
    }

    // free any heap allocated expansions created by expand_token
    for (size_t i =0; expanded_token[i] != NULL; i++){
        if (free_vars[i]){
            free(expanded_token[i]);
        }
    }
    return 1;
}


char *expand_token(const char *token){

    if(strchr(token,'$') == NULL) return NULL; 
    char *buffer = malloc(130);
    // allows truncation to 128 chars after expansion

    if(!buffer){return NULL;}
    size_t buffer_idx = 0; // tracks current write position

    for(size_t j=0; token[j]!= '\0' && buffer_idx < 128;){
        // walk through token char by char

        if (token[j] == '$'){
            j++; // skip $
            if (token[j] == '\0') continue; 

            char name[128]; 
            size_t n = 0; 
            if (token[j-1]=='$' && (
                token[j]==' ' ||
                token[j]=='\0' ||
                token[j] == '\t' ||
                token[j] == '\n' 
            )){
                continue; //empty var name
            }

            //collects var name
            while(token[j]!='$' && 
                token[j]!=' ' &&
                token[j]!='\0' &&
                token[j] != '\t' &&
                token[j] != '\n' &&
                n<127){
                    name[n++] = token[j++]; 
            }

            name[n] = '\0'; 

            const char *val = get_var(name); // look up var
            if (val == NULL) val="";

            for (size_t k = 0; val[k] != '\0' && buffer_idx<128; k++){
                buffer[buffer_idx++] = val[k]; // copy var value into buffer
            }
        } else { 
            // not $, copy into result
            buffer[buffer_idx++] = token[j++];
            continue;
        }
    }
    buffer[buffer_idx] = '\0'; 
    return buffer; // returns heap-allocated memory
}

static void set_var_checked(const char *name, const char *val){
    // calls set_var, returns 1 on success, 0 on failiure
    if(!set_var(name,val)){
        display_error("ERROR: Memory allocation error", "");
    }
}

void signal_helper(int sig){
    // CTRL+C should not kill the shell
    (void)sig;
    write(STDOUT_FILENO, "\n", 1); 
}

static void exit_server(void){
    if (server_pid !=-1){ // servers process id
        kill(server_pid, SIGTERM); // terminate that pid
        server_pid = -1;
   }
}


char *expand_token_server(const char *token, int *overflow){
    //during variable expansion, the expand_token_server function tracks 
    //whether expansion causes overflow. If it does, the message is rejected before being sent
    *overflow = 0;
    if(strchr(token,'$') == NULL){
        *overflow = (strlen(token)>128);
        return NULL;
    } 
    char *buffer = malloc(300);
    // allows truncation to 128 chars after expansion

    if(!buffer){return NULL;}
    size_t buffer_idx = 0; // tracks current write position

    for(size_t j=0; token[j]!= '\0' && buffer_idx < 299;){
        // walk through token char by char
        if (token[j] == '$'){
            j++; // skip $
            if (token[j] == '\0') continue; 

            char name[128]; 
            size_t n = 0; 
            if (token[j-1]=='$' && (
                token[j]==' ' ||
                token[j]=='\0' ||
                token[j] == '\t' ||
                token[j] == '\n' 
            )){
                continue; //empty var name
            }
            //collects var name
            while(token[j]!='$' && 
                token[j]!=' ' &&
                token[j]!='\0' &&
                token[j] != '\t' &&
                token[j] != '\n' &&
                n<127){
                    name[n++] = token[j++]; 
            }
            name[n] = '\0'; 

            const char *val = get_var(name); // look up var
            if (val == NULL) val="";

            for (size_t k = 0; val[k] != '\0' && buffer_idx<299; k++){
                buffer[buffer_idx++] = val[k]; // copy var value into buffer
            }
        } else { 
            // not $, copy into result
            buffer[buffer_idx++] = token[j++];
            continue;
        }
    }
    buffer[buffer_idx] = '\0'; 
    if(buffer_idx>128){
        *overflow = 1;
        free(buffer);
        return NULL;
    }
    return buffer; // returns heap-allocated memory
}
