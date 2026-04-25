#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "io_helpers.h"


// ===== Output helpers =====

/* Prereq: str is a NULL terminated string
 */
void display_message(char *str) {
    write(STDOUT_FILENO, str, strnlen(str, MAX_STR_LEN));
}


/* Prereq: pre_str, str are NULL terminated string
 */
void display_error(char *pre_str, char *str) {
    write(STDERR_FILENO, pre_str, strnlen(pre_str, MAX_STR_LEN));
    write(STDERR_FILENO, str, strnlen(str, MAX_STR_LEN));
    write(STDERR_FILENO, "\n", 1);
}


// ===== Input tokenizing =====

/* Prereq: in_ptr points to a character buffer of size > MAX_STR_LEN
 * Return: number of bytes read
 */
ssize_t get_input(char *in_ptr) {
    int retval = read(STDIN_FILENO, in_ptr, MAX_STR_LEN+1); // Not a sanitizer issue since in_ptr is allocated as MAX_STR_LEN+1
    int read_len = retval;
    // Probably should exit if the retval is an error -- but for now, just returning.
    if (retval == -1) { // error occured
        if (errno == EINTR){
            // ctr + c, get input like normally
            in_ptr[0] = '\0';
            write(STDOUT_FILENO, "\n", 1);
            return -1;

        }
        read_len = 0;
    }
    if (read_len > MAX_STR_LEN) {
        read_len = 0;
        retval = -1;
        write(STDERR_FILENO, "ERROR: input line too long\n", strlen("ERROR: input line too long\n"));
        int junk = in_ptr[MAX_STR_LEN];
        while(junk != '\n' && (junk = getchar()) != EOF);
    }
    in_ptr[read_len] = '\0';
    return retval;
}

/* Prereq: in_ptr is a string, tokens is of size >= len(in_ptr)
 * Warning: in_ptr is modified
 * Return: number of tokens.
 */
size_t tokenize_input(char *in_ptr, char **tokens) {
    char *curr_ptr = strtok (in_ptr, DELIMITERS);
    size_t token_count = 0;
    
    // DELIMENTERS = spaces, tabs, new lines
    while (curr_ptr!=NULL) {  
        tokens[token_count] = curr_ptr;
        token_count ++;
        curr_ptr = strtok(NULL,DELIMITERS); // next token from the prev string
       
    }
    tokens[token_count] = NULL;
    return token_count;
}
