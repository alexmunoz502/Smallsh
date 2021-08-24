#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <signal.h>
#include <fcntl.h>

// Global variables
bool allowBackgroundMode = true;
int lastStatus = 0;

// structure to organize parts of user input
struct input 
{
	char *command; 
	char *argc[513];
    bool isBackground;
};

// Deallocation of input struct variables memory
void deallocate_input_variables(struct input *userInput)
{
    free(userInput->command);
    userInput->command = NULL;
    for (int i = 0; i < 513; i++)
    {
        if (userInput->argc[i] == NULL) break;
        free(userInput->argc[i]);
        userInput->argc[i] = NULL;
    }
}

// Background Mode Toggling
void toggle_background_mode()
{
    allowBackgroundMode = !allowBackgroundMode;
    if (!allowBackgroundMode)
    {
        write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 51);
    }
    else
    {
        write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 31);     
    }
}

// Signal Handling
void handle_SIGINT(bool canInterrupt)
{
    // Function Variables
    struct sigaction SIGINT_action = { 0 };

    // Allow Interrupt on Ctrl-C
    if (canInterrupt == true)
        SIGINT_action.sa_handler = SIG_DFL; // 'default' handling
    // Ignore interrupt on Ctrl-C
    else
        SIGINT_action.sa_handler = SIG_IGN; // 'ignore' handling

    // Set signals, flags, action
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);
}

void handle_SIGTSTP()
{
    // Function Variables
    struct sigaction SIGTSTP_action = { 0 };

    // Toggle allow background mode on Ctrl-Z
    SIGTSTP_action.sa_handler = toggle_background_mode;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

// Changing Directories Functionality
void change_directory(struct input* userInput)
{
    // No Supplied Arguments - Change directory to HOME address
    if (userInput->argc[0] == NULL)
    {
        char* HOME = getenv("HOME");
        chdir(HOME);
    }

    // Supplied Arguments - Change directory to user-specified address
    else 
    { 
        // This command only supports 1 argument, the desired destination
        // Supports both absolute and relative file paths
        DIR *targetDirectory = opendir(userInput->argc[0]);
        if (targetDirectory)
        {
            chdir(userInput->argc[0]); 
            closedir(targetDirectory);    
        }
        else perror("DIRECTORY ERROR");
    }
}

// Execute non-built-in commands
void execute_non_built_in_command(struct input *userInput)
{
    // Function Variables
    pid_t childPID;
    int childStatus;
    char *argv[514] = { NULL };
    char *inputRedirection = NULL;
    char *outputRedirection = NULL;

    // Since execvp needs an argv array with the command as the first element
    // we must combine the command and argc variables from the user input into
    // a new array that can be used for the execvp call
    // we also use this opportunity to check for redirects
    argv[0] = userInput->command;
    int argIndex = 0;
    for (int i = 0; i < 514; i++)
    {       
        // End of arguments found, terminate loop early
        if (userInput->argc[i] == NULL) break;

        // argIndex will keep track of the position we want to be in while copying the Args
        if (i != argIndex)
            continue;

        // Set Input Redirection
        if (*userInput->argc[i] == '<')
        {
            // Make sure next index exists and is not null
            argIndex++;
            if ((argIndex < 514) && (userInput->argc[argIndex] != NULL))
            {
                // Set input redirection and increment the argIndex
                inputRedirection = userInput->argc[i + 1];
                argIndex++;
            }
            continue;
        }
        // Set Output Redirection
        if (*userInput->argc[i] == '>')
        {
            // Make sure next index exists and is not null
            argIndex++;
            if ((argIndex < 514) && (userInput->argc[argIndex] != NULL))
                // Set input redirection and increment the argIndex
                outputRedirection = userInput->argc[i + 1];
            // Arguments after output redirection not supported
            break;
        }
        
        // Copy argument to new array
        argv[i + 1] = userInput->argc[i];
        argIndex++;
    
    }

    // Set input redirection if command is in background
    if (inputRedirection == NULL && userInput->isBackground == true)
    {
        inputRedirection = "/dev/null";
    }

    // Set output redirection if command is in background
    if (outputRedirection == NULL && userInput->isBackground == true)
    {
        outputRedirection = "/dev/null";
    }

    // Fork a child process
    childPID = fork();

    switch(childPID)
    {
        case -1:
            // Something went wrong while forking, fork failed
            perror("FORK ERROR");
            exit(1);
        case 0:
            // Child Process
            if (userInput->isBackground == false)
            {
                // Interrupt kills foreground child
                handle_SIGINT(true);
            }

            // Set I/O Redirection
            // - Input
            if (inputRedirection != NULL) 
            {
                int sourceFD = open(inputRedirection, O_RDONLY);
                if (sourceFD == -1)
                {
                    perror("INPUT REDIRECTION OPEN");
                    exit(EXIT_FAILURE);
                }

                int result = dup2(sourceFD, 0);
                if (result == -1) 
                { 
                    perror("INPUT REDIRECTION DUP2"); 
                    exit(EXIT_FAILURE); 
                }
            }
            // - Output
            if (outputRedirection != NULL) 
            { 
                int targetFD = open(outputRedirection, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (targetFD == -1)
                {
                    perror("OUTPUT REDIRECTION OPEN");
                    exit(EXIT_FAILURE);
                }

                int result = dup2(targetFD, 1);
                if (result == -1) 
                { 
                    perror("OUTPUT REDIRECTION DUP2"); 
                    exit(EXIT_FAILURE); 
                }
            }

            // Run Command with Args
            if (execvp(argv[0], argv) == -1)
            {
                perror("EXECVP ERROR");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            // Parent Process
            if (userInput->isBackground == true)
            {
                // Print the pid of background mode processes when executed
                printf("background pid is %d\n", childPID);
                fflush(stdout);
            }
            else
            {
                // Wait for child's termination if user didn't choose to run in background
                waitpid(childPID, &childStatus, 0);

                // Search for background child processes exiting or terminating
                if (allowBackgroundMode == true)
                {
                    // Background Termination by Interrupt
                    if (WIFSIGNALED(childStatus) == EXIT_FAILURE && WTERMSIG(childStatus) != 0){
                        printf("terminated by signal %d\n", WTERMSIG(childStatus));
                    }

                    // Background Process Exited or Non-Interrupt Terminated
                    childPID = waitpid(-1, &childStatus, WNOHANG);
                    if (WIFEXITED(childStatus) != EXIT_SUCCESS && childPID > 0)
                    {
                        printf("background pid %d is done: exit value %d\n", childPID, WEXITSTATUS(childStatus));
                    }
                    else if (WIFSIGNALED(childStatus) != EXIT_SUCCESS && childPID > 0 && userInput->isBackground == false)
                    {
                        printf("background pid %d is done: terminated by signal %d\n", childPID, WTERMSIG(childStatus));
                    }
                }
            }
            break;
    }

    // Assign child status to global status tracker
    lastStatus = childStatus;
}

// Status Command Functionality
void display_status()
{
    // lastStatus is a global variable
    if (WIFEXITED(lastStatus))
    {
		printf("exit value %d\n", WEXITSTATUS(lastStatus));
	}
	else if (WIFSIGNALED(lastStatus))
    {
	printf("terminated by signal %d\n", WTERMSIG(lastStatus));
	}
}

// Input Handling
int process_input(struct input* userInput)
{
    // Exit Codes
    static const int EXIT_PROGRAM = 0;
    static const int EXIT_CONTINUE = 1;

    // Process Functionality
    // - No Input
    if (userInput == NULL)
    {
        return EXIT_CONTINUE;
    }
    // - Comment
    if (userInput->command[0] == '#')
    {
        // Ignore Entire Line
        return EXIT_CONTINUE;
    }
    // - Exit
    if ((strcmp(userInput->command, "exit")) == 0)
    {
        return EXIT_PROGRAM;
    }
    // - Change Directory
    if ((strcmp(userInput->command, "cd")) == 0)
    {
        change_directory(userInput);
        return EXIT_CONTINUE;
    }
    // - Status of Last Foreground Process Exit
    if ((strcmp(userInput->command, "status")) == 0)
    {
        display_status();
        return EXIT_CONTINUE;
    }
    // - Non-built-in Functions
    else
    {
        execute_non_built_in_command(userInput);
        return EXIT_CONTINUE;
    }
    
}

// Parse the input string and assign the user input struct variables
struct input* parse_line(char *line)
{
    // Function Constants
    static const char *DELIMITER = " ";

    // Function Variables
    int size = 0;
    char *token = NULL;
    char *pointer = NULL;
    struct input *userInput = NULL;

    // Set input pointer to NULL if there is no line to parse
    if (line == NULL) return NULL;

    // Assign user input to allocated memory
    userInput = calloc(1, sizeof(struct input));
    userInput->argc[512] = NULL;
    
    // Parse Line Functionality
    token = strtok_r(line, DELIMITER, &pointer);

    // This first token will be the command the user wishes to run
    userInput->command = calloc(strlen(token) + 1, sizeof(char));
    strcpy(userInput->command, token);

    // Each subsequent token, if any, will be stored as input arguments
    int i = 0;
    while ((token = strtok_r(NULL, DELIMITER, &pointer)) != NULL)
    {
        userInput->argc[i] = calloc(strlen(token) + 1, sizeof(char));
        strcpy(userInput->argc[i], token);
        i++;
    }

    // Check if user wishes the command to be run in the background
    if (*userInput->argc[i-1] == '&')
    {
        // Enable background mode if allowed
        if (allowBackgroundMode) userInput->isBackground = true;
        else userInput->isBackground = false;

        // Remove character from arg list
        free(userInput->argc[i-1]);
        userInput->argc[i-1] = NULL;
    }
    else userInput->isBackground = false;

    return userInput;
}

// Replace subtring with subtring helper Function
// Followed tutorial from thread: https://stackoverflow.com/questions/32413667/replace-all-occurrences-of-a-substring-in-a-string-in-c
void replace_substring(char* string, const char* substring, char* replacement)
{
    // Function Constants
    static const int REPLACING = 1;

    // Function Variables
    char buffer[2048] = { 0 };
    char *pointer = &buffer[0];
    const char *tempString = string;
    size_t lenSubstring = strlen(substring);
    size_t lenReplacement = strlen(replacement);

    // Subtring Replacement Functionality
    while (REPLACING) {
        const char *substringIndex = strstr(tempString, substring);

        // Check if another substring exists in the string
        if (substringIndex == NULL) {
            // No more substrings to replace, exit loop
            strcpy(pointer, tempString);
            break;
        }

        // Copy string up to index of found substring
        memcpy(pointer, tempString, substringIndex - tempString);
        pointer += substringIndex - tempString;

        // Copy string after index of found substring
        memcpy(pointer, replacement, lenReplacement);
        pointer += lenReplacement;

        // Increment the pointer to continue searching at the index after the replaced substring
        tempString = substringIndex + lenSubstring;
    }

    // Copy the string with the replaced substrings to the original string
    strcpy(string, buffer);
}

// Read a line of characters from stdin to pass to the parser
char* read_line()
{
    // Function Constants
    static const int MAX_LINE_LENGTH = 2048;
    static const int EXPANDING = 1;
    static const char *PID_SYMBOL = "$$";
    const int SMALLSH_PID = getpid();

    // Function Variables
    size_t size = 0;
    ssize_t length = 0; 
    char *buffer = calloc(MAX_LINE_LENGTH, sizeof(char));

    // Read Line Functionality
    fgets(buffer, MAX_LINE_LENGTH, stdin);
    length = strlen(buffer);

    // Strip newline and carriage return characters from end of line
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
    {
        buffer[length - 1] = '\0';
    }
    // Empty input from user
    if (buffer[0] == '\0' || length == 0) 
    {
        free(buffer);
        return NULL;
    }

    // Variable Expansion - convert each instance of the PID symbol to the Smallsh PID
    if (strstr(buffer, PID_SYMBOL) != NULL)
    {
            // Convert PID to String
            char str_pid[8];
            sprintf(str_pid, "%d", SMALLSH_PID);

            // Replace each instance of PID symbol with the PID string
            replace_substring(buffer, PID_SYMBOL, str_pid);
    }

    return buffer;
}

// Flush IO and display input prompt for user
void smallsh_prompt()
{
    fflush(stdout);
    printf(": ");
    fflush(stdout);
}

// Main Program Function
int main(void) {
    // Program Constants
    static const int RUNNING = 1;

    // Program Variables
    int state = RUNNING;
    char *line = NULL;
    struct input *userInput = NULL;

    // Smallsh Shell Line Program
    while (state == RUNNING)
    {   
        // Handle interrupt and stop signals
        handle_SIGTSTP();
        handle_SIGINT(false); // false: can't be interrupted

        // Get User Input
        smallsh_prompt();
        line = read_line();
        // printf("Line: %s\n", line);
        // fflush(stdout);
        userInput = parse_line(line);
        

        // Execute User Input
        state = process_input(userInput);

        // Deallocate memory
        // - Line
        free(line);
        line = NULL;
        // - Struct
        if (userInput != NULL) deallocate_input_variables(userInput);
        free(userInput);
        userInput = NULL; 
        
    }

    // Kill any background processes
    kill(getpid(), SIGTERM);
  
    // Exit Program
    return EXIT_SUCCESS;
}