#include "variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Var {
    char *name;
    char *val;
};
 static struct Var *variables = NULL; // dynamic array of variables
 static size_t count = 0; //stores number of variables
 static size_t capacity = 0;

/* Prereq: name and val are NULL terminated strings
 */
int set_var(const char *name, const char *val){
    if (name==NULL || val==NULL || name[0]=='\0') return 0;

    for(size_t i=0; i<count; i++){
        // variable name already exists
        if (strcmp(variables[i].name, name)==0){ 
            //no update required
            if(strcmp(variables[i].val,val)==0) return 1;

            // free old value
            char *new_val = malloc(strlen(val) +1);
            if(!new_val) return 0;
            strcpy(new_val, val); 
            free(variables[i].val); // free the old value
            variables[i].val = new_val; 
            return 1;
        }
    }
    // new variable being added
    if(count>=capacity){  
        size_t new_cap;
        if (capacity==0){
            new_cap=2;
        } else{
            new_cap = capacity*2;
        }

        struct Var *newStruct = realloc(variables, new_cap * sizeof(struct Var));
        if (newStruct==NULL) return 0;
        variables = newStruct;
        capacity = new_cap;
    }
    
    variables[count].name = malloc(strlen(name)+1); // name + '\0'
    if(variables[count].name==NULL) return 0;
    strcpy(variables[count].name, name);

    variables[count].val = malloc(strlen(val)+1);
    if(variables[count].val==NULL){
        free(variables[count].name);
        return 0;
    }
    strcpy(variables[count].val,val);
    count++;
    return 1; 
};

/* Prereq:name is a NULL terminated string
 * Return: the value at the address pointed to by name
 */
const char *get_var(const char *name){
    if (name==NULL){ 
        return NULL;
    }
    if (variables == NULL||count==0){ 
        return NULL;
    }
    
    for (size_t i=0; i<count; i++){
        if(strcmp(variables[i].name, name)==0){
            return variables[i].val; 
        }
    }
    return NULL; // var not found
}

void print_vars(void){
    printf("Variables: (%zu):\n", count);
    for(size_t i=0;i<count;i++){
        printf(" %s = %s\n", variables[i].name, variables[i].val);
    }
}


