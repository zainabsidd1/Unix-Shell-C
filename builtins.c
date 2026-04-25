#include <string.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "builtins.h"
#include "io_helpers.h"
#include "commands.h"



// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    if(cmd_num == BUILTINS_COUNT){ //found no matching builtin
        return NULL;
    }
    return BUILTINS_FN[cmd_num]; // returns a function pointer
}

// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;
    int print = 0;

    while (tokens[index] != NULL) {
        if(tokens[index][0] != '\0'){
            if (print){
                display_message(" ");
            }
            display_message(tokens[index]);
            print = 1;
        }
        index += 1;
    }
    display_message("\n");
    return 0;
}

ssize_t bn_cd(char **tokens){
    int size = 0;
    for(int i=0;tokens[i]!=NULL; i++){
        size ++;
    }

    if (size ==1){  // just cd typed
        // go to home dir
        struct passwd *info = getpwuid(getuid());
        if(!info){
             display_error("ERROR: Invalid path","");
             return -2;
        }
        char *home = info->pw_dir;
        int res = chdir(home);
        if (res == -1){
             display_error("ERROR: Invalid path","");
             return -2;
        }
    }else if(size == 2){
        char *currToken = ls_helper(tokens[1]);
        if (currToken == NULL){
            display_error("ERROR: Invalid path","");
             return -2;
        }
        int res = chdir(currToken);
        free(currToken);
        if(res == -1){
        display_error("ERROR: Invalid path","");
        return -2;} 
        
    } else if(size >2){
        display_error("ERROR: Too many arguments: cd takes a single path","");
        return -2;
    }
    return 0;
}

int checkDots(char *token){ //return 1 if the path is only dots, 0 otherwise
    if (token[0] == '\0')return 0;
    for(int i=0; token[i] != '\0'; i++){
        if(token[i] != '.'){
            return 0;
        }
    }
    return 1;
}
char *getDirectory(char *token){
    
    size_t length = strlen(token); //counts the .  -> need length - 1  parent directory shifts
    char *buffer = malloc((length)*3+1);
    if (buffer == NULL){
        return NULL;
    }
    if (length<=1){
        buffer[0]='.';
        buffer[1]='\0';
    } else{
        buffer[0] = '\0';
    }
    for (int i=1; i<(int)length; i++){ // stops at length - 1
        strcat(buffer, "../");
    }
    if(length>1)buffer[strlen(buffer)-1] = '\0'; // remove final /
    return buffer;
}

ssize_t bn_cat(char **tokens){
    if (tokens[1] == NULL){ // no file, read from STDIN
        char buf[1024];
        ssize_t num;
        while ((num = read(STDIN_FILENO, buf, sizeof(buf))) >0){
            write(STDOUT_FILENO,buf, num); // write to stdout
        }
        return 0;
    }

    if (tokens[2] != NULL){
        display_error("ERROR: Too many arguments: cat takes a single file","");
        return -2;
    }
    // file was given
    FILE *f = fopen(tokens[1], "r");
    if (f==NULL){
        display_error("ERROR: Cannot open file","");
        return -2;
    }
    char buffer[1024];
    while(1){
        size_t numBytes = fread(buffer, 1, sizeof(buffer), f);
        if(numBytes == 0){
            break;
        }
        write(STDOUT_FILENO, buffer, numBytes);
    }
    fclose(f);
    return 0;
}

ssize_t bn_wc(char **tokens){
    int c;
    int c_count = 0;
    int w_count = 0;
    int nl_count = 0;
    int wordFlag = 0;

    if (tokens[1] == NULL){
        char buf[1024];
        ssize_t num;
        while ((num = read(STDIN_FILENO, buf, sizeof(buf))) >0){
            for(int i=0; i<num; i++){
                c_count ++;
                if (buf[i]=='\n') nl_count ++;
                if(buf[i]== ' ' || buf[i]=='\n' ||buf[i]=='\t'){ // white space
                    wordFlag = 0; // not in a word
                } else { //at a char
                    if(wordFlag==0)w_count++; //prev was whitespace, new word
                    wordFlag = 1;
                }
            }
        }
    } else {
        if (tokens[2] != NULL){
        display_error("ERROR: Too many arguments: wc takes a single file","");
        return -2;
        }
        FILE *f = fopen(tokens[1],"r");
        if (f==NULL){
            display_error("ERROR: Cannot open file","");
            return -2;
        }
        while((c = fgetc(f)) != EOF){ // char by char
            c_count ++;
            if (c=='\n') nl_count ++;
            if(c== ' ' || c=='\n' ||c=='\t'){ // white space
                wordFlag = 0; // not in a word
            } else { //at a char
                if(wordFlag==0)w_count++; //prev was whitespace, new word
                wordFlag = 1;
            }
        }
        fclose(f);
    }
    char buf[256];
    snprintf(buf,sizeof(buf), "word count %d\n", w_count);
    display_message(buf);
    snprintf(buf,sizeof(buf), "character count %d\n", c_count);
    display_message(buf);
    snprintf(buf, sizeof(buf),"newline count %d\n", nl_count);
    display_message(buf);
    
    return 0;
}

ssize_t bn_ls(char **tokens){
    int is_a =0; // show hidden
    int is_d = 0; //depth
    int d = -1;
    int is_f = 0; //substring
    int is_rec = 0; //recursive
    char *path =".";
    char *substr = NULL;
    for(int i=1; tokens[i]!=NULL; i++){
        if(strcmp(tokens[i],"--a")==0){
            is_a = 1; // turn on 'show hidden files'
        }
        else if(strcmp(tokens[i],"--f")==0){
            is_f = 1;
            if(tokens[i+1]!=NULL){
                substr = tokens[i+1];
                i++;
            } else{
                display_error("ERROR: Invalid flag usage","");
                return -2;
            }
        }
        else if(strcmp(tokens[i], "--rec")==0){
            is_rec = 1;
        }else if(strcmp(tokens[i], "--d")==0){
            if(tokens[i+1]==NULL){
                display_error("ERROR: No depth entered","");
                return -2;
            }
            is_d = 1;
            d = atoi(tokens[i+1]);
            i++;
        }
        else{
            path = tokens[i]; // not a flag, treated as a path
        }
    }
    if(is_d == 1 && is_rec == 0){
        display_error("ERROR: --d call needs --rec","");
        return -2;
    }
    char *new_path = ls_helper(path); // normalize the path
    if(new_path==NULL){
        display_error("ERROR: Invalid path","");
        return -2;
    }
    // struct dirent *ent = readdir(dir) one line at a time
    ls_rec(new_path, is_a, is_f, substr, is_rec, is_d, d, 0);
    free(new_path);

    return 0;
}

void ls_rec(char *path, int is_a, int is_f, char *substr, int is_rec, int is_d, int d, int curr_d){
    DIR *currDir = NULL;
    currDir = opendir(path);
    if (currDir == NULL){
        display_error("ERROR: Invalid path","");
        return;
    }
    struct dirent *ent;
    while((ent=readdir(currDir))!=NULL){
        char *f_name = ent->d_name; //entry name

        int isSpecialEntry = strcmp(f_name, ".")==0 || strcmp(f_name, "..")==0; //want to include . and .. no matter what
        if(!isSpecialEntry && f_name[0]=='.' && is_a ==0)continue; // hidden file, dont show all
        if(is_f==1 && strstr(f_name, substr)==NULL && ent->d_type != DT_DIR) continue; // not containing substr

        if(is_f==0 || strstr(f_name,substr)!= NULL){ //if --f off, print. or if --f on and substr matches, print.
            display_message(f_name);
            display_message("\n");
        }

        if(is_rec==1 && (ent->d_type == DT_DIR)){ // if dir
            if(strcmp(f_name, ".")==0 || strcmp(f_name, "..")==0)continue; //dont recurse
            if(is_d == 0 || curr_d < d){ // keep recursing until curr_depth = d
                size_t size = strlen(path) + strlen(f_name) + 2;
                char *buf = malloc(size); // +2 for '/' and '\0
                if(buf == NULL)return;
                snprintf(buf, size, "%s/%s", path, f_name); // new path
                ls_rec(buf, is_a, is_f, substr, is_rec, is_d, d, curr_d +1);
                free(buf);
            }
        }
    }
    closedir(currDir);
}

char *ls_helper(char *path){
    // converts the raw path string from user into equivalent parent-navigation path

    size_t length = strlen(path); //counts the .  -> need length - 1  parent directory shifts
    char *buffer = malloc((length)*4+1);
    buffer[0] = '\0';
    char *path_copy = malloc(strlen(path)+1);
    strcpy(path_copy, path);
    if(path_copy[0]=='/'){ // absolute path
        strcat(buffer,"/"); // stay absolute
    }
    char *curr_ptr = strtok(path_copy, "/"); //split on '/'
    
    while (curr_ptr!=NULL) {
        //return 1 if the path is only dots, 0 otherwise
        int is_dots = checkDots(curr_ptr);
        int len_buffer = strlen(buffer);
        if(len_buffer>0 && buffer[len_buffer-1] != '/'){ 
                strcat(buffer,"/");
        }
        if(is_dots==1){
            char *expanded = getDirectory(curr_ptr);
            if(expanded==NULL){
                free(buffer);
                free(path_copy);
                return NULL;
            } 
            strcat(buffer, expanded);
            free(expanded);
        } else{
            strcat(buffer, curr_ptr);
        }
        curr_ptr = strtok(NULL,"/"); // move to next path component 
    }
    free(path_copy);
    return buffer; //caller must free this
}

ssize_t bn_kill(char **tokens){
    if(tokens[1] == NULL){
        display_error("ERROR: The process does not exist", "");
        return -2;
    }

    pid_t pid = atoi(tokens[1]);
    int signum;

    if(tokens[2] == NULL){
        signum = SIGTERM;
    } else {signum = atoi(tokens[2]);}
    
    int res = kill(pid, signum);
    if (res == -1){
        if (errno == ESRCH) { // invalid pid
            display_error("ERROR: The process does not exist", "");
        } else if (errno == EINVAL){ // invalid signal
            display_error("ERROR: Invalid signal specified","");
        }
        return -2;
    }
    return 0;
    
}

ssize_t bn_ps(char **tokens){
    // list process names and ids for all processes launced by this shell
    
    (void)tokens;
    if(bg_count == 0){
        return 0;
    }
    for(int i=0; i<bg_count; i++){
        BgProcess process = bg_jobs[i];
        char buf[216];
        snprintf(buf, sizeof(buf), "%s %d\n", process.cmd, process.pid);
        write(STDOUT_FILENO, buf, strlen(buf));
    }
    return 1;
}

ssize_t bn_start_server(char **tokens){
    // start-server [port number]
    if (tokens[1] == NULL){
        display_error("ERROR: No port provided","");
        return -2;
    }
    int port = atoi(tokens[1]);

    pid_t pid = fork();
    if (pid == 0){ //child - server
        // socket configuration
        int listen_soc = socket(AF_INET, SOCK_STREAM, 0);
        if(listen_soc==-1){
            perror("socket");
            exit(1);
        }
        int reuse = 1;
        setsockopt(listen_soc, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));// allow reuse
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port=htons(port);
        addr.sin_addr.s_addr = INADDR_ANY; // accept connections from anywhere

        if(bind(listen_soc, (struct sockaddr*)&addr, sizeof(addr)) == -1){
            display_error("ERROR: Builtin failed: ","start-server");
            close(listen_soc);
            exit(1);
        }
        listen(listen_soc, 50); // start accepting connections
        int client_fds[1024];
        int client_count = 0;
        int next_client = 1;
        fd_set all_fds, listen_fds;
        int max_fd = listen_soc; // to track the num of fds to listen to

        FD_ZERO(&all_fds);
        FD_SET(listen_soc, &all_fds);

        while(1){
            listen_fds = all_fds; // working copy
            // blocks until some fd becomes readable
            if( select(max_fd + 1, &listen_fds, NULL, NULL, NULL) == -1){
                if(errno == EINTR) continue;
                break;
            }

            if (FD_ISSET(listen_soc, &listen_fds)){
                // listening socket is ready
                int client_fd = accept(listen_soc, NULL, NULL); // communicate with this client
                if (client_fd < 0) {
                    continue;
                }
                client_fds[client_count] = client_fd;
                if (client_fd > max_fd) {
                    max_fd = client_fd;
                }
                FD_SET(client_fd, &all_fds); // monitor this client too
                char msg[32];
                snprintf(msg, sizeof(msg), "client%d:\n", next_client);
                write(client_fd, msg, strlen(msg));
                next_client ++;
                client_count++;
            }

            for(int i=0; i<client_count; i++){
                if (FD_ISSET(client_fds[i], &listen_fds)){
                    char buf[256];
                    // read from client
                    ssize_t n = recv(client_fds[i], buf, sizeof(buf)-1, 0);

                    if (n<=0){ // disconnect client
                        FD_CLR(client_fds[i], &all_fds);
                        close(client_fds[i]);
                        client_fds[i] = client_fds[client_count - 1]; // remove from list
                        client_count --;
                        i--;
                    } else {
                        buf[n] = '\0';
                        if (strncmp(buf, "\\connected", 10) == 0){ // 'how many clients'
                            char msg[32];
                            snprintf(msg, sizeof(msg), "%d\n", client_count);
                            write(client_fds[i], msg, strlen(msg));
                        } else{ 
                            // not connection msg, write msg to server + all clients
                            write(STDOUT_FILENO, buf, n);
                            for(int j=0; j<client_count; j++){
                                write(client_fds[j], buf, n);
                            }
                        }
                    }
                }
            }
        }
        exit(0);
    } else if (pid>0){ //parent
        server_pid = pid;
        return 0;
    }
    return 0;
}

ssize_t bn_close_server(char **tokens){
    (void)tokens;
    if(server_pid == -1){
        display_error("ERROR: No server running", "");
        return -2;
    }
    kill(server_pid, SIGTERM);
    server_pid = -1;
    return 0;
}

ssize_t bn_send(char **tokens){
    // send [port number] [hostname] [message]
    if(tokens[1] == NULL){
        display_error("ERROR: No port provided","");
        return -2;
    } else if (tokens[2] == NULL){
        display_error("ERROR: No hostname provided", "");
        return -2;
    } else if (tokens[3] == NULL){ // no message
        return 0;
    }
    
    int port = atoi(tokens[1]);
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc<0){
        display_error("ERROR: Builtin failed: ", "send");
        return -2;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, tokens[2], &addr.sin_addr) <= 0) {
        display_error("ERROR: Could not resolve hostname", "");
        close(soc);
        return -2;
    }

    if(connect(soc, (struct sockaddr *)&addr, sizeof(addr))==-1){
        display_error("ERROR: Builtin failed: ", "send");
        close(soc);
        return -2;
    }

    // read client ID
    char id[32];
    ssize_t n = read(soc, id, sizeof(id)-1);
    if(n>0){
        id[n] = '\0';
        if(id[n-1]=='\n') { // remove newline
            id[n-1] = '\0';
        }
    }


    char msg[330];
    char full_msg[256] = "";
    for (int i = 3; tokens[i] != NULL; i++) {
        if (i > 3) strncat(full_msg, " ", sizeof(full_msg) - strlen(full_msg) - 1);
        strncat(full_msg, tokens[i], sizeof(full_msg) - strlen(full_msg) - 1);
    }
    int overflow = 0;
    char *expanded_msg = expand_token_server(full_msg, &overflow);
    if(overflow){
        display_error("ERROR: Message exceeded character limit", "");
        close(soc);
        return -1;
    }
    char *final_msg = expanded_msg ? expanded_msg : full_msg;

    if(strlen(final_msg)>128){
        display_error("ERROR: Message exceeded character limit","");
        if(expanded_msg) free(expanded_msg);
        close(soc);
        return -1;
    }

    snprintf(msg, sizeof(msg), "%s %s\n", id, final_msg);
    if(expanded_msg)free(expanded_msg);
    write(soc, msg, strlen(msg));
    close(soc);
    return 0;

}


ssize_t bn_start_client(char **tokens){
    // start-client [port number] [hostname]
    if(tokens[1] == NULL){
        display_error("ERROR: No port provided","");
        return -2;
    } else if (tokens[2] == NULL){
        display_error("ERROR: No hostname provided", "");
        return -2;
    }

    int port = atoi(tokens[1]);
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc<0){
        display_error("ERROR: Builtin failed: ", "start-client");
        return -2;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, tokens[2], &addr.sin_addr) <= 0) {
        display_error("ERROR: Could not resolve hostname", "");
        close(soc);
        return -2;
    }

    if(connect(soc, (struct sockaddr *)&addr, sizeof(addr))==-1){
        display_error("ERROR: Builtin failed: ", "start-client");
        close(soc);
        return -2;
    }

    // client id
    char id[32];
    ssize_t n = read(soc, id, sizeof(id)-1);
    if(n>0){
        id[n] = '\0';
        if(n>0 && id[n-1] == '\n'){
            id[n-1] = '\0';
        }
    }

    fd_set readfds, all_fds;
    FD_ZERO(&readfds);
    FD_ZERO(&all_fds);
    FD_SET(STDIN_FILENO, &all_fds);
    FD_SET(soc, &all_fds);

    while(1){
        readfds = all_fds; // local copy
        if(select(soc+1, &readfds, NULL, NULL, NULL)<0){
            if(errno==EINTR) continue;
            break;
        }
        if(FD_ISSET(STDIN_FILENO, &readfds)){ // user typed something
            char buf[MAX_STR_LEN + 1];
            ssize_t bytes_read = read(STDIN_FILENO, buf, MAX_STR_LEN);
            if (bytes_read<= 0) break; //EOF
            buf[bytes_read] = '\0';

            if(buf[0] == '\0') break;

            if(buf[bytes_read-1] == '\n') buf[bytes_read-1] = '\0'; // remove nl
            int overflow = 0;
            char *expanded = expand_token_server(buf, &overflow);
            if(overflow){
                display_error("ERROR: Message exceeded character limit","");
                continue;
            }
            char *message = expanded ? expanded : buf;

            if(strlen(message)>128){
                display_error("ERROR: Message exceeded character limit","");
                if(expanded) free(expanded);
                continue;
            }
            char full_msg[256];
            if (strncmp(message, "\\connected", 10)==0){
                snprintf(full_msg, sizeof(full_msg), "\\connected\n");
            } else{
                snprintf(full_msg, sizeof(full_msg), "%s %s\n", id, message);
            }
            write(soc, full_msg, strlen(full_msg));
            if (expanded) free(expanded);
        }

        if(FD_ISSET(soc, &readfds)){ // server sent something
            char buf[256];
            int bytes_read = read(soc, buf, sizeof(buf)-1);
            if(bytes_read<=0) break; //disconnected
            buf[bytes_read] = '\0';
            write(STDOUT_FILENO, buf, bytes_read);
        }
    }
    close(soc);
    return 0;
}
