#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <agon/vdp.h>
#include <agon/mos.h>
#include <agon/timer.h>
#include <time.h>
#include "runtime.h"
#include <ctype.h>

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

    previousMode = getsysvar_scrMode(); // if user changes screen mode, we can reset after running
    vdp_logical_scr_dims(false);
    vdp_clear_screen();
    
    srand(time(NULL));  // seed with current time or the RND feature will be the same every time run

    if(fname[0] == 0){
        printf("File not saved yet!");
        delay(2000);
        return;
    } 

  
  // Create a file pointer variable to allow us to access the file
  FILE *file;
  
  // open the file...
  file = fopen(fname, "r");

  // If we've failed to open the file, exit with an error message and status, 
  if (file == NULL)
  {
    printf("Error opening file.\n");
  }

    // numLines will keep track of the number of lines read so far from the file
    uint16_t numLines = 0;

    while (numLines < MAX_LINES && fgets(codeData[numLines], MAX_LEN, file) != NULL) {
        size_t len = strlen(codeData[numLines]);
        toUpperCase(codeData[numLines]);  // convert all commands to UPPER case for quick processing
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
// if it fails, the space will be empty
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
        strip_substr(curLine, " TO");
        strip_substr(curLine, " =");
        strip_substr(curLine, " WITH");
        strip_substr(curLine, " FROM");
        strip_substr(curLine, " BY");
        strip_substr(curLine, " *");
        strip_substr(curLine, " +");
        strip_substr(curLine, " -");
        strip_substr(curLine, " UPTO");
        strip_substr(curLine, " TIMES");

        strcpy(codeData[labelCounter],curLine); // put cleaned line back into array

        lcommand = strtok(curLine, " ");
        lparam1 = strtok(NULL, " ");
        lparam2 = strtok(NULL, " ");
        uint8_t labelNum;

        if (strcmp(lcommand,"LABEL") == 0 ){
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

        // if line is empty, then don't waste time trying to match a command
        if(command != NULL){ 
            parseLine( command,  param1,  param2);
        } else {
            if(DEBUGGING) printf("Blank line\n");
        }

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

    if (strcmp(command,"SET") == 0 ){
        uint8_t value;                          // value to be set
        uint8_t varOffset = lower(*param1);     // variable to be set

        if(*param2 == 39){ // must be a single quote, ie char, 'k' for example will store ascii value of 'k'
            char v = param2[1];
            value = param2[1];

            if(DEBUGGING) printf("SET offset %d to ascii value %d\n", varOffset, value);


        } else if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];

            if(DEBUGGING) printf("SET offset %d to  %d\n", varOffset, value);

        } else if(strlen(param2) == 8){
            value = (uint8_t) strtol(param2, NULL, 2);

            if(DEBUGGING) printf("SET offset %d to ascii value %d\n", varOffset, value);

        }   else  { // else it is an int value
            value = atoi(param2);

            if(DEBUGGING) printf("SET offset %d to value %d\n", varOffset, value);
        }

        // store the variable value
        varSpace[varOffset] = value;

    }


//-----------------------------------------------
//
// process RND command
// RND <value/value>  <value/value> 
// RND <variable> <range> 
//
//-----------------------------------------------

    if (strcmp(command,"RND") == 0 ){
   
        uint8_t value;

        if(*param2 > 57){ // must be a char, ie set to another variable
             value = varSpace[lower(*param2)];
        } else { // else it is an int value
             value = atoi(param2);
        }
  
        uint8_t rndValue = rand() % value;
        varSpace[lower(*param1)] = rndValue;

        if(DEBUGGING) printf("SET offset %d to random value %d\n", lower(*param1), rndValue );   
    }
    
//-----------------------------------------------
//
//  process VDP command
//  VDP <value/variable>
//  sends a byte to VDP from a varaible, an int, or a char
//
//-----------------------------------------------

 if (strcmp(command,"VDP") == 0 ){
            uint8_t p1val = *param1;
            uint8_t leng = strlen(param1);
            uint8_t value;
            bool deb = DEBUGGING;
            DEBUGGING = false;  // cannot have debugging while sending VDP

            if(leng == 8){
                value = (uint8_t) strtol(param1, NULL, 2);
            }
            else if(p1val == 39){ // must be a single quote, ie char to be sent in [1]
                value = param1[1];
            } else if(p1val > 57){ // must be a char, ie set to another variable
                value = varSpace[lower(*param1)];
            } else { // else it is an int value
                value = atoi(param1);
            }
            putchar(value);

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

   if (strcmp(command,"VDPS") == 0 ){
            uint8_t offset;
            uint8_t count;

            // get offset
            if(*param1 > 57){ // must be a char, ie set to another variable
                offset = varSpace[lower(*param1)];
            } else { // else it is an int value
                offset = atoi(param1);
            }

            // get number of bytes to send
            if(*param2 > 57){ // must be a char, ie set to another variable
                count = varSpace[lower(*param2)];
            } else { // else it is an int value
                count = atoi(param2);
            }

            for(uint8_t sendloop = 0; sendloop < count; sendloop ++){
                putchar(dataSpace[offset + sendloop]);
            }

       }

//-----------------------------------------------
//
//  process SETDATA command
//  SETDATA <offset/variable> <value/variable>
//  stores a byte into data
//
//-----------------------------------------------

   if (strcmp(command,"SETDATA") == 0 ){

            uint8_t offset;
            uint8_t value;

            if(*param1 > 57){ // must be a char, ie set to another variable
                offset = varSpace[lower(*param1)];
            } else { // else it is an int value
                offset = atoi(param1);
            }

            if(*param2 == 39){ // must be a single quote, ie char
                char v = param2[1];
                value = v;
            } else if(*param2 > 57){ // must be a char, ie set to another variable
                value = varSpace[lower(*param2)];
            } else { // else it is an int value
                value = atoi(param2);
            }
            
            dataSpace[offset] = value;
            if(DEBUGGING) printf("Set data at offset %d to %d\n", offset, value);
 
       }

//-----------------------------------------------
//
//  process LOOP command
//  LOOP <value/variable>
//  loops <value> times
//
//-----------------------------------------------

   if (strcmp(command,"LOOP") == 0 ){

            uint8_t loops;

            loopReturnLine = currentLine;

            if(*param1 > 57){ // must be a char, ie set to another variable
                loops = varSpace[lower(*param1)];
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

   if (strcmp(command,"ENDLOOP") == 0 ){
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

   if (strcmp(command,"GETDATA") == 0 ){
            uint8_t p1val = *param1;
            uint8_t p2val = *param2;
            uint8_t offset;
            uint8_t var;

            if(*param1 > 57){ // must be a char, ie set to another variable
                offset = varSpace[lower(*param1)];
            } else { // else it is an int value
                offset = atoi(param1);
            }

            if(*param2 > 57){ // must be a char, ie set to another variable
                var = lower(*param2);
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

   if (strcmp(command,"BEEP") == 0 ){

            uint8_t freq;
            uint8_t time;

            if(*param1 > 57){ // must be a char, ie set to another variable
                freq = varSpace[lower(*param1)];
            } else { // else it is an int value
                freq = atoi(param1);
            }

            if(*param2 > 57){ // must be a char, ie set to another variable
                time = varSpace[lower(*param2)];
            } else { // else it is an int value
                time = atoi(param2);
            }
            if(DEBUGGING) printf("Beep at %d for %d \n", freq, time);
            vdp_audio_play_note(0, 127, freq * 10, time * 10);
       }


//-----------------------------------------------
//
// process ADD command
//  ADD <variable1/value> <variable2>
//  adds variable1 or value to variable2
//  result 'a' has 1 if overrun
//
//-----------------------------------------------

    if (strcmp(command,"ADD") == 0 ){

        if(*param1 > 57){ // must be a char, ie set to another variable
   
            uint8_t value = varSpace[lower(*param1)];

            uint8_t varOffest = lower(*param2);
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

            uint8_t varOffest = lower(*param2);
            uint16_t added = value + varSpace[varOffest]; // add two numbers

            if(added > 255){
                added -= 256;
                varSpace[varOffest] = added;
                varSpace[carryChar] = 1;        // overrun
                if(DEBUGGING) printf("OVERRUN carry set in 'a'\n");
            } else {
                varSpace[varOffest] = added;
                varSpace[carryChar] = 0;        // no overrun, less that 256
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

    if (strcmp(command,"MUL") == 0 ){
        uint8_t varOffset;
        uint8_t value;
        uint16_t timesed;
        uint8_t factor;

        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];
        } else { // else it is an int value
            value = atoi(param2);
        }
        varOffset = lower(*param1);
        timesed = value * varSpace[varOffset]; // mult two numbers
        if(timesed > 255){
            factor = timesed / 256;
            varSpace[varOffset] = timesed%256;
            varSpace[carryChar] = factor;        // overrun
        } else {
            varSpace[varOffset] = timesed;
            varSpace[carryChar] = 0;        // no overrun, less that 256
        }
    
        if(DEBUGGING) printf("Multiplied variable %d by %d giving %d\n",  varOffset, value, timesed);
        
    }

//-----------------------------------------------
//
// process DIVISION command
//  DIV <variable1/value> <variable2>
//  divides variable1 by variable2 or value
//  result 'a' has mod of result
//
//-----------------------------------------------

    if (strcmp(command,"DIV") == 0 ){
        uint8_t value;
        uint8_t varOffest;


        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];         
        } else { // else it is an int value
            value = atoi(param2);
        }
        varOffest = lower(*param1); 
        uint16_t divided = varSpace[varOffest] / value; // div two numbers
        uint16_t remainder = varSpace[varOffest] % value; // get mod
        
        varSpace[varOffest] = divided;
        varSpace[carryChar] = remainder;

        if(DEBUGGING) printf("Divided variable %d by %d giving %d mod %d\n",  varOffest, value, divided, remainder);
    }

//-----------------------------------------------
//
// process SUB command
//  SUB <variable1/value> <variable2>
//  subtracts variable1 or value from variable2
//  result 'a' has 1 if overrun
//
//-----------------------------------------------

    if (strcmp(command,"SUB") == 0 ){

        uint8_t value;
        uint8_t varOffest;
        int16_t subbed;

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);
        }

        varOffest = lower(*param2);
        subbed = varSpace[varOffest] - value; // sub two numbers

        if(subbed < 0){
            subbed += 256;
            varSpace[varOffest] = subbed;
            varSpace[carryChar] = 1;        // overrun
        } else {
            varSpace[varOffest] = subbed;
            varSpace[carryChar] = 0;        // no overrun, less that 256
        }
        
        if(DEBUGGING) printf("SUB %d from var offset %d totalling %d\n", value, varOffest, subbed);

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

    if (strcmp(command,"COMP") == 0 ){

        uint8_t compVar;
        uint8_t varOffest;
        uint8_t baseVar;

        if(*param2 > 57){ // must be a char, ie set to another variable
            compVar = varSpace[lower(*param2)];
        } else { // else it is an int value
            compVar = atoi(param2);
        }
        baseVar = varSpace[lower(*param1)];

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
    }

//-----------------------------------------------
//
// process DELAY command
//  DELAY <variable1/value>
//  DELAY hundredths of a second
//
//-----------------------------------------------

    if (strcmp(command,"DELAY") == 0 ){
        uint8_t value;
        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);
        }
        delay(value * 10);
        if(DEBUGGING) printf("DELAY for %d h/s\n", value);
    }
  
//-----------------------------------------------
//
//  process INK command
//  INK <variable1/value>
//
//-----------------------------------------------

    if (strcmp(command,"INK")  == 0 ){
  
        uint8_t value;

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
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

    if (strcmp(command,"PEN")  == 0 ){
        uint8_t value;

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
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

    if (strcmp(command,"PRINTNUM") == 0){
   
        uint8_t value;
        
        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
            if(DEBUGGING) printf("Print number in offest %d which is %d\n", lower(*param1), value);
        } else { // else it is an int value
            value = atoi(param1);     
            if(DEBUGGING) printf("Print number %d\n",  value);      
        }

        if (strcmp(param2,"HEX") == 0){
                printf("%02X",value);
        } else if (strcmp(param2,"BIN")  == 0 ){
                printf(BYTE_TO_BINARY_PATTERN , BYTE_TO_BINARY(value));
        } else {
                printf("%d",value);
        }      
    }

//-----------------------------------------------
//
// process MODE command
//  MODE <value>
//
//-----------------------------------------------

    if (strcmp(command,"MODE") == 0 ){
        uint8_t value;

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
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

    if (strcmp(command,"DEBUG") == 0 ){

        if(*param1 == '0'){
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

    if (strcmp(command,"CURSOR") == 0  ){

        if(*param1 == '0'){
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

    if (strcmp(command,"EXIT") == 0 ){
        running = false;
    }

//-----------------------------------------------
//
// process PRINT command
//  PRINT <string>
//
//-----------------------------------------------

    if (strcmp(command,"PRINT") == 0 ){
        if(DEBUGGING) printf("Print text: %s", param1);
        mos_putstring(param1);
    }

//-----------------------------------------------
//
// process CR command
//  CR 
//
//-----------------------------------------------

    if (strcmp(command,"CR") == 0 ){
        if(DEBUGGING) printf("Printing CR");
        printf("\n");
    }

//-----------------------------------------------
//
// process SPC command
//  SPC 
//
//-----------------------------------------------

    if (strcmp(command,"SPC") == 0 ){
        if(DEBUGGING) printf("Printing SPACE");
        printf(" ");
    }

//-----------------------------------------------
//
// process CLS command
//  CLS clear screen 
//
//-----------------------------------------------

    if (strcmp(command,"CLS") == 0 ){
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

    if (strcmp(command,"TABTO") == 0 ){
        uint8_t xpos;
        uint8_t ypos;
        
        if(*param1 > 57){ // must be a char, ie set to another variable
            xpos = varSpace[lower(*param1)];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(*param2 > 57){ // must be a char, ie set to another variable
            ypos = varSpace[lower(*param2)];
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

    if (strcmp(command,"PLOT") == 0 ){
        uint8_t p1val = *param1;
        uint8_t p2val = *param2;
        uint8_t xpos;
        uint8_t ypos;
        
        if(*param1 > 57){ // must be a char, ie set to another variable
            xpos = varSpace[lower(*param1)];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(*param2 > 57){ // must be a char, ie set to another variable
            ypos = varSpace[lower(*param2)];
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

    if (strcmp(command,"LINETO") == 0 ){
        uint8_t xpos;
        uint8_t ypos;
        
        if(*param1 > 57){ // must be a char, ie set to another variable
           xpos = varSpace[lower(*param1)];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(*param2 > 57){ // must be a char, ie set to another variable
            ypos = varSpace[lower(*param2)];
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

    if (strcmp(command,"MOVETO") == 0 ){
        uint8_t xpos;
        uint8_t ypos;
        
        if(*param1 > 57){ // must be a char, ie set to another variable
            xpos = varSpace[lower(*param1)];
        } else { // else it is an int value
            xpos = atoi(param1);
        }

        if(*param2 > 57){ // must be a char, ie set to another variable
            ypos = varSpace[lower(*param2)];
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

    if (strcmp(command,"GOTO") == 0 ){
        uint8_t value;

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
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

    if (strcmp(command,"CALL") == 0 ){
        uint8_t value;

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];

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

    if (strcmp(command,"CALLIF") == 0 ){

        uint8_t labelValue;
        uint8_t checkValue;

        if(*param1 > 57){ // must be a char, ie set to another variable
            labelValue = varSpace[lower(*param1)];
        } else { // else it is an int value
            labelValue = atoi(param1);
        }

        if(*param2 == 39){ // must be a single quote, ie char
                char v = param2[2];
                checkValue = (uint8_t)v;
        } else  if(*param2 > 57){ // must be a char, ie set to another variable
            checkValue = varSpace[lower(*param2)];;
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

    if (strcmp(command,"RET") == 0 ){
        uint16_t returnLine = returnStack[returnStackIndex];
 
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

    if (strcmp(command,"GOTOIF") == 0 ){
        uint8_t labelValue;
        uint8_t checkValue;

        if(*param1 > 57){ // must be a char, ie set to another variable
            labelValue = varSpace[lower(*param1)];

        } else { // else it is an int value
            labelValue = atoi(param1);
        }

        if(*param2 > 57){ // must be a char, ie set to another variable
            checkValue = varSpace[lower(*param2)];
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

    if (strcmp(command,"KEY") == 0 ){
        uint8_t result = vdp_getKeyCode();
        varSpace[resultChar] = result;
        if(DEBUGGING) printf("KEY pressed was %d \n", result );
    }


//-----------------------------------------------
//
//  WAIT
// wait until any key press
//
//-----------------------------------------------

    if (strcmp(command,"WAIT") == 0 ){
        vdp_waitKeyUp();
        vdp_waitKeyDown();
    }

 
//-----------------------------------------------
//
//  PRINTVARS
// dump out all variables to screen
//
//-----------------------------------------------

    if (strcmp(command,"PRINTVARS") == 0 ){
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

    if (strcmp(command,"INPUT") == 0 ){
        
        char buffer[20];
        char *endptr;
        int val;

        fgets(buffer, sizeof(buffer), stdin);
        val = strtol(buffer, &endptr, 10);

        if(val >255) val =0;

        if(param1 != NULL){
            varSpace[lower(*param1)] = val;
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
    // if (num >94) {
    //      return num - 95;
    // } else {
       return num - 63;
    // };
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
// convert to UPPER case

void toUpperCase(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

//-----------------------------------------------
//
//  END
//
//-----------------------------------------------
