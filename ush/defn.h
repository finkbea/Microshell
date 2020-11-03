/* Prototypes */
int expand (char *orig, char *new, int newsize);
int commands(char **args, int argc);
int processline (char *line, int infd, int outfd, int flags);
