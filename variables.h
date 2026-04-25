#ifndef __VARIABLES_H__
#define __VARIABLES_H__

/* Prereq: name and val are NULL terminated strings
 */
int set_var(const char *name, const char *val);

/* Prereq:name is a NULL terminated string
 * Return: the value at the address pointed to by name
 */
const char *get_var(const char *name);

void print_vars(void);

#endif