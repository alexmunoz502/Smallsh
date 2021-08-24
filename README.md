## Smallsh
#####  A basic shell program for Linux-based systems

#### --- ABOUT ---
This program was part of my operating system studies at Oregon State University. 
I learned how shell programs work, as well as parsing text commands and process
management through PIDs.  

#### --- SYSTEM REQUIREMENTS ---
The program runs on Linux-based operating systems.

#### --- COMPILING INSTRUCTIONS ---
The program can be compiled with the gcc compiler using the command:  
`gcc main.c -o smallsh`  

#### --- FILES ---
There is only one file in this program: main.c  

This file is compiled into a smallsh executable , which is then run to use 
the smallsh shell.  

#### --- HOW TO USE --- 
The smallsh has a few different functions that mirror the Linux bash shell.
Other than these built-in functions, smallsh calls upon the bash shell to
resolve any unprogrammed commands, effectively giving them the same 
functionality.  

###### Comments
The shell will ignore any input following the octothorpe ('#') symbol.  

###### Variable Expansion
Exactly two dollar signs ('$') next to eachother are expanded and replaced by
the process id (PID)  

###### exit
At any time, the exit command can be used to kill the smallsh program.  EXAMPLE:  
`: exit`  

###### cd
The cd command changes the current working directory.  EXAMPLE:  
`: cd my_folder`  
This command will change the current working directory to 'my_folder'  

###### status
The status command returns the exit status of the last foreground process. EXAMPLE:  
`: status`  

###### Other Functionality
Adding the ampersand character ('&') at the end of a command will run it in 
background mode, allowing you to continue to use smallsh while the command runs 
in the background. This is done using child processes.  

The interrupt signal, SIGINT (ctrl-c), is disabled in smallsh.  

You can kill background processes by fetching the PID through `: echo $$` and then 
using the kill command with the supplied pid `: kill PID`  