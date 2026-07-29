# KISS

KISS is a very simple stripped down language for the Agon platform (Agon light, Agon light2, Agon Origins, Console8) and is work in progress.  
The objective is to think in simple commands and simple structures, with limited data.  
A good prep for learning assembler, with a limited number of single character variable names and a limited number of actual commands.  
The editor part is based on `AED` (Another Text Editor by Igor Chaves Cananea).  
VDP calls allow significant use of the VDP system if required.  
To load, edit and run your file, use:  
`kiss myfile.txt`  

Use `CTRL R` ro run the file.  

A command line version is available with no editor, that takes the file name as the first argument. eg.  
`runkiss myfile.txt`

# The Language

Concept-
8 bit byte values only.  
variables A -> Z.  
@ is used as special case, the 'answer' to some queries.  
? is used as special case, the 'carry' to some math or comparison functions.

Goto or conditional goto.  
Single loop.  
Subroutines, up to 16 deep.  
Up to 256 labels.  
Unlimited comments.  

There is absolutely no error checking or reporrting, so just like writing in assembler, you could very easily crash with very little feedback!

You can enable DEBUG mode with the command `DEBUG 1` which will report each line as it is processed.


# Command set

## Dealing with Variables

`SET <variable> <value/variable>`  
sets a variable to a value 0-255, or ascii code 'g' for example:  
SET x 25  
You can make it more friendly, such as:  
SET x = 25  
SET x to 25  
SET x = y  

`RND <variable> <value/variable>`  
Sets a variable to a random value in range.  

`COMP <variable> <value/variable>`  
Compare a variable with either another variable or a number.  
Sets the 'result' to be the difference of A - B.  
ie. if both the same, then result will be 0.  
Carry flag '?' set to 0 if same or greater, or 255 if negative result.  

Eg:  
COMP x t  
COMP x with 6  
COMP x to y


`GETDATA <offset/variable> <variable>`   
Read data at file offset and put into variable

`SETDATA <offset/variable> <value/variable>`  
Set data at given offset with value or variable


## Program Flow & logic


`LABEL <number>`  
A label with number 0-255. Used for GOTO, etc.


`LOOP <value/variable>`  
Loop a number of times.  
(Currently only one loop at a time, no loops within loops)

`ENDLOOP <value/variable>`  
Reduce counter and loop round if not zero.

`GOTO <label ID>`  
Goto label.

`GOTOIF <label ID>` <value expected in '@' value/variable>  
Goto label if 'result' is a certain value or value in a variable.

`CALL <label ID>`  
Call labelled subroutine and return later.

`CALLIF` <label ID> <value expected in '@' value/variable>  
Call if result matches.  
eg. if 'result' is 20, then gosub label subroutine.

`RET`  
Return from a subroutine, if there is a line number in the return variable.  
Stack allows for up to 16 nested call/returns.


`DELAY <variable1/value>`  
Delays action for value 1/100's second.


`EXIT`  
Finish code and exit program


## Maths Operations

`ADD <variable1/value> <variable2>`  
Adds variable1 or value to variable2.  
Carry flag has 1 if overrun.

`SUB <variable1/value> <variable2>`  
Subtracts variable1 or value from variable2.  
Carry flag has -1 (255) if underrun.

`MUL <variable1/value> <variable2>`  
Multiplies variable1 or value by variable2. Result is stored in variable1. 'carry flag' contains overrun times. eg.  
MUL A by 6  
MUL a * u  
MUL a b

`DIV <variable1/value> <variable2>`  
Divides variable1 or value by variable2. Result is stored in variable1 as rounded down integer. Mod is stored in 'carry flag'

## Text and Display

`CLS`  
Clear the screen.

`INK <value/variable>`  
Sets foreground or background colours.  
0-63 is foreground ink, 128-191 is background, assuming current screen mode has capability.

`MODE <value/variable>`  
Sets screen mode.


`TABTO <value/variable> <value/variable>`  
Set cursor tab position to x,y


`PRINT <string>`  
Sends a contiguous series of chars to VDP as string. No spaces allowed.


`PRINTNUM` <value/variable> <optional format>  
Prints out a number. Format options: DEC, HEX, BIN.  
Default is decimal

`CR`  
Print a CR

`SPC`  
Print a space

`CURSOR <value>`  
Turn cursor off/on (0 or 1)


## Plotting Line Graphics

`PEN <value/variable>`  
Sets ink colour for PLOT commands.  
0-63 is foreground ink.  


`PLOT <value/variable> <value/variable>`  
Plot point at x,y.  
Only basic lines are currently implemented as commands, with X and Y limited to 255.  
Full functionality can be achieved by sending raw VDP commands.

`MOVETO <value/variable> <value/variable>`  
Move plot position to x,y

`LINETO <value/variable> <value/variable>`  
Move plot position to x,y


## Interaction with user

`KEY`  
checks down status and sets '@' to ascii code or 0 if none.


`WAIT`  
Waits for user to press and release any key.


`BEEP <offset/variable> <value/variable>`  
Beep <freq> for <time>

`INPUT <variable>`
Requests numerical input from user.


## Using the Power of VDP

`VDP <value/variable>`  
Sends a byte to VDP.  
NOTE: Debug mode must not be active when sending VDP bytes.


`VDPS <offset/variable> <value/variable>`  
Sends a series of bytes to VDP from data store.


## Other Misc Commands


`DEBUG <value>`  
Turn debug off/on (0 or 1).




# Code notes

Not case sensitive, but cannot mix case within a command.  
eg. `SET` or `set` are valid, `seT` is not.

Variables can be upper or lower case, but are the same.  
eg. `A` is the same as `a`.



# Data

A block of 256 bytes of data is available for additional storage.  

This is stored in a local file named `kiss.data` and loaded at runtime, so can be edited outside of AlphaMinus.
This could contain strings or other information, even data for UDGs.  

The file is not changed by any user code, only the data loaded into memory.  

Export of data to be considered on todo list.  
Importing of other files on todo list


Comments- start with /
/a comment line

Every command or data is seperated by space/s.  
A new code line starts after a CR.  

Any line's command not recognised will be ignored.
