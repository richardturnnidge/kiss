// this version has indexing of commands
// may still have bugs


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <agon/vdp.h>
#include <agon/mos.h>
#include <agon/timer.h>
#include <agon/gpio.h>
#include <time.h>
#include "runtime.h"
#include <ctype.h>
#include "agon/joystick.h"

// runtime part of code
#define MAX_LINES 2048                    // max number of lines in our program
#define MAX_LEN 20                      // max length of chars in each code line
#define MAX_LINE_LEN 256
#define MAX_TEXT_LABELS 64

bool doDebug = false;
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

uint16_t labelLine[MAX_TEXT_LABELS];
char labelName[MAX_TEXT_LABELS][8];

uint8_t textLabelCount;
char codeData[MAX_LINES][MAX_LEN];      // used to store each line of code
uint8_t varSpace[30];                   // used to store each of 26 variables a-z
bool running = false;                   // is code running
uint8_t commandIndex[MAX_LINES];        // used to store index of each line's command. Max 256 commands should be enough

int8_t sin_table[256];

// make sure order does not have short word after long word containing short word.
// eg, SET must come before SETDATA in list, else wrong one will be chosen

char *commandList[] = {
    "BLANK",
    "SET", 
    "RND",
    "COMP",
    "GETDATA",
    "SETDATA",

    "LABEL",
    "LOOP",
    "ENDLOOP",
    "GOTO",
    "GOTOIF",
    "GOTOIFNOT",
    "CALL",
    "CALLIF",
    "CALLIFNOT",
    "RET",
    "DELAY",
    "EXIT",

    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "SIN",
    "COS",

    "OR",
    "AND",
    "XOR",
    "NOT",
    "SHIFTLEFT",
    "SHIFTRIGHT",

    "CLS",
    "INK",
    "MODE",
    "TABTO",
    "PRINT",
    "PRINTNUM",
    "CR",
    "SPC",
    "CURSOR",
    "SWAP",

    "PEN",
    "PLOT",
    "MOVETO",
    "LINETO",
    "CIRCLE",
    

    "KEY",
    "JOY",
    "WAIT",
    "BEEP",
    "GETNUM",
    "INPUT",
    "OUTPUT",

    "VDP",
    "VDPS",

    "DEBUG",
    "PRINTVARS",
    "TIMER",
    "TIMERRET"
    
};


enum cmds {
    BLANK,
    SET, 
    RND,
    COMP,
    GETDATA,
    SETDATA,

    LABEL,
    LOOP,
    ENDLOOP,
    GOTO,
    GOTOIF,
    GOTOIFNOT,
    CALL,
    CALLIF,
    CALLIFNOT,
    RET,
    DELAY,
    EXIT,

    ADD,
    SUB,
    MUL,
    DIV,
    SIN,
    COS,

    OR,
    AND,
    XOR,
    NOT,
    SHIFTLEFT,
    SHIFTRIGHT,

    CLS,
    INK,
    MODE,
    TABTO,
    PRINT,
    PRINTNUM,
    CR,
    SPC,
    CURSOR,
    SWAP,

    PEN,
    PLOT,
    MOVETO,
    LINETO,
    CIRCLE,

    KEY,
    JOY,
    WAIT,
    BEEP,
    GETNUM,
    INPUT,
    OUTPUT,

    VDP,
    VDPS,

    DEBUG,
    PRINTVARS,
    TIMER,
    TIMERRET
    
};



// used for printing out binary numbers 0-255
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


// NEW version to useswitch/case to choose command
// variables we will use in parsing lines

uint8_t value;   
uint8_t varOffset;
uint8_t compVar;
uint8_t baseVar;
uint8_t offset;
uint8_t var;
uint8_t loops;
uint8_t labelValue;
uint8_t checkValue;
uint16_t returnLine;
uint16_t added;
uint16_t subbed;
uint16_t timesed;
uint8_t factor;
uint16_t divided;
uint16_t leftOver;
uint8_t xpos;
uint8_t ypos;
uint8_t freq;
uint8_t beeptime;
uint8_t count;
uint8_t leng;
uint8_t radius;
uint16_t joy;
char buffer[20];
char *endptr;
int val;

uint32_t lastTime;
bool timerRunning;
uint16_t timerReturnLine;
uint8_t timerFreq;
uint16_t timerLine;
uint16_t prevLine;

        uint8_t port;
        uint8_t pin;

//void runcode(text_buffer* aTextBuffer){
void runcode(char* fname){

    previousMode = getsysvar_scrMode(); // if user changes screen mode, we can reset after running
    vdp_logical_scr_dims(false);
    vdp_clear_screen();
    timerRunning = 0;
    DEBUGGING = doDebug;
    currentLine = 0;
    textLabelCount = 0;
    returnStackIndex = 0;
 

    build_sin_table();  // in case sin or cosine is needed

    srand(time(NULL));  // seed with current time or the RND feature will be the same every time run

    resetJoysticks(); // reset all joystick ports to their input state

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
        delay(2000);
    return;
    }

    // numLines will keep track of the number of lines read so far from the file
    uint16_t numLines = 0;

    char anotherbuffer[20];
    strcpy(anotherbuffer, codeData[numLines]);



    // try this version

    char line[MAX_LINE_LEN];
    int line_num = 0;

    while (numLines < MAX_LINES && read_line(file, line, sizeof(line))) {
        if(DEBUGGING) printf("Line %d: %s\n", line_num++, line);
        line[strlen(line)] = '\0';
        strcpy(codeData[numLines], line);
        codeData[numLines][19] = '\0';
        toUpperCase(codeData[numLines]);  // convert all commands to UPPER case for quick processing

        numLines++;
    }
    numLines++;
    // vdp_waitKeyUp();
    // vdp_waitKeyDown();




    // while (numLines < MAX_LINES && fgets(codeData[numLines], 50, file) != NULL) {
    //     size_t len = strlen(codeData[numLines]);

    //     codeData[numLines][19] = '\0';
    //     toUpperCase(codeData[numLines]);  // convert all commands to UPPER case for quick processing




    //     // Standard newline cleanup
    //     while (len > 0 && (codeData[numLines][len - 1] == '\n' || codeData[numLines][len - 1] == '\r')) {
    //         codeData[numLines][len - 1] = '\0';
    //         len--;
    //     }
    //     //printf("LINE %d: %s\n", numLines, codeData[numLines]);
    //     numLines++;
    // }

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


        char *stripped = strip_leading_spaces(curLine);

        strcpy(curLine,stripped);


        // need to tidy all this stripping up one day


        strcpy(codeData[labelCounter],curLine); // put cleaned line back into array


        

        lcommand = strtok(curLine, " ");
        lparam1 = strtok(NULL, " ");
        lparam2 = strtok(NULL, " ");
        uint8_t labelNum;


        // need to check here for text labels or numerical labels
        // TODO

        if (strcmp(lcommand,"LABEL") == 0 ){
            if (lparam1[0] < 65) { // it is a number
                uint8_t pval = *lparam1;
                labelNum = atoi(lparam1);
                labels[labelNum] = labelCounter;
                if(DEBUGGING){
                    printf("Got NUMBER label %d to go to line %d\n", labelNum, labelCounter);
                }
            } else { // it is text label
                strcpy(labelName[textLabelCount], lparam1);
                labelLine[textLabelCount] = labelCounter;

                if(DEBUGGING){
                    printf("Got TEXT label %s to go to line %d\n", labelName[textLabelCount], labelCounter);
                }          
                textLabelCount++;  // inc for next one
            }

        }




        // try to create command index for each code line
        // then use later instead of strng lookup for every line
        //printf("Line was>%s<",curLine);
        //char *stripped = strip_leading_spaces(curLine);
        //printf("Now>%s<\n",stripped);

        uint16_t cmdIndexFound = find_string(commandList, sizeof(commandList), curLine);

        if(DEBUGGING) printf("found index %d for line %d with cmd %s\n", cmdIndexFound, labelCounter, curLine);

        commandIndex[labelCounter] = cmdIndexFound;


    }

// reset control variables
  running = true;
  currentLine = 0;

// rest all varaibles
    for(uint8_t v = 0; v < 28; v++){
        varSpace[v] = 0;
    }

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
        char *param4;

        // split line into command and params
        command = strtok(src, " ");
        param1 = strtok(NULL, " ");
        param2 = strtok(NULL, " ");
        param3 = strtok(NULL, " ");
        param4 = strtok(NULL, " ");

        if(DEBUGGING) printf("Line %d: ",currentLine);

        currentLine ++;     // inc line now, in case it gets changed by a GOTO



        if (timerRunning){
            if (clock() - lastTime >= timerFreq) {   // 100 ms passed
                //printf("Tick! %d\n", clock());
    
                lastTime = clock();


                timerReturnLine = currentLine-1;
                currentLine = timerLine;

            }
        }


        // if line is empty, then don't waste time trying to match a command
        if(command != NULL){ 
            parseLine( command,  param1,  param2,  param3, param4);
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

void parseLine(char *command, char *param1, char *param2, char *param3, char *param4){

uint8_t lineCmd = commandIndex[currentLine - 1];



switch (lineCmd) {

//-----------------------------------------------
//-----------------------------------------------
//
// Dealing with Variables
//
// `SET <variable> <value/variable>`  
// `RND <variable> <value/variable>`  
// `COMP <variable> <value/variable>`  
// `GETDATA <offset/variable> <variable>`   
// `SETDATA <offset/variable> <value/variable>`  
//
//-----------------------------------------------
//-----------------------------------------------

    case BLANK:
    //case 50:
        // not used
            if(DEBUGGING) printf("Probably a comment: %s \n", command);
        break;
        

//-----------------------------------------------
//
// process SET command
// SET var to value/value
//
//-----------------------------------------------


    case SET:

        varOffset = lower(*param1);     // variable to be set

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

    break;


//-----------------------------------------------
//
// process RND command
// RND <value/value>  <value/value> 
// RND <variable> <range> 
//
//-----------------------------------------------

    case RND:
   


        if(*param2 > 57){ // must be a char, ie set to another variable
             value = varSpace[lower(*param2)];
        } else { // else it is an int value
             value = atoi(param2);
        }
  
        uint8_t rndValue = rand() % value;
        varSpace[lower(*param1)] = rndValue;

        if(DEBUGGING) printf("SET offset %d to random value %d\n", lower(*param1), rndValue );   
        break;

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

    case COMP:

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
        break;


//-----------------------------------------------
//
//  process GETDATA command
//  GETDATA <offset/variable> <value/variable>
//  gets a byte from data
//
//-----------------------------------------------

   case GETDATA:

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
        
        break;

    
    
//-----------------------------------------------
//
//  process SETDATA command
//  SETDATA <offset/variable> <value/variable>
//  stores a byte into data
//
//-----------------------------------------------

   case  SETDATA:

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
 
           break;

//-----------------------------------------------
//-----------------------------------------------
//
// Program Flow & logic
//
// `LABEL <number>`  
// `LOOP <value/variable>`  
// `ENDLOOP <value/variable>`  
// `GOTO <label ID>`  
// `GOTOIF <label ID>` <value expected in '@' value/variable>  
// `CALL <label ID>`  
// `CALLIF` <label ID> <value expected in '@' value/variable>  
// `RET`  
// `DELAY <variable1/value>`  
// `EXIT`  
//
//-----------------------------------------------
//-----------------------------------------------



//-----------------------------------------------
//
//  process LABEL command
//  This is dealt with before processing as list 
//  needs to be built
//
//-----------------------------------------------

   case LABEL:
        if(DEBUGGING) printf("LABEL %s\n", param1);
        break;

//-----------------------------------------------
//
//  process LOOP command
//  LOOP <value/variable>
//  loops <value> times
//
//-----------------------------------------------

   case LOOP:



        loopReturnLine = currentLine;

        if(*param1 > 57){ // must be a char, ie set to another variable
            loops = varSpace[lower(*param1)];
        } else { // else it is an int value
            loops = atoi(param1);
        }
        loopMax = loops-1;
        if(DEBUGGING) printf("Starting loop %d times\n", loopMax);
        break;

//-----------------------------------------------
//
//  process ENDLOOP command
//  ENDLOOP <value/variable>
//
//-----------------------------------------------

   case ENDLOOP:
        if(loopMax > 0){
            // still running
            if(DEBUGGING) printf("Looping back\n");
            loopMax--;
            currentLine = loopReturnLine;
        } else {
            // end of looping
            if(DEBUGGING) printf("End of loop\n");
        }
        break;


//-----------------------------------------------
//
// process GOTO command
//  GOTO <variable1/value>
//
//-----------------------------------------------

   case GOTO:


        if(*param1 > 57){ // must be a char, ie set to another variable
            currentLine = getLabelLine(param1);
            if(DEBUGGING) printf("GOTO TEXT LABEL %s which is line %d \n", param1, currentLine);

        } else { // else it is an int value
            value = atoi(param1);
            currentLine = labels[value];
             if(DEBUGGING) printf("GOTO NUMBER LABEL %d which is line %d \n", value, labels[value]);
        }
        

       
        break;



//-----------------------------------------------
//
// process GOTOIF command
//  GOTOIF <variable1/value> <variable1/value>
//
//-----------------------------------------------

   case GOTOIF:

        if(*param1 > 57){ // must be a char, ie set to another variable
            currentLine = getLabelLine(param1);
            if(DEBUGGING) printf("GOTO TEXT LABEL %s which is line %d \n", param1, currentLine);

        } else { // else it is an int value
            value = atoi(param1);
            currentLine = labels[value];
             if(DEBUGGING) printf("GOTO NUMBER LABEL %d which is line %d \n", value, labels[value]);
        }

        // if(*param1 > 57){ // must be a char, ie set to another variable
        //     labelValue = varSpace[lower(*param1)];

        // } else { // else it is an int value
        //     labelValue = atoi(param1);
        // }

        if(*param2 > 57){ // must be a char, ie set to another variable
            checkValue = varSpace[lower(*param2)];
        } else { // else it is an int value
            checkValue = atoi(param2);
        }

        if(*param3 == 39){ // must be a single quote, ie char
            char v = param3[2];
            value = (uint8_t)v;
        } else  if(*param3 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param3)];;
        } else { // else it is an int value
            value = atoi(param3);
        }

        if(DEBUGGING) printf("GOTOIF LABEL %d line %d checkValue %d with: %d\n", labelValue, labels[labelValue], checkValue, value);
        if(value == checkValue){
            //currentLine = labels[labelValue];
            if(DEBUGGING) printf("GOTOIF LABEL %d \n", labelValue);
        }
        break;
//-----------------------------------------------
//
// process GOTOIFNOT command
//  GOTOIFNOT <variable1/value> <variable1/value>
//
//-----------------------------------------------

   case GOTOIFNOT:


        // if(*param1 > 57){ // must be a char, ie set to another variable
        //     labelValue = varSpace[lower(*param1)];

        // } else { // else it is an int value
        //     labelValue = atoi(param1);
        // }

        if(*param1 > 57){ // must be a char, ie set to another variable
            currentLine = getLabelLine(param1);
            if(DEBUGGING) printf("GOTO TEXT LABEL %s which is line %d \n", param1, currentLine);

        } else { // else it is an int value
            value = atoi(param1);
            currentLine = labels[value];
             if(DEBUGGING) printf("GOTO NUMBER LABEL %d which is line %d \n", value, labels[value]);
        }

        if(*param2 > 57){ // must be a char, ie set to another variable
            checkValue = varSpace[lower(*param2)];
        } else { // else it is an int value
            checkValue = atoi(param2);
        }

        if(*param3 == 39){ // must be a single quote, ie char
            char v = param3[2];
            value = (uint8_t)v;
        } else  if(*param3 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param3)];;
        } else { // else it is an int value
            value = atoi(param3);
        }

        if(DEBUGGING) printf("GOTOIF LABEL %d line %d checkValue %d with: %d\n", labelValue, labels[labelValue], checkValue, value);
        if(value != checkValue){
            //currentLine = labels[labelValue];
            if(DEBUGGING) printf("GOTOIFNOT LABEL %d \n", labelValue);
        }
        break;

//-----------------------------------------------
//
// process CALL command
//  CALL <variable1/value>
//
//-----------------------------------------------

    case CALL:
        prevLine = currentLine;

        // if(*param1 > 57){ // must be a char, ie set to another variable
        //     value = varSpace[lower(*param1)];

        // } else { // else it is an int value:
        //     value = atoi(param1);
        // }

        if(*param1 > 57){ // must be a char, ie set to another variable
            currentLine = getLabelLine(param1);
            if(DEBUGGING) printf("CALL TEXT LABEL %s which is line %d \n", param1, currentLine);

        } else { // else it is an int value
            value = atoi(param1);
            currentLine = labels[value];
             if(DEBUGGING) printf("CALL NUMBER LABEL %d which is line %d \n", value, labels[value]);
        }

        returnStackIndex ++;
        returnStack[returnStackIndex] = prevLine;
   
        //currentLine = labels[value];

        if(DEBUGGING) printf("CALL LABEL %s which is line %d return stack index is %d\n", param1, currentLine,returnStackIndex-1);

        break;


//-----------------------------------------------
//
// process CALLIF command
//  CALLIF <variable1/value> <variable2/value>
//
//-----------------------------------------------

    case CALLIF:


        prevLine = currentLine;
        // if(*param1 > 57){ // must be a char, ie set to another variable
        //     labelValue = varSpace[lower(*param1)];
        // } else { // else it is an int value
        //     labelValue = atoi(param1);
        // }

        if(*param1 > 57){ // must be a char, ie set to another variable
            currentLine = getLabelLine(param1);
            if(DEBUGGING) printf("CALLIF TEXT LABEL %s which is line %d \n", param1, currentLine);

        } else { // else it is an int value
            value = atoi(param1);
            currentLine = labels[value];
             if(DEBUGGING) printf("CALLIF NUMBER LABEL %d which is line %d \n", value, labels[value]);
        }

        if(*param2 == 39){ // must be a single quote, ie char
            char v = param2[2];
            checkValue = (uint8_t)v;
        } else  if(*param2 > 57){ // must be a char, ie set to another variable
            checkValue = varSpace[lower(*param2)];;
        } else { // else it is an int value
            checkValue = atoi(param2);
        }

        if(*param3 == 39){ // must be a single quote, ie char
                char v = param3[2];
                value = (uint8_t)v;
        } else  if(*param3 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param3)];;
        } else { // else it is an int value
            value = atoi(param3);
        }



        if(DEBUGGING) printf("CALLIF LABEL from %d to line %d IF checkValue: %d = var: %d\n", prevLine, currentLine, checkValue, value);

        if(value == checkValue){

            returnStackIndex ++;
            returnStack[returnStackIndex] = prevLine;
            //currentLine = labels[labelValue];

            if(DEBUGGING) printf("Doing CALLIF to LABEL index%d \n", labelValue);
        }
        else{
            currentLine = prevLine;
            if(DEBUGGING) printf("CALLIF did not match so not calling\n");
        }

        
        break;


//-----------------------------------------------
//
// process CALLIFNOT command
//  CALLIFNOT <variable1/value> <variable2/value>
//
//-----------------------------------------------

    case CALLIFNOT:

        prevLine = currentLine;

        // if(*param1 > 57){ // must be a char, ie set to another variable
        //     labelValue = varSpace[lower(*param1)];
        // } else { // else it is an int value
        //     labelValue = atoi(param1);
        // }

        if(*param1 > 57){ // must be a char, ie set to another variable
            currentLine = getLabelLine(param1);
            if(DEBUGGING) printf("CALL TEXT LABEL %s which is line %d \n", param1, currentLine);

        } else { // else it is an int value
            value = atoi(param1);
            currentLine = labels[value];
             if(DEBUGGING) printf("CALL NUMBER LABEL %d which is line %d \n", value, labels[value]);
        }

        if(*param2 == 39){ // must be a single quote, ie char
                char v = param2[2];
                checkValue = (uint8_t)v;
        } else  if(*param2 > 57){ // must be a char, ie set to another variable
            checkValue = varSpace[lower(*param2)];;
        } else { // else it is an int value
            checkValue = atoi(param2);
        }

        if(*param3 == 39){ // must be a single quote, ie char
                char v = param3[2];
                value = (uint8_t)v;
        } else  if(*param3 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param3)];;
        } else { // else it is an int value
            value = atoi(param3);
        }



        if(DEBUGGING) printf("CALLIFNOT LABEL from %d to line %d IF checkValue: %d = var: %d\n", prevLine, currentLine, checkValue, value);

        if(value != checkValue){

            returnStackIndex ++;
            returnStack[returnStackIndex] = prevLine;
            //currentLine = labels[labelValue];

            if(DEBUGGING) printf("CALLIFNOT LABEL line %d \n", currentLine);
        }
        else{
            if(DEBUGGING) printf("CALLIFNOT matched so not calling\n");
            currentLine = prevLine;
        }

        
        break;


//-----------------------------------------------
//
// process RET
// return to most recent line on return stack
//
//-----------------------------------------------

    case RET:
        
        returnLine = returnStack[returnStackIndex];

        currentLine = returnLine;

        if(DEBUGGING) printf("RET to stack index %d which is line %d \n", returnStackIndex, returnLine);
        if(returnStackIndex >0) returnStackIndex--;


    break;

//-----------------------------------------------
//
// process TIMERRET
// return from timer routine
//
//-----------------------------------------------

    case TIMERRET:
        
        if(timerRunning == 1) // timer is running, so need special case return
            {
                    currentLine = timerReturnLine;
                    if(DEBUGGING) printf("returning from timer function\n");
            } 

    break;


//-----------------------------------------------
//
// process DELAY command
//  DELAY <variable1/value>
//  DELAY hundredths of a second
//
//-----------------------------------------------

    case DELAY:

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);
        }
        delay(value * 10);
        if(DEBUGGING) printf("DELAY for %d h/s\n", value);
        break;
  

//-----------------------------------------------
//
// process EXIT command
//  EXIT
//
//-----------------------------------------------

    case EXIT:
        if(DEBUGGING) printf("EXITing\n");
        running = false;
        break;


//-----------------------------------------------
//-----------------------------------------------
//
// Maths Operations
//
// `ADD <variable1/value> <variable2>`  
// `SUB <variable1/value> <variable2>`  
// `MUL <variable1/value> <variable2>`  
// `DIV <variable1/value> <variable2>`  
// `SIN <variable2>`  
// `COS <variable2>`  
//
//-----------------------------------------------
//-----------------------------------------------


//-----------------------------------------------
//
// process ADD command
//  ADD <variable1/value> <variable2>
//  adds variable1 or value to variable2
//  result 'a' has 1 if overrun
//
//-----------------------------------------------

    case ADD:

        if(*param1 > 57){ // must be a char, ie set to another variable
   
            value = varSpace[lower(*param1)];

            varOffset = lower(*param2);
             added = value + varSpace[varOffset]; // add two numbers

            if(added > 255){
                added -= 256;
                varSpace[varOffset] = added;
                varSpace[carryChar] = 1;        // overrun
            } else {
                varSpace[varOffset] = added;
                varSpace[carryChar] = 0;        // no overrun, less that 256
            }
            
            if(DEBUGGING) printf("ADD %d to var offset %d totalling %d\n", value, varOffset, added);


        } else { // else it is an int value
            value = atoi(param1);

            varOffset = lower(*param2);
             added = value + varSpace[varOffset]; // add two numbers

            if(added > 255){
                added -= 256;
                varSpace[varOffset] = added;
                varSpace[carryChar] = 1;        // overrun
                if(DEBUGGING) printf("OVERRUN carry set in 'a'\n");
            } else {
                varSpace[varOffset] = added;
                varSpace[carryChar] = 0;        // no overrun, less that 256
            }
            
            if(DEBUGGING) printf("ADD %d to var offset %d totalling %d\n", value, varOffset, added);
        }

        break;

//-----------------------------------------------
//
// process SUB command
//  SUB <variable1/value> <variable2>
//  subtracts variable1 or value from variable2
//  result 'a' has 1 if overrun
//
//-----------------------------------------------

    case SUB:



        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);
        }

        varOffset = lower(*param2);
        subbed = varSpace[varOffset] - value; // sub two numbers

        if(subbed < 0){
            subbed += 256;
            varSpace[varOffset] = subbed;
            varSpace[carryChar] = 1;        // overrun
        } else {
            varSpace[varOffset] = subbed;
            varSpace[carryChar] = 0;        // no overrun, less that 256
        }
        
        if(DEBUGGING) printf("SUB %d from var offset %d totalling %d\n", value, varOffset, subbed);

        break;


//-----------------------------------------------
//
// process MULTIPLY command
//  MUL <variable1/value> <variable2>
//  adds variable1 or value to variable2
//  result 'a' has x256 overrun

    case MUL:



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
        
        break;

//-----------------------------------------------
//
// process DIVISION command
//  DIV <variable1/value> <variable2>
//  divides variable1 by variable2 or value
//  result 'a' has mod of result
//
//-----------------------------------------------

    case  DIV:

        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];         
        } else { // else it is an int value
            value = atoi(param2);
        }
        varOffset = lower(*param1); 
         divided = varSpace[varOffset] / value; // div two numbers
         leftOver = varSpace[varOffset] % value; // get mod
        
        varSpace[varOffset] = divided;
        varSpace[carryChar] = leftOver;

        if(DEBUGGING) printf("Divided variable %d by %d giving %d mod %d\n",  varOffset, value, divided, leftOver);
        break;

//-----------------------------------------------
//
// process SIN command
//  SIN <variable2>
//  puts SIN of var into var
//  carry set if negative
//
//-----------------------------------------------

    case  SIN:

        varOffset = lower(*param1); 

        value = varSpace[varOffset];
        int8_t s = sin_table[value]; // sin
        //printf("Sin was %d: ", s);
        if (s < 0){
            //printf("Sin was LESS than 0- ");
            varSpace[carryChar] = 1;
            varSpace[resultChar] = 128 - abs(s);
        }else{
            //printf("Sin was MORE than 0- ");
            varSpace[carryChar] = 0;
            varSpace[resultChar] = 128 + abs(s);
            
        }
        
        varSpace[varOffset] = abs(s);
        
        if(DEBUGGING) printf("SIN of %d by %d giving \n",  value, s);
        break;


//-----------------------------------------------
//
// process COS command
//  COS <variable2>
//  puts COS of var into var
//  carry set if negative
//
//-----------------------------------------------

    case  COS:

        varOffset = lower(*param1); 
        
        value = varSpace[varOffset];
        value += 64;
        int8_t c = sin_table[value]; // div two numbers
        //printf("Cos was %d: varoffset was: %d value was: %d - ", c, varOffset, value);
        if (c <0){
            varSpace[carryChar] = 1;
            varSpace[resultChar] = 128 - abs(c);
        }else{
            varSpace[carryChar] = 0;
            varSpace[resultChar] = 128 + abs(c);
        }

        varSpace[varOffset] = abs(c);

        if(DEBUGGING) printf("COS of %d by %d giving \n",  value, c);
        break;


//-----------------------------------------------
//-----------------------------------------------
//
// Binary Operations
//
// `OR <variable1/value> <variable2>`  
// `AND <variable1/value> <variable2>`  
// `XOR <variable1/value> <variable2>`  
// `NOT <variable1/value>`  
//
//-----------------------------------------------
//-----------------------------------------------


//-----------------------------------------------
//
// process OR command
//  OR <variable1> <variable2/value>
//
//-----------------------------------------------

   case  OR:

        varOffset = lower(*param1); 

        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];         
        } else if(strlen(param2) == 8){         // ust be binary
            value = (uint8_t) strtol(param2, NULL, 2);
        } else { // else it is an int value
            value = atoi(param2);
        }

        varSpace[varOffset] = varSpace[varOffset] | value;

        if(DEBUGGING) printf("OR'd variable %d with %d\n",  *param1, value);
        break;


//-----------------------------------------------
//
// process AND command
//  AND <variable1> <variable2/value>
//
//-----------------------------------------------

   case  AND:

        varOffset = lower(*param1); 

        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];         
        } else if(strlen(param2) == 8){         // ust be binary
            value = (uint8_t) strtol(param2, NULL, 2);
        } else { // else it is an int value
            value = atoi(param2);
        }

        varSpace[varOffset] = varSpace[varOffset] & value;

        if(DEBUGGING) printf("AND'd variable %d with %d\n",  *param1, value);
        break;



//-----------------------------------------------
//
// process XOR command
//  XOR <variable1> <variable2/value>
//
//-----------------------------------------------

   case  XOR:

        varOffset = lower(*param1); 

        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];         
        } else if(strlen(param2) == 8){         // ust be binary
            value = (uint8_t) strtol(param2, NULL, 2);
        } else { // else it is an int value
            value = atoi(param2);
        }

        varSpace[varOffset] = varSpace[varOffset] ^ value;

        if(DEBUGGING) printf("XOR'd variable %d with %d\n",  *param1, value);
        break;


//-----------------------------------------------
//
// process NOT command
//  NOT <variable1> 
//
//-----------------------------------------------

   case  NOT:

        varOffset = lower(*param1); 

        varSpace[varOffset] = ~varSpace[varOffset];

        if(DEBUGGING) printf("NOT'd variable %d\n",  *param1);
        break;


//-----------------------------------------------
//
// process SHIFTLEFT command
//  SHIFTLEFT <variable1> 
//
//-----------------------------------------------

   case  SHIFTLEFT:

        varOffset = lower(*param1); 

        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];         
        } else if(strlen(param2) == 8){         // ust be binary
            value = (uint8_t) strtol(param2, NULL, 2);
        } else { // else it is an int value
            value = atoi(param2);
        }
        varSpace[varOffset] = varSpace[varOffset] << value;

        if(DEBUGGING) printf("SHIFT LEFT variable %d by %d bits\n",  *param1, value);
        break;


//-----------------------------------------------
//
// process SHIFTRIGHT command
//  SHIFTRIGHT <variable1> 
//
//-----------------------------------------------

   case  SHIFTRIGHT:

        varOffset = lower(*param1); 

        if(*param2 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param2)];         
        } else if(strlen(param2) == 8){         // ust be binary
            value = (uint8_t) strtol(param2, NULL, 2);
        } else { // else it is an int value
            value = atoi(param2);
        }
        varSpace[varOffset] = varSpace[varOffset] >> value;

        if(DEBUGGING) printf("SHIFT RIGHT variable %d by %d bits\n",  *param1, value);
        break;;



//-----------------------------------------------
//-----------------------------------------------
//
// Text and Display
//
// `CLS`  
// `INK <value/variable>`  
// `MODE <value/variable>`  
// `TABTO <value/variable> <value/variable>`  
// `PRINT <string>`  
// `PRINTNUM` <value/variable> <optional format>  
// `CR`  
// `SPC`  
// `CURSOR <value>`  
//
//-----------------------------------------------
//-----------------------------------------------


//-----------------------------------------------
//
// process CLS command
//  CLS clear screen 
//
//-----------------------------------------------

    case CLS:
        if(DEBUGGING) printf("Clearing screen");
        vdp_clear_screen();
        break;
  

//-----------------------------------------------
//
//  process INK command
//  INK <variable1/value>
//
//-----------------------------------------------

    case INK:
  


        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);           
        }

        if(DEBUGGING) printf("Set INK to %d\n", value);
        vdp_set_text_colour(value);
        break;


//-----------------------------------------------
//
// process MODE command
//  MODE <value>
//
//-----------------------------------------------

    case MODE:


        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);           
        }

        if(DEBUGGING) printf("Set screen MODE to %d\n", value);
        vdp_mode(value);
        break;


//-----------------------------------------------
//
// process TABTO command
//  TABTO <variable1/value> <variable2/value>
//  TABs to x,y
//
//-----------------------------------------------

    case TABTO:

        
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
        break;

//-----------------------------------------------
//
// process PRINT command
//  PRINT <string>
//
//-----------------------------------------------

    case PRINT:
        if(DEBUGGING) printf("Print text: %s", param1);
        mos_putstring(param1);
        break;


//-----------------------------------------------
//
//  process PRINTNUM command
//  PRINTNUM <variable1/value> <format>
//
//-----------------------------------------------

    case PRINTNUM:

        
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
        break;



//-----------------------------------------------
//
// process CR command
//  CR 
//
//-----------------------------------------------

    case CR:
        if(DEBUGGING) printf("Printing CR");
        //printf("\n");
        putchar(10);
        break;

//-----------------------------------------------
//
// process SPC command
//  SPC 
//
//-----------------------------------------------

    case SPC:
        if(DEBUGGING) printf("Printing SPACE");
        putchar(32);
        //printf(" ");
        break;



//-----------------------------------------------
//
// process CURSOR command
//  CURSOR <value> (0 or 1)
//
//-----------------------------------------------

    case CURSOR:

        if(*param1 == '0'){
            vdp_cursor_enable(false);
        } else {
            vdp_cursor_enable(true);
        }
        break;

//-----------------------------------------------
//
// process SWAP command
//  SWAP (screen modes>128 or vblank wait)
//
//-----------------------------------------------

    case SWAP:

        vdp_swap();
        break;


//-----------------------------------------------

//-----------------------------------------------
//
// Plotting Line Graphics
//
// `PEN <value/variable>`  
// `PLOT <value/variable> <value/variable>`  
// `MOVETO <value/variable> <value/variable>`  
// `LINETO <value/variable> <value/variable>` 
// `CIRCLE <value/variable> <value/variable>` 
//
//-----------------------------------------------
//-----------------------------------------------


//-----------------------------------------------
//
//  process PEN command
//  PEN <variable1/value>
//
//-----------------------------------------------

    case PEN:

        if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);           
        }

        if(DEBUGGING) printf("Set PLOT PEN to %d\n", value);
        vdp_set_graphics_colour(0,value);
        break;


//-----------------------------------------------
//
// process PLOT command
//  PLOT <variable1/value> <variable2/value>
//  PLOTS a dot at x,y
//
//-----------------------------------------------

    case PLOT:

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

        if(*param3 == 49){ // set x and y to upper bytes
            if(DEBUGGING) printf("PLOT to extended %d, %d \n", xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]));
            vdp_plot(69, xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]));
        } else { 
            if(DEBUGGING) printf("PLOT to %d, %d \n", xpos, ypos);
            vdp_plot(69, xpos, ypos);
        }

        break;

//-----------------------------------------------
//
// process MOVETO command
//  MOVETO <variable1/value> <variable2/value>
//  MOVETO  x,y
//
//-----------------------------------------------

    case MOVETO:
        
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

        if(*param3 == 49){ // set x and y to upper bytes
            if(DEBUGGING) printf("MOVETO to extended %d, %d \n", xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]));
            vdp_move_to( xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]));
        } else { 
            if(DEBUGGING) printf("MOVETO %d, %d \n", xpos, ypos);
            vdp_move_to(xpos, ypos);
        }

        // if(DEBUGGING) printf("LINETO %d, %d \n", xpos, ypos);
        // vdp_move_to(xpos, ypos);
        break;

    
//-----------------------------------------------
//
// process LINETO command
//  LINETO <variable1/value> <variable2/value>
//  LINETO  x,y
//
//-----------------------------------------------

    case LINETO:

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

        if(*param3 == 49){ // set x and y to upper bytes
            if(DEBUGGING) printf("LINETO to extended %d, %d \n", xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]));
            vdp_line_to( xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]));
        } else { 
            if(DEBUGGING) printf("LINETO %d, %d \n", xpos, ypos);
            vdp_line_to(xpos, ypos);
        }

        // if(DEBUGGING) printf("LINETO %d, %d \n", xpos, ypos);
        // vdp_line_to(xpos, ypos);
        break;


//-----------------------------------------------
//
// process CIRCLE command
//  CIRCLE <variable1/value> <variable2/value> <variable2/value>
//  CIRCLE  x,y
//  radius is taken from @ x
//
//-----------------------------------------------

    case CIRCLE:

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

        if(*param3 > 57){ // must be a char, ie set to another variable
            radius = varSpace[lower(*param3)];
        } else { // else it is an int value
            radius = atoi(param3);
        }

        

        if(*param4 == 49){ // set x and y to upper bytes
            if(DEBUGGING) printf("CIRCLE to extended %d, %d \n", xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]));
            vdp_filled_circle( xpos + (256 * varSpace[25]), ypos + (256 * varSpace[26]), radius); 
        } else { 
            if(DEBUGGING) printf("CIRCLE at %d, %d with radius %d\n", xpos, ypos, radius);
            vdp_filled_circle(xpos, ypos,radius);
        }

        // vdp_filled_circle(xpos, ypos,radius);
        break;


//-----------------------------------------------
//-----------------------------------------------
//
// Interaction with user
//
// `KEY`  
// `JOY`  
// `WAIT`  
// `BEEP <offset/variable> <value/variable>`  
// `INPUT <variable>`
//
//-----------------------------------------------



//-----------------------------------------------
//
// process KEY command
//  KEY <variable1/value>
// check if a key is pressed, using ascii code
//
//-----------------------------------------------

    case KEY:
        value = vdp_getKeyCode();
        varSpace[resultChar] = value;
        if(DEBUGGING) printf("KEY pressed was %d \n", value );
        break;


//-----------------------------------------------
//
// process JOY command
//  JOY <variable1> <variable2>
// returns joystick status into var1 and var2
//
//-----------------------------------------------

    case JOY:
        joy = getJoystickButtons(); // get the state of all buttons

        varSpace[lower(*param1)] = joy / 256;
        varSpace[lower(*param2)] = joy % 256;
        if(DEBUGGING) printf("JOY: " );
        if(DEBUGGING) printf(BYTE_TO_BINARY_PATTERN , BYTE_TO_BINARY(joy) );
        break;


//-----------------------------------------------
//
//  WAIT
// wait until any key press
//
//-----------------------------------------------

    case WAIT:
    if(DEBUGGING) printf("WAITing\n");
        vdp_waitKeyUp();
        vdp_waitKeyDown();
        break;

//-----------------------------------------------
//
//  process BEEP command
//  BEEP <offset/variable> <value/variable>
//  BEEP freq time
//
//-----------------------------------------------

   case BEEP:

            if(*param1 > 57){ // must be a char, ie set to another variable
                freq = varSpace[lower(*param1)];
            } else { // else it is an int value
                freq = atoi(param1);
            }

            if(*param2 > 57){ // must be a char, ie set to another variable
                beeptime = varSpace[lower(*param2)];
            } else { // else it is an int value
                beeptime = atoi(param2);
            }
            if(DEBUGGING) printf("Beep at %d for %d \n", freq, beeptime);
            vdp_audio_play_note(0, 127, freq * 10, beeptime * 10);
           break;

//-----------------------------------------------
//
//  GETNUM <variable>
//  use fgets() and strtol() to grab number from user
//
//-----------------------------------------------

    case GETNUM:

        fgets(buffer, sizeof(buffer), stdin);
        val = strtol(buffer, &endptr, 10);

        if(val >255) val =0;

        if(param1 != NULL){
            varSpace[lower(*param1)] = val;
        } else {
            varSpace[resultChar] = val;
        }
        if(DEBUGGING) printf("User entered %d for var %c \n", val, *param1);
        break;
    

//-----------------------------------------------
//
//  process INPUT command
//  INPUT <port> <PIN> <variable>
//  reads gpio port
//
//-----------------------------------------------

   case INPUT:

            if(param1[0] == 'B') port = PORTB;      // select port
            if(param1[0] == 'C') port = PORTC;
            if(param1[0] == 'D') port = PORTD;

            if(*param2 > 57){ // must be a char, ie set to another variable
                pin = varSpace[lower(*param2)];
            } else { // else it is an int value
                pin = atoi(param2);
            }
            inputMode(port, pin);
            value = input(port, pin);
            varSpace[lower(*param3)] = value;

            if(DEBUGGING) printf("GPIO INPUT port: %s, pin: %d in var: %s (%d) \n", param1, pin, param3, value);

           break;



//-----------------------------------------------
//
//  process OUTPUT command
//  OUTPUT <port> <PIN> <value/variable>
//  writes to gpio port
//  port = A, B, C
//  PIN = 0-7
//  value = 0 or 1
//-----------------------------------------------

   case OUTPUT:

            if(param1[0] == 'B') port = PORTB;      // select port
            if(param1[0] == 'C') port = PORTC;
            if(param1[0] == 'D') port = PORTD;

            if(*param2 > 57){ // must be a char, ie set to another variable
                pin = varSpace[lower(*param2)];
            } else { // else it is an int value
                pin = atoi(param2);
            }

            if(*param3 > 57){ // must be a char, ie set to another variable
                value = varSpace[lower(*param3)];
            } else { // else it is an int value
                value = atoi(param3);
            }           

            outputMode(port, pin);
            if(value == 1){
                outputHigh(port, pin);
            } else {
                outputLow(port, pin);
            }
            if(DEBUGGING) printf("GPIO INPUT port: %s in var: %s (%d) \n", param1, param2, value);

           break;



//-----------------------------------------------
//-----------------------------------------------
//
// Using the Power of VDP
//
// `VDP <value/variable>`  
// `VDPS <offset/variable> <count/variable>`  
//
//-----------------------------------------------
//-----------------------------------------------


//-----------------------------------------------
//
//  process VDP command
//  VDP <value/variable>
//  sends a byte to VDP from a varaible, an int, or a char
//
//-----------------------------------------------

 case VDP:
            
        leng = strlen(param1);

        bool deb = DEBUGGING;
        DEBUGGING = false;  // cannot have debugging while sending VDP

        if(leng == 8){
            value = (uint8_t) strtol(param1, NULL, 2);
        }
        else if(*param1 == 39){ // must be a single quote, ie char to be sent in [1]
            value = param1[1];
        } else if(*param1 > 57){ // must be a char, ie set to another variable
            value = varSpace[lower(*param1)];
        } else { // else it is an int value
            value = atoi(param1);
        }
        putchar(value);

        DEBUGGING = deb;
        break;

//-----------------------------------------------
//
//  process VDPS command
//  VDPS offset count
//  VDPS <value/variable> <value/variable>
//  sends several bytes to VDP from data
//
//-----------------------------------------------

   case VDPS:

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

    break;


//-----------------------------------------------
//-----------------------------------------------
//
// Sprites. Just thinking about it...
//
//-----------------------------------------------
//-----------------------------------------------
// LOADSP num filename
// SPRITETO x y
// SHOWSPRITE num 0/1
 


//-----------------------------------------------
//-----------------------------------------------
//
// Other Misc Commands & Debugging
//
//-----------------------------------------------
//-----------------------------------------------



//-----------------------------------------------
//
// process DEBUG command
//  DEBUG <value> (0 or 1)
//
//-----------------------------------------------

    case DEBUG:

        if(*param1 == '0'){
            DEBUGGING = false;
        } else{
            DEBUGGING = true;
        }

    break;


//-----------------------------------------------
//
//  PRINTVARS
// dump out all variables to screen
//
//-----------------------------------------------

    case PRINTVARS:
        for(uint8_t asc = 0; asc < 28; asc++){
            printf("%c = %d\n", (char)asc+63,  varSpace[asc]);
        }
    break;

 //-----------------------------------------------
//
//  process TIMER command
//  TIMER label freq
//  TIMER <value/variable> <value/variable>
//
//-----------------------------------------------

   case TIMER:

    // get label for timer routine
    if(*param1 > 57){ // must be a char, ie set to another variable
        offset = varSpace[lower(*param1)];
    } else { // else it is an int value
        offset = atoi(param1);
    }

    // get freq of routine. How often in 100ths / sec
    // 0 means cancel
    if(*param2 > 57){ // must be a char, ie set to another variable
        count = varSpace[lower(*param2)];
    } else { // else it is an int value
        count = atoi(param2);
    }

    if(count > 0){         //valid timer value
        if(timerRunning == 0)
            {
                timerFreq = count;
                timerLine = labels[offset];
                //timerReturnLine = currentLine;
                timerRunning = 1;
                if(DEBUGGING) printf("Set timer %d goes to line %d at freq of %d\n",offset, timerLine, timerFreq);
            }

    } else {        //stop timer
        timerRunning = 0   ;

    }
    if(param1 == NULL){
        timerRunning = 0   ;
    }



    break;

  default:
    if(DEBUGGING) printf("Probably a comment: %s \n", command);
    // get label for timer routine
    break;

//-----------------------------------------------
//
//  END OF PARSING
//
//-----------------------------------------------
    }   // end of switch statement
}       // end of command processing loop


//-----------------------------------------------
//
//  OTHER FUNCTIONS
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
// find string in array


uint16_t find_string(char *arr[], uint16_t size, char *target) {
    for (uint16_t i = 0; i < size; i++) {
        if (strcmp(arr[i], target) == 0) {
            return i;  // found, return index
        }
    }
    return 0;  // not found
}


//-----------------------------------------------
// strip leading white space

void strip_leading_spaces_inplace(char *str) {
    char *start = str;
    while (isspace((unsigned char)*start)) {
   //while (*start == 32) {
        start++;
    }
    memmove(str, start, strlen(start) + 1);  // +1 to include null terminator
}

char *strip_leading_spaces(char *str) {
    //while (isspace((unsigned char)*str)) {
    while ((uint8_t)*str == 32) {
        str++;
    }
    return str;
}

//-----------------------------------------------
// convert to UPPER case

void toUpperCase(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}




//-----------------------------------------------
// Returns 1 if a line was read, 0 on EOF with nothing read
int read_line(FILE *fp, char *buffer, size_t max_len) {
    int ch;
    size_t i = 0;

    while (i < max_len - 1) {
        ch = fgetc(fp);

        if (ch == EOF) {
            if (i == 0) return 0;   // nothing read, true EOF
            break;                  // last line with no trailing newline
        }
        if (ch == '\n') {
            break;                  // end of line
        }

        buffer[i++] = (char)ch;
    }

    buffer[i] = '\0';
    return 1;
}




//-----------------------------------------------
// get label's line number, either text or numerical


uint16_t getLabelLine(char *labelParam){
    uint16_t lineToreturn;
        if(*labelParam > 57){                   // must be a char, ie text label
            
            // need to look up label in array of those captured
            if(DEBUGGING) printf("GOTO: %s ", labelParam);
            uint16_t linefound;

            for (uint16_t i = 0; i < textLabelCount; i++) {
                if (strcmp(labelName[i], labelParam) == 0) {
                    linefound = i;  // found, return index
                    break;
                }
            }

            if(DEBUGGING) printf("Index: %d ", linefound);
            lineToreturn = labelLine[linefound];
            if(DEBUGGING) printf("line: %d \n", lineToreturn);
        } else {                            // else it is an int value
            value = atoi(labelParam);
            lineToreturn = labels[value];
        }
        
        for(uint8_t u = 0; u < textLabelCount; u++){
            if(DEBUGGING) printf("Label: %s Line: %d\n", labelName[u], labelLine[u]);

        }

        return lineToreturn;
}



//-----------------------------------------------
// build SIN table to be used by SIN and COS functions

void build_sin_table(void) {
    for (int i = 0; i < 256; i++) {
        double angle = (i / 256.0) * 2.0 * M_PI;
        sin_table[i] = (int8_t)(sin(angle) * 127.0);
    }
}

//-----------------------------------------------
//
//  END
//
//-----------------------------------------------
