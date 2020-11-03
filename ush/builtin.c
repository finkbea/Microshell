/* CS 347 -- environment variable processing!
 *
 *   made April 2019 by Adicus Finkbeiner
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
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include "globalv.h"

/* Constants */
//NUMCOMMANDS is the total number of commands currently implemented
//MAXLENGTH is the maximum length of the commands names, it is currently just set to 10 because nothing is longer than 10 characters
//OVERWRITE is set to 1, set this to 0 and envset will no longer overwrite previous environmental variables with the same name
#define NUMCOMMANDS 7
#define MAXLENGTH 10
#define OVERWRITE 1
#define WAIT 1
#define NOWAIT 0
typedef void (*funcptr) (char **, int argc);

//EXITVALUE
//EXITVALUE is for the dollar question mark expansion done in expand.c and EXITVALUE is a global variable declared in globalv.h. It is set to 0 if the builtin command runs correctly and
//set to 1 otherwise.

/* Prototypes */
int commands(char **args, int argc);

//if there is no argument I just exit, elsewise I convert the first argument to a number and exit using that
void exitC(char **args, int argc){
  if (argc == 1){
    exit(0);
  }
  else{
    exit(atoi(args[1]));
  }
}
//checks if there is 3 arguments, if there aren't then we return an error, else I just use setenv
void envsetC(char **args, int argc){
  if (argc != 3){
    fprintf(stderr, "incorrect number of arguments, follow the format: envset name value\n");
    EXITVALUE = 1;
    return;
  }
  setenv(args[1], args[2], OVERWRITE);
}
//Makes sure their are two arguments then calls unsetenv
void envunsetC(char **args, int argc){
  if (argc != 2){
    fprintf(stderr, "incorrect number of arguments, follow the format: envunset name\n");
    EXITVALUE = 1;
    return;
  }
  unsetenv(args[1]);
}
//If no destination is specified it will take us to the home directory, else if there aren't two arguments it will return an error. Lastly if there are two arguments
//it will test to make sure it is a proper directory and if it is then will change to that directory.
void cdC(char **args, int argc){
  if (args[1] == NULL){
    chdir(getenv("HOME"));
  }
  else if (argc != 2){
    fprintf(stderr, "incorrect number of arguments, follow the format: cd [directory]\n");
    EXITVALUE = 1;
    return;
  }
  else{
    int test = chdir(args[1]);
    if (test != 0){
      fprintf(stderr, "input a correct directory\n");
      EXITVALUE = 1;
      return;
    }
    chdir(args[1]);
  }
}
//Uses my global variable SHIFTED,shift can only take in one or two arguments so any more causes an error, if it is one argument I just set SHIFTED to 1. I then check if
//it tries to shift more than the number of command line arguments, if so it returns an error. Lastly, if nothing goes wrong, SHIFTED adds the first argument to itself,
//which is the amount that is supposed to be shifted. SHIFTED gets used in expand in expand.c for the $# and $n parts. I just add SHIFTED to the non zero n's in $n and
//I subtract SHIFTED from $#.
void shiftC(char **args, int argc){
  if (argc > 2){
    fprintf(stderr, "Shift requires exactly two arguments\n");
    EXITVALUE = 1;
    return;
  }
  else if (argc == 1){
    SHIFTED = 1;
  }
  else if (atoi(args[1]) > AC-1){
    fprintf(stderr, "Cannot shift more than the number of arguments\n");
    EXITVALUE = 1;
    return;
  }
  else {
    SHIFTED += atoi(args[1]);
  }
}
//Reverse shift, its the exact same as shift only if their is only one argument, SHIFTED gets reset, and I subtract from SHIFTED rather than add to it
void unshiftC(char **args, int argc){
  if (argc > 2){
    fprintf(stderr, "Unshift requires exactly two arguments\n");
    EXITVALUE = 1;
    return;
  }
  else if (argc == 1){
    SHIFTED = 0;
  }
  else if (atoi(args[1]) > SHIFTED){
    fprintf(stderr, "Cannot shift more than the number of arguments\n");
    EXITVALUE = 1;
    return;
  }
  else {
    SHIFTED -= atoi(args[1]);
  }
}
//increments through each argument, #include "globalv.h"for each argument (file name) it will first create the buffer for stat and then make sure the call to stat returns a positive number.
//If it doesn't then I return an error. Then it goes through and prints out each part of the argument for sstat. The name is just the name, for the username and group name
//it gets the values from using the imported structs and functions. for the file permissions list it just checks all of the permissions. The number of links and size in bytes
//were both stored in my original struct buf and the modification time is of type time_t and contains time values stored in the buffer, it then calls localtime
//and asctime on the address of the seconds.
void statC(char **args, int argc){
  for (int i =1; i <argc; i++){
    struct stat buf;
    if (stat(args[i], &buf) <  0){
      //fprintf(stderr, "input a correct file\n");
      EXITVALUE = 1;
      return;
    }
    else {
      //file name
      printf("%s ", args[i]);
      //the user name
      struct passwd *pass;
      pass = getpwuid(buf.st_uid);
      printf("%s ", pass->pw_name);
      //the group name
      struct group *pass2;
      pass2 = getgrgid(buf.st_gid);
      printf("%s ", pass2->gr_name);
      //file permissions list
      printf( (S_ISDIR(buf.st_mode)) ? "d" : "-");
      printf( (buf.st_mode & S_IRUSR) ? "r" : "-");
      printf( (buf.st_mode & S_IWUSR) ? "w" : "-");
      printf( (buf.st_mode & S_IXUSR) ? "x" : "-");
      printf( (buf.st_mode & S_IRGRP) ? "r" : "-");
      printf( (buf.st_mode & S_IWGRP) ? "w" : "-");
      printf( (buf.st_mode & S_IXGRP) ? "x" : "-");
      printf( (buf.st_mode & S_IROTH) ? "r" : "-");
      printf( (buf.st_mode & S_IWOTH) ? "w" : "-");
      printf( (buf.st_mode & S_IXOTH) ? "x" : "-");
      //number of links and size in bytes
      printf(" %lu %lu ", buf.st_nlink, buf.st_size);
      //the mofification time
      time_t secs = buf.st_mtim.tv_sec;
      //char *timestr = ctime(&secs);
      printf("%s", asctime(localtime(&secs)));
    }
  }
}

//checks if there is a built in command and executes one if found
int commands(char **args, int argc){
  //"string array" of all the commands
  char strings[NUMCOMMANDS][MAXLENGTH] = {"exit", "envset", "envunset", "cd", "shift", "unshift", "sstat"};
  //function pointers to all of the functions currently in builtin
  funcptr flist[NUMCOMMANDS];
  flist[0] = exitC;
  flist[1] = envsetC;
  flist[2] = envunsetC;
  flist[3] = cdC;
  flist[4] = shiftC;
  flist[5] = unshiftC;
  flist[6] = statC;
  //checks if the command in args is equal to the current command string in strings, if so then that command is executed and we return 1 to signify that it was done.
  //If no command was found in this loop then 0 is returned.
  for (int i =0; i <NUMCOMMANDS; i++){
    if (strcmp(args[0], strings[i]) == 0){
      flist[i](args, argc);
      argc = 0;
      return 0;
    }
  }
  return 1;
}
