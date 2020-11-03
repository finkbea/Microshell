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
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include "defn.h"
#include "globalv.h"

/* Constants */
//DOLLAR is the dollar sign ($), LCURLY is ({) and RCURLY is (}), POUND is the pound sign (#), STAR is (*)
#define DOLLAR 36
#define LCURLY 123
#define RCURLY 125
#define LINELEN 1024
#define POUND 35
#define SPACE ' '
#define STAR 42
#define QUESTIONMARK 63
#define RIGHTPARENTHESES 41
#define LEFTPARENTHESES 40
#define WAIT 1
#define NOWAIT 0

//global variable, tracks the current index of the new array

/* Prototypes */
int expand (char *orig, char *new, int newsize);
int concatenate(char *new, char *temp, int newIndex);

//expand
int expand (char *orig, char *new, int newsize){
  //first i get the size for the loop through the line, then i create a tempIndex that will track the index of the temp array, a boolean(int) inBraces that knows whether or
  //not we are inside the curly braces. Lastly i make a character array called temp that will be used to hold everything inside the curly braces.
  int size = strlen(orig);
  int tempIndex = 0;
  int inBraces = 0;
  int PC = 0; //parentheses count
  int inParentheses; //checks if currently inside parentheses
  int pipeline[2];
  char temp[LINELEN];
  char *parTemp;
  int newIndex = 0;
  for (int i =0; i<size; i++){
    //checks if the current char is a dollar and the next one is left parentheses and their is at least as many left parentheses as right parentheses
    if (orig[i] == DOLLAR && orig[i+1] == LEFTPARENTHESES && PC>=0 && inParentheses == 0 && inBraces == 0){
      inParentheses = 1;
      parTemp = &orig[i+2];
      i+=2;
      PC++;
    }
    //the parentheses count gets +1 for left parentheses and -1 for right, so at 0 the amount of left and right parentheses should be equal
    if (orig[i] == LEFTPARENTHESES){
      PC++;
    }
    //decrements Parentheses count for each right parentheses
    if (orig[i] == RIGHTPARENTHESES){
      PC--;
    }
    //if we're in parentheses and their is the same amount of right and left parentheses then I make sure the pipeline won't return an error and call processline on parTemp
    //with no wait. I then close the write end of the pipe and read everything in the read end, storing it in new. Afterwords I check for errors again and then remove all
    //newline characters from new.
    if (PC == 0 && inParentheses == 1){
      inParentheses = 0;
      orig[i] = 0;
      if (pipe(pipeline) < 0){
        perror("pipe");
        return -1;
      }
      EXITVALUE = processline(parTemp, 0, pipeline[1], 2);
      int bytesRead = 1;
      close(pipeline[1]);
      while(newIndex < newsize && (bytesRead = read(pipeline[0], new+newIndex, newsize-newIndex)) > 0){
        newIndex+=bytesRead;
      }
      if (EXITVALUE < 0) {
        close(pipeline[0]);
        close(pipeline[1]);
        perror("pipe");
      }
      close(pipeline[0]);
      //removes the newline characters
      for (int j = 0; j < newIndex; j++){
        if (new[j] == '\n'){
          new[j] = SPACE;
        }
      }
      new[newIndex]=0;
      int status;
      waitpid(EXITVALUE, &status, NOWAIT);
      //new[strlen(new)-1] = 0;

    }
    //there should always be at least as many left parentheses as right parentheses
    if (PC < 0){
      fprintf(stderr, "Incorrect parentheses\n");
      return 0;
    }


    //contains the rest of the functions inside of it, if the two paretheses counts are not equal then we are either inside parentheses or there are too many right parentheses
    if (PC == 0 && inParentheses == 0){

      //makes sure we don't exceed the max size
      if (newIndex > LINELEN){
        return 0;
      }
      //The first expansion, checks if the current character and next character are both dollar signs, if they are then I create a new character array of length 6
      //(5 for the pid and 1 for the 0), I then use snprintf() to copy the pid into the character array and then concatenate them together before finally incrementing i
      //so as to avoid printing a dollar sign.
      if (orig[i] == DOLLAR && orig[i+1] == DOLLAR){
        char pid[6];
        snprintf(pid,6,"%d", getpid());
        newIndex = concatenate(new, pid, newIndex);
        i++;
      }
      //a simple statement, if the current character in orig is the dollar sign and the next character is a left curly brace then we are now inside curly brackets so our boolean
      //gets changed and I increment i by one so as to not get the curly bracket inside the new array (because I am checking the next element).
      else if (orig[i] == DOLLAR && orig[i+1] == LCURLY){
        inBraces=1;
        i++;
      }
      //Once I reach the right curly brace then I set the boolean so we are no longer in braces and set a null value at the end of temp. I then check if getenv of temp
      //is not equal to NULL (if it is, we do nothing because nothing needs to be changed), if it isn't I call concatenate to add the getenv of temp to the new array.
      //After this is done I then reset tempIndex to 0 and temp to empty.
      else if (orig[i] == RCURLY && inBraces == 1){
        inBraces=0;
        temp[tempIndex] = 0;
        if (getenv(temp) != NULL){
          newIndex = concatenate(new, getenv(temp), newIndex);
        }
        memset(temp, 0, tempIndex);
        tempIndex=0;
      }
      //$n, I check if the value at teh current index is equal to a dollar sign, if the value at the next index is an integer, and make sure I'm not in curly braces.
      //In the function I first set x equal to the int value of the next index and then I check if the number of command line arguments is 1 (just ush), if so then I
      //make sure x (the value following the dollar sign) is 0, if so I add "ush.c" (the name of the file) to the new line. This is the only case that matters in the
      //interactive execution. If there is more than one argument I first check if x is 0, if so then I print out the first argument (the name of the script).
      //If x is greater than the number of arguments, nothing happens, otherwise I just print out the argument after x. Lastly I do i++, because I already know the next
      //index will just be x, something I don't need to print.
      //For explanation of SHIFTED view comments on shiftC in builtin.c
      else if (orig[i] == DOLLAR && isdigit(orig[i+1]) && inBraces == 0) {
        int ti = i;
        char num[LINELEN] = "";
        int numIndex = 0;
        ti=i;
        while (isdigit(orig[ti+1]) != 0){
          num[numIndex] = orig[ti+1];
          numIndex++;
          ti++;
        }
        int x = atoi(num);
        if (AC == 1){
          if (x == 0){
            newIndex = concatenate(new, "./ush", newIndex);
          }
        }
        else {
          if (x == 0){
            newIndex = concatenate(new, MV[1], newIndex);
          }
          else if (x+SHIFTED >= AC-1){
          }
          else {
            newIndex = concatenate(new, MV[x+1+SHIFTED], newIndex);
          }
        }
        i+=numIndex;
      }
      //$#, If the current index is a dollar sign and the next is the pound sign and I'm not in braces then I just add the argument count minus one to new and increment i
      //to skip over the pound sign.
      else if (orig[i] == DOLLAR && orig[i+1] == POUND && inBraces == 0) {
        char buffer[AC%10];
        int x = AC-1-SHIFTED;
        if (x < 1){
          x=1;
        }
        sprintf(buffer, "%d", x);
        newIndex = concatenate(new, buffer, newIndex);
        i++;
      }
      //simple wildcard
      //Can only be accessed if the current index us a space, the next index is the star, and the one after that is either a space or null terminator and it cannot be in braces.
      //Opens the current directory makes sure it isn't null. I then use file of type struct dirent and readdir to view the next directory entry. I then add a space
      //(this is very important to not create errors), finally I enter into the main portion of my loop and so long as the file isn't NULL I keep looping. If the file name
      //doesn't start with a period I add it into new and add a space before reading the next file then increments i by 2 at the end (after the loop) to skip over the next two
      //indices that I already accounted for here.
      else if (orig[i] == SPACE && orig[i+1] == STAR && (orig[i+2] == SPACE || orig[i+2] == '\0') && inBraces == 0){
        DIR *directory = opendir(".");
        if (directory == NULL){
          fprintf(stderr, "Improper directory\n");
          return 0;
        }
        struct dirent *file;
        file = readdir(directory);
        //put a space here so it won't attach to the command
        new[newIndex] = SPACE;
        newIndex++;
        while (file != NULL){
          if (file->d_name[0] != '.'){
            newIndex = concatenate(new, file->d_name, newIndex);
            new[newIndex] = SPACE;
            newIndex++;
          }
          file = readdir(directory);
        }
        i+=2;
      }
      //complex wildcard
      //Similar to the simple wildcard except I got the length of the stuff following the * using the same method used two else if statements above. When inside the while
      //loop I just compare the last couple of characters of the two strings and before adding them to new. Lastly I increment i by the number of chars that follow the *.
      else if (orig[i] == SPACE && orig[i+1] == STAR && (orig[i+2] != SPACE && orig[i+2] != '\0') && inBraces == 0){
        int ti = i;
        char string[LINELEN] = "";
        int stringIndex = 0;
        ti=i+1;
        while (orig[ti+1] != SPACE && orig[ti+1] != '\0'){
          string[stringIndex] = orig[ti+1];
          stringIndex++;
          ti++;
        }
        DIR *directory = opendir(".");
        if (directory == NULL){
          fprintf(stderr, "Improper directory\n");
          return 0;
        }
        struct dirent *file;
        file = readdir(directory);
        //put a space here so it won't attach to the command
        new[newIndex] = SPACE;
        newIndex++;
        while (file != NULL){
          char *name = file->d_name;
          int len = strlen(name);
          if (strcmp(&name[len-stringIndex], string) == 0){
            newIndex = concatenate(new, name, newIndex);
            new[newIndex] = SPACE;
            newIndex++;
          }
          file = readdir(directory);
        }
        i+=stringIndex;
      }
      //$?
      //gets the length of the int and creates a buffer so that I can store the int in a char array so I can use concatenate. Resets the error value to 0 afterwards
      else if (orig[i] == DOLLAR && orig[i+1] == QUESTIONMARK && inBraces == 0){
        char sEXITVALUE[5];
        sprintf(sEXITVALUE, "%d", EXITVALUE);
        newIndex = concatenate(new, sEXITVALUE, newIndex);
        i++;
        EXITVALUE = 0;
      }
      //if there is a slash before the * we just print the *, so I increment the index and add the following char to new (which is the *)
      else if (orig[i] == '\\' && orig[i+1] == STAR && inBraces == 0){
        i++;
        new[newIndex] = orig[i];
        newIndex++;
      }
      //If the part of the input is inside braces it just gets added to the temp array and I increment the tempIndex.
      else if (inBraces == 1){
        temp[tempIndex] = orig[i];
        tempIndex++;
      }
      //If the input isn't in braces and none of the other if statements worked then I just copy the current character into the new array and increment the index
      else if (inBraces == 0){
        new[newIndex] = orig[i];
        newIndex++;
      }
    }
  }
  //Once the loop is over I check to make sure I found a closing curly bracket, if there wasn't one then we return 0 and alert the user.
  if(inBraces == 1){
    new[newIndex]=0;
    newIndex=0;
    fprintf(stderr, "Odd number of curly brackets\n");
    return 0;
  }
  //checks if the expansion is too long
  if (newIndex >= newsize){
    new[newIndex]=0;
    newIndex=0;
    fprintf(stderr, "expansion too long\n");
    return 0;
  }
  //sets a 0 at the end of the string and sets the newIndex to 0 so as not to break the program if the user inputs another statement
  new[newIndex]=0;
  newIndex=0;
  return 1;

}
//Combines two strings together, just runs through the second array and adds it character by character into the new array (this is why newIndex is global)
int concatenate(char *new, char *temp, int newIndex){
  int i = 0;
  while (temp[i] != 0){
    new[newIndex] = temp[i];
    newIndex++;
    //checks if the expansion is too long
    if (newIndex >= LINELEN){
      fprintf(stderr, "Size cannot exceed 200,000\n" );
      return -1;
    }
    i++;
  }
  return newIndex;
}
