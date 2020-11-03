/* CS 352 -- Miro Shell!
 *
 *   Sept 21, 2000,  Phil Nelson
 *   Modified April 8, 2001
 *   Modified January 6, 2003
 *   Modified January 8, 2017
 *   Modified April 2019 by Adicus Finkbeiner for CS347
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "defn.h"
#include "globalv.h"



/* Constants */

#define LINELEN 200000
#define SPACE ' '
#define COMMENT 35 //#
#define DOLLAR 36 //$
#define WAIT 1
#define NOWAIT 0
#define PIPE 124
#define EXPAND 2 //flag for expand
//#define SIGINT 2

//global variable from globalv.h, initially set to 0
int SHIFTED = 0;
int EXITVALUE = 0;

/* Prototypes */

char ** arg_parse (char *line, int *argcptr);
int argcount(char *line);
char* comment(char *line);
void sigintHandler();
int numberOfPipes(char *line);
char** pipeProcessor(char *line, int pipeCount);
void pipes(char **pArgs, int pipeCount, int infd, int outfd);

char ** arg_parse (char *line, int *argcptr){
    //counts the total number of arguments found in line
    int argc = argcount(line);
    int size = strlen(line);
    if (argc == 0){
      return NULL;
    }
    char** args = malloc ((argc+1) * sizeof(char*));
    if (args == NULL){
      fprintf(stderr, "Malloc Error\n");
    }

    //reset isQuote to 0, set a new ARGcounter that starts at one, and a new NULLcounter, then run through a very similar for loop to the above one, if the current character
    //is a quote then we change our isQuote boolean from true to false or vice versa. If the current character is a space and the next one isn't a space then we add it to
    //args and increment the ARGcounter. If the current character is a space and the next one isn't a space or quote then we add a 0 and increment the null counter.
    args[0] = &line[0];
    int isQuote = 0;
    int ARGcounter=1;
    int NULLcounter =0;
    for (int j = 0; j<size; j++){
      if (*(line+j) == '\"'){
        isQuote = isQuote ^ 1;
      }
      if ((*(line+j) == SPACE && *(line+j+1) != SPACE) && isQuote == 0){
        args[ARGcounter] = &line[j+1];
        ARGcounter++;
      }
      if (((*(line+j) == SPACE && *(line+j-1) != SPACE) && *(line+j-1) != 0) && isQuote == 0){
        line[j] = '\0';
        NULLcounter++;
      }
    }
    //add one last 0 at the end and increment the counter
    line[size] = '\0';
    NULLcounter++;
    //if their is an odd number of quotes then it lets the user knows and ends the current command
    if (isQuote == 1){
      fprintf(stderr, "Odd number of quotes\n");
      return NULL;
    }

    //Quote Parsing
    //creates two pointers that mimic line. While the NULLcounter is greater than 0, I first set the write pointer equal to the read one b/c write won't increment on quotes
    //every time we hit a 0 value I decrement the counter.
    char *read = line;
    char *write = line;
    while (NULLcounter > 0){
      if (*read == '"'){
        read++;
      }
      if (*read == '\0'){
        *write = *read;
      }
      *write = *read;
      write++;
      read++;
      if (*write == 0){
        NULLcounter--;
      }
    }
   //sets the last value in args to NULL
    args[ARGcounter] = NULL;

    *argcptr = argc;
    return args;
    //tui enable in gdb
    //core dump: ulimit -c unlimited
}

int argcount(char *line){
  int argc = 0;
  //checks if the first element is a space, if not then I increment the counter
  if (*line != SPACE && *line != 0){
    argc++;
  }
  //checks the rest of the elements of line for arguments. If the current element is a space and the next one isn't we know that it is a word, I start the loop at one due
  //to how the check is carried out, this also leads me to do the above check for the first element.
  int size = strlen(line);
  //0 is false, 1 is true, is true if after finding one quote but before finding the second
  int isQuote = 0;
  for (int i =1; i<size; i++){
    if (*(line+i) == '\"'){
      isQuote = isQuote ^ 1;
    }
    if ((*(line+i) == SPACE) && (*(line+i+1) != SPACE) && isQuote == 0){
      argc++;
    }
  }
  return argc;
}

//Very simple function, checks if the current index is a pound sign and makes sure the previous index isn't a dollar sign, if that is the case then I simply add a null point
//at the start of the comment so the rest of the line doesn't need to be read.
char* comment(char *line){
  while (*line){
    if (*line == COMMENT && *(line-1) != DOLLAR){
      *line = 0;
    }
    line++;
  }
  return line;
}
//handles sigints, program won't end when doing ctrl+c
void sigintHandler(){
  signal(SIGINT,sigintHandler);
  fflush(stdout);
}

//returns the number of pipes, gets incremented everytime it sees the character |
int numberOfPipes(char *line){
  int num = 0;
  char *line2 = line;
  while (*line2 != 0){
    if (*line2 == PIPE){
      num++;
    }
    line2++;
  }
  return num;
}
//replaces all pipe symbols with end of string characters and loops until the number of end of string characters passed is equal to the total amount.
//Lastly I set an end of string character at the end of pArgs.
char** pipeProcessor(char *line, int pipeCount){
 char** pArgs = malloc ((pipeCount+2) * sizeof(char*));
 int i = 0;
 int pIndex = 0;
 int j = 0;
 int EOSC = 1; //end of string count
 int count = 0;

 while (count < EOSC){
   if (line[i] == PIPE){
     if (line[i-1] == SPACE){
       line[i-1]='\0';
     }
     line[i] = '\0';
     EOSC++;
   }
   if (line[i] == '\0'){
     pArgs[pIndex] = line + j;
     pIndex++;
     i++;
     j=i;
     count++;
   }
   else {
     i++;
   }
 pArgs[pIndex]=0;
 }
 return pArgs;
}
//the first pipeline is handled before the loop using pipeline, all the other pipelines are handled inside the loop using pipeline2 and the last pipeline is handled after
//the loop using pipeline3
void pipes(char **pArgs, int pipeCount, int infd, int outfd){
  int pipefd[2];
  int cpid;
  int tempRead;
  int i = 0;
  if (pipe(pipefd) < 0) {
    perror("pipe");
  }
  processline(pArgs[i], infd, pipefd[1], NOWAIT);
  close(pipefd[1]);
  tempRead = pipefd[0];
  i++;
  while (i < pipeCount-1){
    if (pipe(pipefd) < 0){
      perror("pipe");
    }
    processline(pArgs[i], tempRead, pipefd[1], NOWAIT);
    close(pipefd[1]);
    close(tempRead);
    tempRead = pipefd[0];
    i++;
  }
  cpid = processline(pArgs[i], tempRead, outfd, WAIT);

  waitpid(cpid, NULL, WNOHANG);
}

/* Shell main */

int main (int mainargc, char **mainargv)
{
  AC=mainargc;
  MV=mainargv;
  //I create a variable of type FILE*, then I make sure that there is more than one argument being passed to the shell. If so I use fopen to open a file whos name is stored
  //in index 1 of mainargv, and the "r" means to read the file. If that file doesn't exist it lets the user know the file does not exist and closes the program.
  //If their is only one argument file just gets set to stdin.
  FILE *file;
  if (mainargc > 1){
    file = fopen(mainargv[1], "r");
    if (!file){
      fprintf(stderr, "File does not exist\n");
      exit(127);
    }
  }
  else {
    file = stdin;
  }


  char   buffer [LINELEN];
  int    len;

  //handles sigints
  signal(SIGINT, sigintHandler);

  while (1) {

  /* prompt and get line */
  //will only print out the % if it is an interactive execution
  if (file == stdin){
    fprintf (stderr, "%% ");
  }

  //takes in file rather than stdin for the sake of argument processing
	if (fgets (buffer, LINELEN, file) != buffer)
	  break;

        /* Get rid of \n at end of buffer. */
  comment(buffer);
	len = strlen(buffer);
	if (buffer[len-1] == '\n')
	    buffer[len-1] = 0;

	/* Run it ... */
	processline (buffer, 0, 1, WAIT | EXPAND);
    }

    if (!feof(file))
        perror ("read");

    return 0;		/* Also known as exit (0); */
}

int processline (char *line, int infd, int outfd, int flags)
{
    pid_t  cpid;
    int    status;
    int argc=0;

    char new [LINELEN];
    int newsize = LINELEN;
    char **pArgs;
    int pipeCount;

    if (flags & EXPAND){
      if (expand(line, new, newsize) == 0){
        return -1;
      }
    }
    else {
      if (line != '\0'){
        strcpy(new, line);
      }
    }

    //pipes
    //I first make sure that there is at least one pipe, if so I set the pipe args (pArgs) to be equal to the results of my pipeProcessor on new.
    //Afterwords I pass pArgs to my pipes function along with the infd and outfd before waiting on any remaining zombies.
    pipeCount = numberOfPipes(new);
    if (pipeCount > 0){
      pArgs = pipeProcessor(new, pipeCount);
      pipes(pArgs, pipeCount, infd, outfd);
      while (waitpid(-1, &status, 0) > 0){
      }
      free(pArgs);
      return 0;
    }

    char** args = arg_parse(new, &argc);

    if (argc == 0){
      free(args);
      return 0;
    }
    if (commands(args, argc) == 0){
      free(args);
      return 0;
    }

    /* Start a new process to do the job. */
    cpid = fork();
    if (cpid < 0) {
      /* Fork wasn't successful */
      perror ("fork");
      return -1;
    }


    /* Check for who we are! */
    if (cpid == 0) {
      if (outfd != 1){
        dup2(outfd, 1);
      }
      if (infd != 0){
        dup2(infd, 0);
      }
      /* We are the child! */
      execvp (args[0], args);
      /* execlp reurned, wasn't successful */
      perror ("exec");
      fclose(stdin);  // avoid a linux stdio bug
      exit (127);
    }

    if (flags & WAIT){

    /* Have the parent wait for child to complete */
    if (wait (&status) < 0) {
      /* Wait wasn't successful */

      }
      if (WIFEXITED(status)){
        EXITVALUE = WEXITSTATUS(status);
      }
      else if (WIFSIGNALED(status)){
        EXITVALUE = 128 + WTERMSIG(status);
        if (WTERMSIG(status) != SIGINT){ //SIGINT is signal 2
          fprintf(stderr, "Terminated by %s\n", strsignal(status));
        }
        if (WCOREDUMP(status)){
          fprintf(stderr, " (core dumped)\n");
        }

      perror ("wait");
      }
    }
    free(args);
    return cpid;
}
