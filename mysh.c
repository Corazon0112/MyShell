#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_COMMAND_LENGTH 10000
#define MAX_TOKENS 1000
#define MAX_TOKEN_LENGTH 1000
#define BUFLENGTH 16

// Directory paths to search for executables
#define DIR_PATHS {"/usr/local/bin", "/usr/bin", "/bin"}

// Global int variable that keeps track of if the previous command failed or succeeded
//Used for conditionals
int currstatus = 1;

typedef struct {
    int fd;
    int pos;
    int len;
    char buf[BUFLENGTH];
} lines_t;

// Function prototypes
void print_prompt();
void fdinit(lines_t *L, int fd);
char *read_command(lines_t *L); 
void parse_command(char* command, char* tokens[]);
void execute_command(char* tokens[]);
int check_wildcard(char* token, char* tokens[], int tokencount);
void execute_builtin_command(char* tokens[]);
void print_welcome_message();
void print_goodbye_message();
int check_slash(char* command);
void check_redirection(char* tokens[]);
void execute_full(char* tokens[]);
int check_pipe(char* tokens[]);
void preprocess_command(char* command);


int main(int argc, char* argv[]) {
    bool interactive_mode = true;  // Default mode is interactive
    int filefd = STDIN_FILENO;     // Default file descriptor for input is standard input

    // Check if any arguments were passed to determine operation mode
    if (argc > 1) {
        // Extract the file name from command line arguments
        char *filename = argv[1];
        // Get the last three characters of the filename to check its extension
        char *lastthree = &filename[strlen(filename)-3];

        // If the file extension is ".sh", switch to batch mode
        if (strcmp(lastthree, ".sh") == 0) {
            interactive_mode = false;
            filefd = open(argv[1], O_RDONLY);  // Open the file for reading
        }
    } else {
        // If no arguments, check if the input is from a terminal (interactive mode)
        interactive_mode = isatty(STDIN_FILENO);
    }

    // Initialize the input stream with the file descriptor
    lines_t inputstream;
    fdinit(&inputstream, filefd);

    // If in interactive mode, print a welcome message
    if (interactive_mode) {
        print_welcome_message();
    }

    // Main loop for reading and executing commands
    while (1) {
        char command[MAX_COMMAND_LENGTH];
        char* tokens[MAX_TOKENS];
 
        // If in interactive mode, display a prompt
        if (interactive_mode) {
            print_prompt();
        }

        // Read a command from the input stream
        char *line = read_command(&inputstream);
        if (line) {
            strcpy(command, line);  // Copy the read line into command
        } else {
            break;  // Exit loop if no line is read (EOF or error)
        }
        
        // Parse the command into tokens
        parse_command(command, tokens);

        // Execute the parsed command
        execute_full(tokens);
    }

    // If in interactive mode, print a goodbye message before exiting
    if (interactive_mode) {
        print_goodbye_message();
    }

    return 0;  // Return success status
}

// Function to print the shell prompt
void print_prompt() {
    const char *prompt = "mysh> ";
    write(dup(STDOUT_FILENO), prompt, strlen(prompt));  // Duplicate stdout and write prompt to it
}

// Function to print the welcome message at the start
void print_welcome_message() {
    printf("Welcome to my shell!\n");
}

// Function to print a goodbye message before exiting
void print_goodbye_message() {
    printf("Exiting\n");
}

// Function to initialize a lines_t structure with a file descriptor
void fdinit(lines_t *L, int fd) {
    L->fd = fd;   // Assign the file descriptor
    L->pos = 0;   // Initialize current position in buffer to 0
    L->len = 0;   // Initialize length of data in buffer to 0
}

char *read_command(lines_t *L) {
    if (L->fd < 0) return NULL; // Check if file descriptor is valid
    char *line = NULL; // Pointer to the line being read
    int line_length = 0; // Length of the line
    int segment_start = L->pos; // Start position of the current segment within the buffer

    while (1) {
        // If the position is at the end of the buffer, try to read more data
        if (L->pos == L->len) {
            // If there's an unfinished line segment, append it to the line
            if (segment_start < L->pos) {
                int segment_length = L->pos - segment_start;
                line = realloc(line, line_length + segment_length + 1); // Extend the line buffer
                memcpy(line + line_length, L->buf + segment_start, segment_length); // Copy segment
                line_length += segment_length;
                line[line_length] = '\0'; // Null-terminate the line
            }
            // Read new data into the buffer
            L->len = read(L->fd, L->buf, BUFLENGTH);
            if (L->len < 1) { // If read fails or EOF is reached, close the file descriptor and return
                close(L->fd);
                L->fd = -1;
                return line;
            }
            // Reset position and segment start for the new data
            L->pos = 0;
            segment_start = 0;
        }
        // Process the current buffer content
        while (L->pos < L->len) {
            // If a newline character is found, end the line
            if (L->buf[L->pos] == '\n') {
                int segment_length = L->pos - segment_start;
                line = realloc(line, line_length + segment_length + 1);
                memcpy(line + line_length, L->buf + segment_start, segment_length);
                line[line_length + segment_length] = '\0';
                L->pos++;
                return line;
            }
            L->pos++;
        }
    }
    return NULL; // Should never reach this point
}

void preprocess_command(char* command) {
    int i = 0; // Index for iterating through the command
    while (command[i] != '\0') { // Continue until the end of the string
        if (command[i] == '>' || command[i] == '<' || command[i] == '|') {
            // Add space before the token if necessary
            if (i > 0 && command[i - 1] != ' ') {
                int j = strlen(command);
                while (j >= i) {
                    command[j + 1] = command[j];
                    j--;
                }
                command[i] = ' ';
                i++;
            }
            // Add space after the token if necessary
            if (command[i + 1] != ' ') {
                int j = strlen(command);
                while (j >= i + 1) {
                    command[j + 1] = command[j];
                    j--;
                }
                command[i + 1] = ' ';
                i++;
            }
        }
        i++;
    }
} 

// Parses a command string into an array of tokens for execution.
void parse_command(char* command, char* tokens[]) {
    char* token; // Pointer to the current token
    int token_count = 0; // Number of tokens found

    preprocess_command(command); // Preprocess to handle special tokens

    // Tokenize the command string using spaces, tabs, and newlines as delimiters
    token = strtok(command, " \t\n");
    while (token != NULL && token_count < MAX_TOKENS - 1) {
        // Handle wildcard expansion, if necessary
        int x = 0;
        if ((x = check_wildcard(token, tokens, token_count)) > 0)
            token_count += x;
        else { // Normal token processing
            tokens[token_count] = malloc(strlen(token) + 1); // Allocate memory for the token
            strcpy(tokens[token_count], token); // Copy the token
            token_count++; // Increment token count
        }
        token = strtok(NULL, " \t\n"); // Continue tokenization
    }
    tokens[token_count] = NULL; // Null-terminate the tokens array
}

// Determines if a command contains a slash '/', indicating a direct pathname.
// Returns 1 if true, indicating a direct pathname.
int check_slash(char* command) {
    for (int i = 0; command[i] != '\0'; i++)
        if (command[i] == '/')
            return 1; // Slash found, indicating a direct pathname
    return 0; // No slash found, not a direct pathname
}

// Executes a single command, considering redirections, conditionals, and whether the command is built-in or external.
void execute_command(char* tokens[]) {
    // Save current STDIN and STDOUT file descriptors to restore later after redirections
    int original_stdout = dup(STDOUT_FILENO);
    int original_stdin = dup(STDIN_FILENO);

    // Handling conditional execution based on the outcome of the previous command
    if (strcmp(tokens[0], "then") == 0) {
        if (currstatus != 1) return; // Skip command if the previous command did not succeed
        tokens++; // Move past the conditional token for execution
    }

    if (strcmp(tokens[0], "else") == 0) {
        if (currstatus != 0) return; // Skip command if the previous command succeeded
        tokens++; // Move past the conditional token for execution
    }

    // Execute built-in commands directly without forking
    if (strcmp(tokens[0], "cd") == 0 || strcmp(tokens[0], "pwd") == 0 || strcmp(tokens[0], "which") == 0 || strcmp(tokens[0], "exit") == 0) {
        check_redirection(tokens); // Handle redirection if any before executing
        execute_builtin_command(tokens); // Execute the built-in command
    } else if (check_slash(tokens[0]) == 0) { // If not a direct pathname, search for the command in specified directories
        char* dir_paths[] = DIR_PATHS; // Assuming DIR_PATHS is an array of directory paths
        int found = 0; // Flag to check if command is found
        for (int i = 0; dir_paths[i] != NULL && !found; i++) {
            char path[MAX_COMMAND_LENGTH];
            // Construct the full path of the command
            strcpy(path, dir_paths[i]);
            strcat(path, "/");
            strcat(path, tokens[0]);
            // Check if the command exists and is executable
            if (access(path, X_OK) == 0) {
                found = 1; // Command found
                pid_t pid = fork(); // Fork a child process to execute the command
                if (pid == 0) { // Child process
                    check_redirection(tokens); // Handle redirection if any
                    execv(path, tokens); // Execute the command
                    perror("execv"); // Execv should not return, print an error if it does
                    currstatus = 0;
                    exit(EXIT_FAILURE);
                } else if (pid > 0) { // Parent process waits for child to complete
                    int status;
                    waitpid(pid, &status, 0);
                    currstatus = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
                } else {
                    perror("fork"); // Forking failed
                    currstatus = 0;
                }
            }
        }
        if (!found)
            printf("Command not found: %s\n", tokens[0]);
    } else { // Command includes a slash '/', execute directly
        pid_t pid = fork(); // Fork a child process to execute the command
        if (pid == 0) { // Child process
            check_redirection(tokens); // Handle redirection if any
            execv(tokens[0], tokens); // Execute the command
            perror("execv"); // Execv should not return, print an error if it does
            currstatus = 0;
            exit(EXIT_FAILURE);
        } else if (pid > 0) { // Parent process waits for child to complete
            int status;
            waitpid(pid, &status, 0);
            currstatus = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
        } else {
            perror("fork"); // Forking failed
            currstatus = 0;
        }
    }

    // Restore the original STDIN and STDOUT file descriptors
    dup2(original_stdout, STDOUT_FILENO);
    dup2(original_stdin, STDIN_FILENO);
    close(original_stdout);
    close(original_stdin);
}

// Expands wildcard patterns to matching file names, adding them to the tokens array.
// Returns the count of matched files.
int check_wildcard(char* token, char* tokens[], int tokencount) {
    bool wildcardFound = false;
    bool pathFound = false;
    int matchCount = 0;
    // Allocate memory for path and temp token
    char *startingpath = malloc(strlen(token) + 1);
    strcpy(startingpath, token);
    int finalPathStart = 0;
    
    // Check if the token includes a path by looking for '/'
    for (int i = 0; i < strlen(token); i++)
        if (token[i] == '/' && i < (strlen(token) - 1)) {
            finalPathStart = i + 1;
            pathFound = true;
        }
    
    // Isolate the path part of the token, if any
    if (pathFound)
        startingpath[finalPathStart - 1] = '\0';

    // Isolate the part of the token after the last '/'
    char *temptoken = malloc(strlen(token) + 1);
    strcpy(temptoken, &token[finalPathStart]);
    
    // Find the location of the wildcard '*' if present
    int wildcardLocation = 0;
    for (int i = 0; i < strlen(temptoken); i++) {
        if (temptoken[i] == '*') {
            wildcardFound = true;
            break;
        }
        wildcardLocation++;
    }

    // If a wildcard is found, attempt to match files
    if (wildcardFound) {
        char* firsthalf = temptoken;
        char* secondhalf = &temptoken[wildcardLocation + 1];
        firsthalf[wildcardLocation] = '\0'; // Split the token into before and after '*'

        DIR *d;
        struct dirent *dir;
        // Open the directory (either specified or current)
        if (pathFound && strlen(startingpath) > 0)
            d = opendir(startingpath);
        else
            d = opendir(".");

        if (d) {
            while ((dir = readdir(d))) {
                char *currname = dir->d_name;
                struct stat sbuf;
                char* fullpath = malloc(strlen(startingpath) + strlen(currname) + 2);
                
                // Construct the full path to check its status
                if (pathFound)
                    sprintf(fullpath, "%s/%s", startingpath, currname);
                else
                    strcpy(fullpath, currname);

                if (stat(fullpath, &sbuf) == 0 && S_ISREG(sbuf.st_mode) && currname[0] != '.') {
                    bool matchCheck = true;
                    // Check match for the first half of the token
                    for (int i = 0; i < strlen(firsthalf) && matchCheck; i++)
                        if (currname[i] != firsthalf[i])
                            matchCheck = false;

                    // Check match for the second half of the token
                    int currnameLen = strlen(currname), secondhalfLen = strlen(secondhalf);
                    for (int i = 0; i < secondhalfLen && matchCheck; i++)
                        if (currname[currnameLen - secondhalfLen + i] != secondhalf[i])
                            matchCheck = false;

                    // If a match is found, add it to the tokens
                    if (matchCheck) {
                        tokens[tokencount++] = fullpath; // Use pre-increment to ensure 'tokens[tokencount]' is not used before increment
                        matchCount++;
                    } else
                        free(fullpath); // Free the memory if not a match
                } else
                    free(fullpath); // Free the memory if stat fails or not a regular file
            }
            closedir(d);
        }
    }

    // Free the temporary memory allocated
    free(startingpath);
    free(temptoken);

    return matchCount;
}

// Frees all dynamically allocated tokens in an array to avoid memory leaks.
void free_tokens(char* tokens[]) {
    for (int i = 0; tokens[i] != NULL && i < MAX_TOKENS - 1; i++) {
        free(tokens[i]);
        tokens[i] = NULL; // Ensure the pointer is set to NULL after freeing
    }
}

// Execute built-in shell commands
void execute_builtin_command(char* tokens[]) {
    // Change directory with 'cd'
    if (strcmp(tokens[0], "cd") == 0) {
        if (tokens[1] != NULL) {
            if (chdir(tokens[1]) != 0) {
                perror("cd"); // Print error if change directory fails
                currstatus = 0;
            } else
                currstatus = 1;
        } else {
            fprintf(stderr, "cd: missing argument\n"); // No directory specified
            currstatus = 0;
        }
    } 
    // Print the current working directory with 'pwd'
    else if (strcmp(tokens[0], "pwd") == 0) {
        char cwd[1024]; // Buffer for the current working directory
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd); // Print the current directory
            currstatus = 1;
        } else {
            perror("pwd"); // Print error if getting current directory fails
            currstatus = 0;
        }
    } 
    // Check if a command exists in PATH with 'which'
    else if (strcmp(tokens[0], "which") == 0) {
        if (tokens[1] == NULL || tokens[2] != NULL || 
            strcmp(tokens[1], "cd") == 0 || strcmp(tokens[1], "pwd") == 0 ||
            strcmp(tokens[1], "which") == 0 || strcmp(tokens[1], "exit") == 0) {
            fprintf(stderr, "which: incorrect arguments\n");
            currstatus = 0;
        } else {
            char *path_env = getenv("PATH");
            char *path_env_copy = malloc(strlen(path_env) + 1);
            strcpy(path_env_copy, path_env);
            char *path = strtok(path_env_copy, ":");

            while (path != NULL) {
                char cmd_path[1024]; // Buffer for command path
                sprintf(cmd_path, "%s/%s", path, tokens[1]); // Construct command path
                
                if (access(cmd_path, X_OK) == 0) {
                    printf("%s\n", cmd_path); // Command found in PATH
                    currstatus = 1;
                    free(path_env_copy);
                    return;
                }

                path = strtok(NULL, ":"); // Continue searching in PATH
            }
            fprintf(stderr, "which: no command found in PATH\n");
            currstatus = 0;
            free(path_env_copy);
        }
    } 
    // Exit the shell with 'exit'
    else if (strcmp(tokens[0], "exit") == 0) {
        printf("Exiting mysh\n");
        free_tokens(tokens); // Clean up before exiting
        exit(EXIT_SUCCESS);
    }
}

// Checks for and performs input/output redirection
void check_redirection(char *tokens[]) {
    int input_index = -1, output_index = -1; // Indices for redirection symbols
    char *input_file = NULL, *output_file = NULL;

    // Loop through tokens to find redirection symbols and files
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "<") == 0) { // Input redirection
            input_file = tokens[i + 1];
            input_index = i;
        } else if (strcmp(tokens[i], ">") == 0) { // Output redirection
            output_file = tokens[i + 1];
            output_index = i;
        }
    }

    // Perform input redirection if needed
    if (input_file != NULL) {
        int fd = open(input_file, O_RDONLY);
        if (fd < 0) {
            perror("open input file");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
        tokens[input_index] = tokens[input_index + 1] = NULL; // Remove redirection symbols and files from tokens
    }

    // Perform output redirection if needed
    if (output_file != NULL) {
        int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
        if (fd < 0) {
            perror("open output file");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        tokens[output_index] = tokens[output_index + 1] = NULL; // Remove redirection symbols and files from tokens
    }
}

//check if pipe exists in the command
//returns 1 if it does
int check_pipe(char* tokens[]) {
    int i = 0;
    int state = 0;
    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], "|") == 0) {
            state = 1;
            break;
        }
        i++;
    }
    return state;
}

// Executes the full command with consideration for piping.
void execute_full(char* tokens[]) {
    // Save the original stdout and stdin file descriptors
    int original_stdout = dup(STDOUT_FILENO);
    int original_stdin = dup(STDIN_FILENO);

    if (check_pipe(tokens) == 0) {
        // No pipe found, execute command normally
        execute_command(tokens);
    } else {
        // Find the index of the pipe symbol in the tokens array
        int pipe_index = 0;
        while (tokens[pipe_index] != NULL) {
            if (strcmp(tokens[pipe_index], "|") == 0) {
                break; // Pipe symbol found
            }
            pipe_index++;
        }

        int p[2]; // Array to hold pipe file descriptors
        if (pipe(p) == -1) { // Attempt to create a pipe
            perror("pipe");
            exit(1);
        }

        pid_t pid1 = fork(); // Fork the first child process
        if (pid1 == -1) {
            perror("fork");
            exit(1);
        } else if (pid1 == 0) { // Child 1: Executes the command before the pipe
            close(p[0]); // Close the read end of the pipe
            dup2(p[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe
            close(p[1]); // Close the write end of the pipe
            tokens[pipe_index] = NULL; // Nullify the pipe symbol for execution
            execute_command(tokens); // Execute the first part of the piped command
            exit(0);
        } else {
            // Fork the second child process to handle the command after the pipe
            pid_t pid2 = fork();
            if (pid2 == -1) {
                perror("fork");
                exit(1);
            } else if (pid2 == 0) { // Child 2: Executes the command after the pipe
                close(p[1]); // Close the write end of the pipe
                dup2(p[0], STDIN_FILENO); // Redirect stdin to the read end of the pipe
                close(p[0]); // Close the read end of the pipe
                execute_command(tokens + pipe_index + 1); // Execute the second part of the piped command
                exit(0);
            } else {
                // Parent process: Waits for both child processes to complete
                close(p[0]);
                close(p[1]);
                wait(NULL); // Wait for the first child
                wait(NULL); // Wait for the second child
            }
        }
    }
    // Restore the original stdout and stdin file descriptors
    dup2(original_stdout, STDOUT_FILENO);
    dup2(original_stdin, STDIN_FILENO);
    close(original_stdout);
    close(original_stdin);
}