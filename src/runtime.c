#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <agon/vdp.h>
#include <agon/mos.h>
#include <agon/timer.h>
#include <time.h>
#include "runtime.h"

// runtime part of code
#define MAX_LINES 2048                    // max number of lines in our program
#define MAX_LEN 20                      // max length of chars in each code line

bool DEBUGGING = false;                 // are we debugging
uint16_t currentLine = 0;               // current line being executed
uint16_t returnLine = 0;               // line to use for a RET command
uint8_t previousMode ;
uint8_t resultChar = 1 ;
uint8_t carryChar = 0 ;
uint8_t varStart = 63 ;
uint8_t loopCount = 0 ;
uint8_t loopMax = 0 ;
uint16_t loopReturnLine = 0 ;


uint8_t dataSpace[256];
uint16_t labels[256];                   // used to store line number of each label 0-255
uint16_t returnStack[17];                   // used to store retun line numbers for each CALL
uint8_t returnStackIndex = 0;               // current return stack position
char codeData[MAX_LINES][MAX_LEN];      // used to store each line of code
uint8_t varSpace[30];                   // used to store each of 26 variables a-z
bool running = false;                   // is code running

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 

//void runcode(text_buffer* aTextBuffer){
void runcode(char* fname){

    previousMode = getsysvar_scrMode();
    vdp_logical_scr_dims(false);
    vdp_clear_screen();
    //char* fname = aTextBuffer->fname_;      // this is name of current file
    
    srand(time(NULL));  // seed with current time

    // probably not needed
    for(uint16_t loop = 0; loop < MAX_LINES; loop ++){
        for(uint16_t loop2 = 0; loop2 < MAX_LEN; loop2 ++){
        codeData[loop][loop2] = '\0';
        }
    }

    if(fname[0] == 0){
        printf("File not saved yet!");
        delay(2000);
        return;
    } 
  
  // There are several ways to have an "array of strings" in C, the simplest of 
  // which is to have a 2D char array with each string stored at a row in the 
  // 2D array.  This will result in "unused space" as not all rows may be used 
  // if the file has fewer lines than MAX_LINES or if rows are of less length 
  // than MAX_LEN, so we should think about whether we are OK with this or not.
  // Another more sophisticated technique would be to use dynamic memory 
  // allocation.
 
  
  // Create a file pointer variable to allow us to access the file
  FILE *file;
  
  // Open the file in reading mode, fopen() will return NULL if it fails to 
  // open the file...
  file = fopen(fname, "r");


  // If we've failed to open the file, exit with an error message and status, 
  // returning 1 instead of returning 0 is a signal to the shell that something
  // has gone wrong in the execution of our program.
  if (file == NULL)
  {
    printf("Error opening file.\n");

  }

    // numLines will keep track of the number of lines read so far from the file
    uint16_t numLines = 0;

    while (numLines < MAX_LINES && fgets(codeData[numLines], MAX_LEN, file) != NULL) {
        size_t len = strlen(codeData[numLines]);
        // Standard newline cleanup
        while (len > 0 && (codeData[numLines][len - 1] == '\n' || codeData[numLines][len - 1] == '\r')) {
            codeData[numLines][len - 1] = '\0';
            len--;
        }
        numLines++;
    }

  // Close the file when we are done working with it.
  fclose(file);
   
// try to load a data file if exists
  FILE *datafile;
  datafile = fopen("kiss.data", "r");

  if (datafile != NULL)
  {
        fread(dataSpace, 1,256, datafile ); 
        if(DEBUGGING) printf("got data file, 1st byte is: %d\n", dataSpace[0]);
  } else {
        if(DEBUGGING) printf("failed to open data file");
  }

    fclose(datafile);

  
// capture and store all labels for GOTOs and CALLs

    char curLine[MAX_LEN];

    for (int labelCounter = 0; labelCounter < numLines-1; labelCounter++){
        strcpy(curLine, codeData[labelCounter]);
        char *lcommand;
        char *lparam1;
        char *lparam2;

        // replace ',' with spc if used
        replace_char(curLine, ',',' ');
        replace_char(curLine, '=',' ');

        //remove unwanted extra words
        strip_substr(curLine, " to");
        strip_substr(curLine, " =");
        strip_substr(curLine, " with");
        strip_substr(curLine, " from");
        strip_substr(curLine, " by");
        strip_substr(curLine, " *");
        strip_substr(curLine, " +");
        strip_substr(curLine, " -");
        strip_substr(curLine, " upto");
        strip_substr(curLine, " times");

        strcpy(codeData[labelCounter],curLine);

        lcommand = strtok(curLine, " ");
        lparam1 = strtok(NULL, " ");
        lparam2 = strtok(NULL, " ");
        uint8_t labelNum;

        if (strcmp(lcommand,"label") == 0 || strcmp(lcommand,"LABEL") == 0 ){
            uint8_t pval = *lparam1;
            labelNum = atoi(lparam1);
            labels[labelNum] = labelCounter;
            if(DEBUGGING){
                printf("Got label %d to go to line %d\n", labelNum, labelCounter);
            }
        }
    }

// reset control variables
  running = true;
  currentLine = 0;

    if(DEBUGGING){
        printf("Running code with %d lines\n\n", numLines);
    }

// Parse each line in turn through the program

    while(running && currentLine < numLines ){
        char src[MAX_LEN];
        strcpy(src, codeData[currentLine]);
        
        char *command;
        char *param1;
        char *param2;
        char *param3;

        // split line into command and params
        command = strtok(src, " ");
        param1 = strtok(NULL, " ");
        param2 = strtok(NULL, " ");
        param3 = strtok(NULL, " ");

        if(DEBUGGING) printf("Line %d: ",currentLine);

        currentLine ++;     // inc line now, in case it gets changed by a GOTO

        parseLine( command,  param1,  param2);
        

        if(DEBUGGING){
            vdp_waitKeyUp();
            vdp_waitKeyDown();
        }

        // always allow ESC to exit
        if(vdp_getKeyCode() == 27){
            running =false;
        }
    }

    // now we have finished the program and ready to exit

    if(DEBUGGING){
        printf("\n\nVariable space dump\n\n");
        for(uint8_t asc = 0; asc < 28; asc++){
            printf("Var %c (%d) = %d\n", (char)asc+63, asc, varSpace[asc]);
        }
    }   

    if(DEBUGGING){
        printf("\nPress any key to exit");
    }
           if(DEBUGGING){
            vdp_waitKeyUp();
            vdp_waitKeyDown();
        }
    

    vdp_mode(previousMode);
    vdp_cursor_enable(true);
    vdp_clear_screen();

}
/*-----------------------------------------------


Here we step though all possible commands and act on them




------------------------------------------------- */

void parseLine(char *command, char *param1, char *param2){

//
//-----------------------------------------------
//
// process SET command
// SET var to value/value
//
//-----------------------------------------------

    if (strcmp(command,"set") == 0 || strcmp(command,"SET") == 0 ){
        uint8_t p2val = *param2;
        uint8_t varOffset;
        uint8_t leng = strlen(param2);
        uint8_t value;

        if(leng == 8){
            value = (uint8_t) strtol(param2, NULL, 2);
            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = value;

            if(DEBUGGING) printf("SET offset %d to ascii value %d\n", varOffset, value);

        }
        else if(p2val == 39){ // must be a single quote, ie char
            char v = param2[1];
            uint8_t value = v;

            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = value;

            if(DEBUGGING) printf("SET offset %d to ascii value %d\n", varOffset, value);


        } else if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);

            uint8_t value = varSpace[var];

            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = value;

            if(DEBUGGING) printf("SET offset %d to value of offset %d which is %d\n", varOffset, var, value);


        } else { // else it is an int value
            uint8_t value = atoi(param2);

            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = value;

            if(DEBUGGING) printf("SET offset %d to value %d\n", varOffset, value);
        }

    }


//-----------------------------------------------
//
// process RND command
// RND <value/value>  <value/value> 
// RND <variable> <range> 
//
//-----------------------------------------------

    if (strcmp(command,"rnd") == 0 || strcmp(command,"RND") == 0 ){
        uint8_t p2val = *param2;
        uint8_t varOffset;
        uint8_t leng = strlen(param2);
        uint8_t value;
        uint8_t rndValue;

        if(p2val == 39){ // must be a single quote, ie char
            char v = param2[1];
            uint8_t value = v;
            rndValue = rand() % value;
            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = rndValue = rand() % value;;

            if(DEBUGGING) printf("SET offset %d to random value %d\n", varOffset, rndValue);


        } else if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);

            uint8_t value = varSpace[var];
            rndValue = rand() % value;

            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = rndValue = rand() % value;;

            if(DEBUGGING) printf("SET offset %d to random value %d\n", varOffset, rndValue);


        } else { // else it is an int value
            uint8_t value = atoi(param2);
            rndValue = rand() % value;

            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = rndValue = rand() % value;;

            if(DEBUGGING) printf("SET offset %d to random value %d\n", varOffset, rndValue );
        }

    }
    
//-----------------------------------------------
//
//  process VDP command
//  VDP <value/variable>
//  sends a byte to VDP from a varaible, an int, or a char
//
//-----------------------------------------------

 if (strcmp(command,"vdp") == 0 || strcmp(command,"VDP") == 0 ){
            uint8_t p1val = *param1;
            uint8_t leng = strlen(param1);
            uint8_t value;
            bool deb = DEBUGGING;
            DEBUGGING = false;
            if(leng == 8){
                value = (uint8_t) strtol(param1, NULL, 2);
                if(DEBUGGING) printf("Send VDP BINARY value %d\n",value);
                putchar(value);
            }
            else if(p1val == 39){ // must be a single quote, ie char
                char v = param1[1];
                value = v;

                if(DEBUGGING) printf("Send VDP char %d\n", value);
                putchar(value);

            } else if(p1val > 57){ // must be a char, ie set to another variable
                uint8_t var = *param1;
                var = lower(var);
                value = varSpace[var];
               
                if(DEBUGGING) printf("Send VDP variable %d of value %d\n", var, value);
                putchar(value);

            } else { // else it is an int value
                value = atoi(param1);

                if(DEBUGGING) printf("Send VDP value %d\n", value);
                putchar(value);
                
            }
            DEBUGGING = deb;
       }

//-----------------------------------------------
//
//  process VDPS command
//  VDPS offset count
//  VDPS <value/variable> <value/variable>
//  sends several bytes to VDP from data
//
//-----------------------------------------------

   if (strcmp(command,"vdps") == 0 || strcmp(command,"VDPS") == 0 ){
            uint8_t p1val = *param1;
            uint8_t p2val = *param2;
            uint8_t offset;
            uint8_t count;

            if(p1val > 57){ // must be a char, ie set to another variable
                uint8_t var = *param1;
                var = lower(var);
                offset = varSpace[var];
            } else { // else it is an int value
                offset = atoi(param1);
            }
            if(p2val > 57){ // must be a char, ie set to another variable
                uint8_t var = *param2;
                var = lower(var);
                count = varSpace[var];
            } else { // else it is an int value
                count = atoi(param2);
            }

            for(uint8_t sendloop = 0; sendloop < count; sendloop ++){
                putch(dataSpace[offset + sendloop]);
            }

       }

//-----------------------------------------------
//
//  process SETDATA command
//  SETDATA <offset/variable> <value/variable>
//  stores a byte into data
//
//-----------------------------------------------

   if (strcmp(command,"setdata") == 0 || strcmp(command,"SETDATA") == 0 ){
            uint8_t p1val = *param1;
            uint8_t p2val = *param2;
            uint8_t offset;
            uint8_t value;

            if(p1val > 57){ // must be a char, ie set to another variable
                uint8_t var = *param1;
                var = lower(var);
                offset = varSpace[var];
            } else { // else it is an int value
                offset = atoi(param1);
            }
            if(p2val == 39){ // must be a single quote, ie char
                char v = param2[1];
                value = v;
            } else if(p2val > 57){ // must be a char, ie set to another variable
                uint8_t var = *param2;
                var = lower(var);
                value = varSpace[var];
            } else { // else it is an int value
                value = atoi(param2);
            }
            
            dataSpace[offset] = value;
           
       }

//-----------------------------------------------
//
//  process LOOP command
//  LOOP <value/variable>
//  loops <value> times
//
//-----------------------------------------------

   if (strcmp(command,"loop") == 0 || strcmp(command,"LOOP") == 0 ){
            uint8_t p1val = *param1;
            uint8_t loops;

            loopReturnLine = currentLine;

            if(p1val > 57){ // must be a char, ie set to another variable
                uint8_t var = *param1;
                var = lower(var);
                loops = varSpace[var];
            } else { // else it is an int value
                loops = atoi(param1);
            }
            loopMax = loops-1;
       }

//-----------------------------------------------
//
//  process ENDLOOP command
//  ENDLOOP <value/variable>
//
//-----------------------------------------------

   if (strcmp(command,"endloop") == 0 || strcmp(command,"ENDLOOP") == 0 ){
        if(loopMax > 0){
            // still running
            if(DEBUGGING) printf("Looping back\n");
            loopMax--;
            currentLine = loopReturnLine;
        } else {
            // end of looping
            if(DEBUGGING) printf("End of loop\n");
        }
    }

//-----------------------------------------------
//
//  process GETDATA command
//  GETDATA <offset/variable> <value/variable>
//  gets a byte from data
//
//-----------------------------------------------

   if (strcmp(command,"getdata") == 0 || strcmp(command,"GETDATA") == 0 ){
            uint8_t p1val = *param1;
            uint8_t p2val = *param2;
            uint8_t offset;
            uint8_t var;

            if(p1val > 57){ // must be a char, ie set to another variable
                var = *param1;
                var = lower(var);
                offset = varSpace[var];
            } else { // else it is an int value
                offset = atoi(param1);
            }

            if(p2val > 57){ // must be a char, ie set to another variable
                var = *param2;
                var = lower(var);
            } 
            if(DEBUGGING) printf("Get data at offset %d and which is %d put into var %d\n", offset, dataSpace[offset], var);
            varSpace[var] = dataSpace[offset];
           
       }

//-----------------------------------------------
//
//  process BEEP command
//  BEEP <offset/variable> <value/variable>
//  BEEP freq time
//
//-----------------------------------------------

   if (strcmp(command,"BEEP") == 0 || strcmp(command,"beep") == 0 ){
            uint8_t p1val = *param1;
            uint8_t p2val = *param2;
            uint8_t freq;
            uint8_t var;
            uint8_t time;

            if(p1val > 57){ // must be a char, ie set to another variable
                var = *param1;
                var = lower(var);
                freq = varSpace[var];
            } else { // else it is an int value
                freq = atoi(param1);
            }

            if(p2val > 57){ // must be a char, ie set to another variable
                var = *param2;
                var = lower(var);
                time = varSpace[var];
            } else { // else it is an int value
                time = atoi(param1);
            }
            if(DEBUGGING) printf("Beep at %d for %d \n", freq, time);
            vdp_audio_play_note(0, 127, freq * 10, time * 10);
       }

//-----------------------------------------------
//
// RAW - experimental
//
//-----------------------------------------------

    if (strcmp(command,"raw") == 0 || strcmp(command,"RAW") == 0 ){
            uint8_t p1val = *param1;

            if(p1val > 57){ // must be a char, ie set to another variable
                uint8_t var = *param1;
                var = lower(var);
                uint8_t value = varSpace[var];
               
                if(DEBUGGING) printf("Send VDP variable %d of value %d\n", var, value);
                putch(value);

            } else { // else it is an int value
                uint8_t value = atoi(param1);

                if(DEBUGGING) printf("Send VDP value %d\n", value);
                putch(value);
                
            }

       }

//-----------------------------------------------
//
// process ADD command
//  ADD <variable1/value> <variable2>
//  adds variable1 or value to variable2
//  result 'a' has 1 if overrun
//
//-----------------------------------------------

    if (strcmp(command,"add") == 0 || strcmp(command,"ADD") == 0 ){
        uint8_t p1val = *param1;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            uint8_t value = varSpace[var];

            uint8_t varOffest = *param2;
            varOffest = lower(varOffest);
            uint16_t added = value + varSpace[varOffest]; // add two numbers
            if(added > 255){
                added -= 256;
                varSpace[varOffest] = added;
                varSpace[carryChar] = 1;        // overrun
            } else {
                varSpace[varOffest] = added;
                varSpace[carryChar] = 0;        // no overrun, less that 256
            }
            
            if(DEBUGGING) printf("ADD %d to var offset %d totalling %d\n", value, varOffest, added);


        } else { // else it is an int value
            uint8_t value = atoi(param1);
            uint8_t varOffest = *param2;
            //varOffest = varOffest - 97;
            varOffest = lower(varOffest);
            uint16_t added = value + varSpace[varOffest]; // add two numbers
            if(added > 255){
                added -= 256;
                varSpace[varOffest] = added;
                varSpace[carryChar] = 1;        // overrun
                if(DEBUGGING) printf("OVERRUN carry set in 'a'\n");
            } else {
                varSpace[varOffest] = added;
                varSpace[carryChar] = 0;        // no overrun, less that 256
                //if(DEBUGGING) printf("IN BOUNDS\n");
            }
            
            if(DEBUGGING) printf("ADD %d to var offset %d totalling %d\n", value, varOffest, added);
        }

    }

//-----------------------------------------------
//
// process MULTIPLY command
//  MUL <variable1/value> <variable2>
//  adds variable1 or value to variable2
//  result 'a' has x256 overrun

    if (strcmp(command,"mul") == 0 || strcmp(command,"MUL") == 0 ){
        uint8_t p1val = *param2;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            uint8_t value = varSpace[var];

            uint8_t varOffest = *param1;
            varOffest = lower(varOffest);
            uint16_t timesed = value * varSpace[varOffest]; // mult two numbers
            if(timesed > 255){
                uint8_t factor = timesed / 256;
                varSpace[varOffest] = timesed%256;
                varSpace[carryChar] = factor;        // overrun
            } else {
                varSpace[varOffest] = timesed;
                varSpace[carryChar] = 0;        // no overrun, less that 256
            }
            if(DEBUGGING) printf("Multiplied variable %d by %d giving %d\n",  varOffest, value, timesed);
            


        } else { // else it is an int value
            uint8_t value = atoi(param2);
            uint8_t varOffest = *param1;
            varOffest = lower(varOffest);

            uint16_t timesed = value * varSpace[varOffest]; // mult two numbers
            if(timesed > 255){
                uint8_t factor = timesed / 256;
                varSpace[varOffest] = timesed%256;
                varSpace[carryChar] = factor;        // overrun
            } else {
                varSpace[varOffest] = timesed;
                varSpace[carryChar] = 0;        // no overrun, less that 256
            }
            
            if(DEBUGGING) printf("Multiplied variable %d by %d giving %d\n",  varOffest, value, timesed);
        }
        
    }

//-----------------------------------------------
//
// process DIVISION command
//  DIV <variable1/value> <variable2>
//  divides variable1 by variable2 or value
//  result 'a' has mod of result
//
//-----------------------------------------------

    if (strcmp(command,"div") == 0 || strcmp(command,"DIV") == 0 ){
        uint8_t p1val = *param2;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            uint8_t value = varSpace[var];

            uint8_t varOffest = *param1;
            varOffest = lower(varOffest);              
            
            uint16_t divided = varSpace[varOffest] / value; // div two numbers
            uint16_t remainder = varSpace[varOffest] % value; // get mod
            
            varSpace[varOffest] = divided;
            varSpace[carryChar] = remainder;

            if(DEBUGGING) printf("Divided variable %d by %d giving %d mod %d\n",  varOffest, value, divided, remainder);
            


        } else { // else it is an int value
            uint8_t value = atoi(param2);
            uint8_t varOffest = *param1;
            varOffest = lower(varOffest);

            uint16_t divided = varSpace[varOffest] / value; // div two numbers
            uint16_t remainder = varSpace[varOffest] % value; // get mod
            
            varSpace[varOffest] = divided;
            varSpace[carryChar] = remainder;
            
            
            if(DEBUGGING) printf("Divided variable %d by %d giving %d mod %d\n",  varOffest, value, divided, remainder);
        }
        
    }

//-----------------------------------------------
//
// process SUB command
//  SUB <variable1/value> <variable2>
//  subtracts variable1 or value from variable2
//  result 'a' has 1 if overrun
//
//-----------------------------------------------

    if (strcmp(command,"sub") == 0 || strcmp(command,"SUB") == 0 ){
        uint8_t p1val = *param1;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            uint8_t value = varSpace[var];

            uint8_t varOffest = *param2;
            varOffest = lower(varOffest);
            int16_t subbed = varSpace[varOffest] - value; // add two numbers
            if(subbed < 0){
                subbed += 256;
                varSpace[varOffest] = subbed;
                varSpace[carryChar] = 1;        // overrun
            } else {
                varSpace[varOffest] = subbed;
                varSpace[carryChar] = 0;        // no overrun, less that 256
            }
            
            if(DEBUGGING) printf("SUB %d from var offset %d totalling %d\n", value, varOffest, subbed);


        } else { // else it is an int value
            uint8_t value = atoi(param1);
            uint8_t varOffest = *param2;
            varOffest = lower(varOffest);
            int16_t subbed = varSpace[varOffest] - value; // add two numbers
            if(subbed < 0){
                subbed += 256;
                varSpace[varOffest] = subbed;
                varSpace[carryChar] = 1;        // overrun
                if(DEBUGGING) printf("OVERRUN carry set in 'a'\n");
            } else {
                varSpace[varOffest] = subbed;
                varSpace[carryChar] = 0;        // no overrun, less that 256
                if(DEBUGGING) printf("IN BOUNDS\n");
            }
            
            if(DEBUGGING) printf("SUB %d from var offset %d totalling %d\n", value, varOffest, subbed);
        }

    }

//-----------------------------------------------
//
//  process COMPare command
//  COMP <variable1> <variable2/value>
//  subtracts variable2 or value from variable1
//  leaves variables intact
//  result '@' has difference
//  result '?' has 255 if neg, 0 if positive
// compare var with var, or var with number
//
//-----------------------------------------------

// param 1 is the base variable 
// param 2 is what we compare it with (num or var)

    if (strcmp(command,"comp") == 0 || strcmp(command,"COMP") == 0 ){
        uint8_t p2val = *param2;

        if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            uint8_t compVar = varSpace[var];

            uint8_t varOffest = *param1;
            varOffest = lower(varOffest);
            uint8_t baseVar = varSpace[varOffest];

            int16_t subbed = baseVar - compVar; // sub two numbers

             if(subbed == 0){
                varSpace[carryChar] = 0;
                varSpace[resultChar] = 0;       
                if(DEBUGGING) printf("compared the same\n");
            } else if(subbed < 0){
                subbed += 256;
                varSpace[resultChar] = subbed;
                varSpace[carryChar] = 255;        // overrun
            } else {
                varSpace[resultChar] = subbed;
                varSpace[carryChar] = 0;        // no overrun, less that 256
            }
            
            if(DEBUGGING) printf("COM %d with %d giving %d\n", baseVar, compVar, subbed);


        } else { // else it is an int value
            uint8_t compVar = atoi(param2);

            uint8_t varOffest = *param1;
            varOffest = lower(varOffest);
            uint8_t baseVar = varSpace[varOffest];

            int16_t subbed = baseVar - compVar; // sub two numbers

            if(subbed == 0){
                varSpace[carryChar] = 0;
                varSpace[resultChar] = 0;       
                if(DEBUGGING) printf("compared the same\n");
            } else if(subbed < 0){
                subbed += 256;
                varSpace[carryChar] = 255;
                varSpace[resultChar] = subbed;        // overrun
                if(DEBUGGING) printf("OVERRUN carry set in 'a'\n");
            } else {
                varSpace[carryChar] = 0;
                varSpace[resultChar] = subbed;        // no overrun, less that 256
                if(DEBUGGING) printf("IN BOUNDS\n");
            }
            
            if(DEBUGGING) printf("COM %d from with %d giving %d\n", baseVar, compVar, subbed);
        }

    }

//-----------------------------------------------
//
// process DELAY command
//  DELAY <variable1/value>
//  DELAY hundredths of a second
//
//-----------------------------------------------

    if (strcmp(command,"delay") == 0 || strcmp(command,"DELAY") == 0 ){
        uint8_t p1val = *param1;
        uint8_t value;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            value = varSpace[var];

            delay(value * 10);


        } else { // else it is an int value
            value = atoi(param1);
            delay(value * 10);
            
        }
        if(DEBUGGING) printf("DELAY for %d h/s\n", value);
    }
  
//-----------------------------------------------
//
//  process INK command
//  INK <variable1/value>
//
//-----------------------------------------------

    if (strcmp(command,"INK") == 0 || strcmp(command,"ink") == 0 ){
        uint8_t p1val = *param1;
        uint8_t value;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            value = varSpace[var];
        } else { // else it is an int value
            value = atoi(param1);           
        }

        if(DEBUGGING) printf("Set INK to %d\n", value);
        vdp_set_text_colour(value);
    }

//-----------------------------------------------
//
//  process PEN command
//  PEN <variable1/value>
//
//-----------------------------------------------

    if (strcmp(command,"PEN") == 0 || strcmp(command,"pen") == 0 ){
        uint8_t p1val = *param1;
        uint8_t value;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            value = varSpace[var];
        } else { // else it is an int value
            value = atoi(param1);           
        }

        if(DEBUGGING) printf("Set INK to %d\n", value);
        vdp_set_graphics_colour(0,value);
    }

//-----------------------------------------------
//
//  process PRINTNUM command
//  PRINTNUM <variable1/value> <format>
//
//-----------------------------------------------

    if (strcmp(command,"PRINTNUM") == 0 || strcmp(command,"printnum") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t value;
        
        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            value = varSpace[var];
            if(DEBUGGING) printf("Print number in offest %d which is %d\n", var, value);
        } else { // else it is an int value
            value = atoi(param1);     
            if(DEBUGGING) printf("Print number %d\n",  value);      
        }

        if (strcmp(param2,"HEX") == 0 || strcmp(param2,"hex") == 0 ){
                //printf("print hex\n");
                //printf("mode= %s\n",param2);
                printf("%02X",value);
        } else if (strcmp(param2,"BIN") == 0 || strcmp(param2,"bin") == 0 ){

                //printf("print binary\n");
                //printf("mode= %s\n",param2);

                printf(BYTE_TO_BINARY_PATTERN , BYTE_TO_BINARY(value));
        } else {
                //printf("print decimal\n");
                //printf("mode= %s\n",param2);
                printf("%d",value);
        }      
    }

//-----------------------------------------------
//
// process MODE command
//  MODE <value>
//
//-----------------------------------------------

    if (strcmp(command,"MODE") == 0 || strcmp(command,"mode") == 0 ){
        uint8_t p1val = *param1;
        uint8_t value;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            value = varSpace[var];
        } else { // else it is an int value
            value = atoi(param1);           
        }

        if(DEBUGGING) printf("Set screen MODE to %d\n", value);
        vdp_mode(value);
    }

//-----------------------------------------------
//
// process DEBUG command
//  DEBUG <value> (0 or 1)
//
//-----------------------------------------------

    if (strcmp(command,"DEBUG") == 0 || strcmp(command,"debug") == 0 ){
        uint8_t p1val = *param1;

        if(p1val == '0'){
            DEBUGGING = false;
        } else{
            DEBUGGING = true;
        }

    }

//-----------------------------------------------
//
// process CURSOR command
//  CURSOR <value> (0 or 1)
//
//-----------------------------------------------

    if (strcmp(command,"CURSOR") == 0 || strcmp(command,"cursor") == 0 ){
        uint8_t p1val = *param1;

        if(p1val == '0'){
            vdp_cursor_enable(false);
        } else {
            vdp_cursor_enable(true);
        }
    }


//-----------------------------------------------
//
// process EXIT command
//  EXIT
//
//-----------------------------------------------

    if (strcmp(command,"exit") == 0 || strcmp(command,"EXIT") == 0 ){
        running = false;
    }

//-----------------------------------------------
//
// process PRINT command
//  PRINT <string>
//
//-----------------------------------------------

    if (strcmp(command,"print") == 0 || strcmp(command,"PRINT") == 0 ){
        if(DEBUGGING) printf("Print text: %s", param1);
        mos_putstring(param1);
    }

//-----------------------------------------------
//
// process CR command
//  CR 
//
//-----------------------------------------------

    if (strcmp(command,"cr") == 0 || strcmp(command,"CR") == 0 ){
        if(DEBUGGING) printf("Printing CR");
        printf("\n");
    }

//-----------------------------------------------
//
// process SPC command
//  SPC 
//
//-----------------------------------------------

    if (strcmp(command,"spc") == 0 || strcmp(command,"SPC") == 0  || strcmp(command,"SPACE") == 0  || strcmp(command,"space") == 0 ){
        if(DEBUGGING) printf("Printing SPACE");
        printf(" ");
    }

//-----------------------------------------------
//
// process CLS command
//  CLS clear screen 
//
//-----------------------------------------------

    if (strcmp(command,"cls") == 0 || strcmp(command,"CLS") == 0 ){
        if(DEBUGGING) printf("Clearing screen");
        vdp_clear_screen();
    }
  
//-----------------------------------------------
//
// process TABTO command
//  TABTO <variable1/value> <variable2/value>
//  TABs to x,y
//
//-----------------------------------------------

    if (strcmp(command,"tabto") == 0 || strcmp(command,"TABTO") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t xpos;
        uint8_t ypos;
        
        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            xpos = varSpace[var];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            ypos = varSpace[var];
        } else { // else it is an int value
            ypos = atoi(param2);
        }

        if(DEBUGGING) printf("TAB to %d, %d \n", xpos, ypos);
        vdp_cursor_tab(xpos, ypos);
        }

    
//-----------------------------------------------
//
// process PLOT command
//  PLOT <variable1/value> <variable2/value>
//  PLOTS a dot at x,y
//
//-----------------------------------------------

    if (strcmp(command,"plot") == 0 || strcmp(command,"PLOT") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t xpos;
        uint8_t ypos;
        
        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            xpos = varSpace[var];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            ypos = varSpace[var];
        } else { // else it is an int value
            ypos = atoi(param2);
        }

        if(DEBUGGING) printf("PLOT to %d, %d \n", xpos, ypos);
        vdp_plot(4, xpos, ypos);
    }

    
//-----------------------------------------------
//
// process LINETO command
//  LINETO <variable1/value> <variable2/value>
//  LINETO  x,y
//
//-----------------------------------------------

    if (strcmp(command,"lineto") == 0 || strcmp(command,"LINETO") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t xpos;
        uint8_t ypos;
        
        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            xpos = varSpace[var];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            ypos = varSpace[var];
        } else { // else it is an int value
            ypos = atoi(param2);
        }

        if(DEBUGGING) printf("LINETO %d, %d \n", xpos, ypos);
            vdp_line_to(xpos, ypos);
        }

//-----------------------------------------------
//
// process MOVETO command
//  MOVETO <variable1/value> <variable2/value>
//  MOVETO  x,y
//
//-----------------------------------------------

    if (strcmp(command,"moveto") == 0 || strcmp(command,"MOVETO") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t xpos;
        uint8_t ypos;
        
        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            xpos = varSpace[var];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            ypos = varSpace[var];
        } else { // else it is an int value
            ypos = atoi(param2);
        }

        if(DEBUGGING) printf("LINETO %d, %d \n", xpos, ypos);
        vdp_move_to(xpos, ypos);
    }

    
//-----------------------------------------------
//
// process GOTO command
//  GOTO <variable1/value>

    if (strcmp(command,"goto") == 0 || strcmp(command,"GOTO") == 0 ){
        uint8_t p1val = *param1;
        uint8_t value;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            value = varSpace[var];

        } else { // else it is an int value
            value = atoi(param1);
        }
        currentLine = labels[value];

        if(DEBUGGING) printf("GOTO LABEL %d which is line %d \n", value, labels[value]);
    }

//-----------------------------------------------
//
// process CALL command
//  CALL <variable1/value>
//
//-----------------------------------------------

    if (strcmp(command,"call") == 0 || strcmp(command,"CALL") == 0 ){
        uint8_t p1val = *param1;
        uint8_t value;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            value = varSpace[var];

        } else { // else it is an int value
            value = atoi(param1);
        }

        returnStackIndex ++;
        returnStack[returnStackIndex] = currentLine;
   
        currentLine = labels[value];

        if(DEBUGGING) printf("CALL LABEL %d which is line %d return stack index is %d\n", value, labels[value],returnStackIndex);

    }

//-----------------------------------------------
//
// process CALLIF command
//  CALLIF <variable1/value> <variable2/value>
//
//-----------------------------------------------

    if (strcmp(command,"callif") == 0 || strcmp(command,"CALLIF") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t labelValue;
        uint8_t checkValue;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            labelValue = varSpace[var];

        } else { // else it is an int value
            labelValue = atoi(param1);
        }

        if(p2val == 39){ // must be a single quote, ie char
                char v = param2[2];
                checkValue = (uint8_t)v;
        } else  if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            checkValue = varSpace[var];

        } else { // else it is an int value
            checkValue = atoi(param2);
        }
        if(DEBUGGING) printf("CALLIF LABEL %d line %d checkValue %d a: %d\n", labelValue, labels[labelValue], checkValue, varSpace[resultChar]);

       

        if(varSpace[resultChar] == checkValue){

            returnStackIndex ++;
            returnStack[returnStackIndex] = currentLine;
            currentLine = labels[labelValue];

            if(DEBUGGING) printf("CALLIF LABEL %d \n", labelValue);
        }

        
    }

//-----------------------------------------------
//
// process RET
// return to most recent line on return stack
//
//-----------------------------------------------

    if (strcmp(command,"ret") == 0 || strcmp(command,"RET") == 0 ){
        uint16_t returnLine = returnStack[returnStackIndex];
 
        // for(uint8_t g=0;g<4;g++){
        //     printf("RET returnStack[%d]=%d\n",g,returnStack[g] );
        // }

        currentLine = returnLine;

        if(DEBUGGING) printf("RET to stack index %d which is line %d \n", returnStackIndex, returnLine);
        if(returnStackIndex >0) returnStackIndex--;

    }

//-----------------------------------------------
//
// process GOTOIF command
//  GOTOIF <variable1/value> <variable1/value>
//
//-----------------------------------------------

    if (strcmp(command,"gotoif") == 0 || strcmp(command,"GOTOIF") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t labelValue;
        uint8_t checkValue;

        if(p1val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param1;
            var = lower(var);
            labelValue = varSpace[var];

        } else { // else it is an int value
            labelValue = atoi(param1);
        }

        if(p2val > 57){ // must be a char, ie set to another variable
            uint8_t var = *param2;
            var = lower(var);
            checkValue = varSpace[var];

        } else { // else it is an int value
            checkValue = atoi(param2);
        }
        if(DEBUGGING) printf("GOTOIF LABEL %d line %d checkValue %d a: %d\n", labelValue, labels[labelValue], checkValue, varSpace[resultChar]);
        if(varSpace[resultChar] == checkValue){
            currentLine = labels[labelValue];
            if(DEBUGGING) printf("GOTOIF LABEL %d \n", labelValue);
        }
    }

//-----------------------------------------------
//
// process KEY command
//  KEY <variable1/value>
// check if a key is pressed, using ascii code
//
//-----------------------------------------------

    if (strcmp(command,"key") == 0 || strcmp(command,"KEY") == 0 ){

        uint8_t result = vdp_getKeyCode();
        varSpace[resultChar] = result;
        //printf("KEY pressed was %d \n", result );
        if(DEBUGGING) printf("KEY pressed was %d \n", result );
    }


//-----------------------------------------------
//
//  WAIT
// wait until any key press
//
//-----------------------------------------------

    if (strcmp(command,"wait") == 0 || strcmp(command,"WAIT") == 0 ){
        vdp_waitKeyUp();
        vdp_waitKeyDown();
    }

 
//-----------------------------------------------
//
//  PRINTVARS
// dump out all variables to screen
//
//-----------------------------------------------

    if (strcmp(command,"printvars") == 0 || strcmp(command,"PRINTVARS") == 0 ){
        for(uint8_t asc = 0; asc < 28; asc++){
            printf("%c = %d\n", (char)asc+63,  varSpace[asc]);
        }
    }

 
//-----------------------------------------------
//
//  INPUT <variable>
//  use fgets() and strtol() to grab number from user
//
//-----------------------------------------------

    if (strcmp(command,"input") == 0 || strcmp(command,"INPUT") == 0 ){
        
        char buffer[20];
        char *endptr;
        int val;

        fgets(buffer, sizeof(buffer), stdin);
        val = strtol(buffer, &endptr, 10);

        if(val >255) val =0;

        if(param1 != NULL){
            uint8_t varOffset = *param1;
            varOffset = lower(varOffset);
            varSpace[varOffset] = val;
        } else {
            varSpace[resultChar] = val;
        }
        if(DEBUGGING) printf("User entered %d for var %c \n", val, *param1);
    }
    




}       // end of command processing loop


//-----------------------------------------------
//
//  END OF PARSING
//
//-----------------------------------------------


// if we get a variable char, then check if going to be capital
// or lower case the reduce by the correct value to get a number 0-25

// now starting at ascii 63 = ? offset 0
//                       64 = @ offset 1
//                       65 = A so subtract 63 to get offset 2
//                       97 = a so subtract 95 to get offset 2
// varspace starts with 63
// resultChar = 1 ;
// carryChar = 0 ;


uint8_t lower(uint8_t num){
    if (num >94) {
        return num - 95;
    } else {
        return num - 63;
    };
}

//-----------------------------------------------

// Removes all occurrences of `sub` from a single string, in place.
void strip_substr(char *str, const char *sub) {
    size_t sub_len = strlen(sub);
    if (sub_len == 0) return;

    char *src = str, *dst = str;
    while (*src) {
        if (strncmp(src, sub, sub_len) == 0) {
            src += sub_len;   // skip the match
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

//-----------------------------------------------

// Applies strip_substr to every string in an array.
void strip_substr_array(char *arr[], size_t count, const char *sub) {
    for (size_t i = 0; i < count; i++) {
        strip_substr(arr[i], sub);
    }
}

//-----------------------------------------------
//replaces a char with another

void replace_char(char *str, char old_char, char new_char) {
    while (*str) {
        if (*str == old_char) {
            *str = new_char;
        }
        str++;
    }
}

//-----------------------------------------------
//
//  END
//
//-----------------------------------------------
