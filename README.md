CS214 p3 - My Shell
    Tushar Cora Suresh (tc901) and Vatsal Mehta (vkm31)

Overview
MyShell is a custom shell implementation for Unix-based systems, crafted as part of the CS214 project. It offers both interactive and batch modes, efficiently handling command execution, redirection, piping, wildcards, and built-in commands. The design emphasizes compliance with Unix shell behavior, incorporating advanced features like handling wildcards and conditional execution based on the status of previous commands.

Key Features and Design Choices
Modes of Operation: MyShell operates in both interactive and batch modes, automatically determined using isatty().
Command Processing: Commands are read using read() and prompts are output using write(), ensuring low-level control over I/O operations.
Executable Path Resolution: The shell resolves paths to executables and parses argument strings through tokenization.
Input/Output Redirection: Utilizes dup2() for redirecting standard input and output, allowing for file-based input and output within commands.
Pipelines: Supports simple one-level piping between two commands by creating child processes and utilizing unnamed pipes (pipe()).
Wildcards: Implements wildcard expansion by matching files in the current directory against the wildcard pattern provided in the command.
Conditional Execution: Uses a global status variable to implement conditional execution (then, else) based on the exit status of the previous command.
Precedence Handling: Redirection is prioritized over pipelines in command execution.
Special Token Handling: Ensures special characters (<, >, |) are always recognized as individual tokens through preprocessing.
Command and Token Limits: Supports a maximum command length of 10,000 characters, with up to 1,000 tokens per command where each token can be up to 1,000 characters long.
Test Plan and Cases
Basic Functionality
Testing began with basic commands to verify core functionality without special cases.

Built-in Commands: pwd, exit, cd test, cd .., which cd, which touch
Bare Names: touch test, rm test, gcc test.c -o test, mv test newName
Pathnames: Executing compiled files (e.g., ./test, ./a), and testing nested shells with ./mysh.

Redirection
Focused on testing file creation and mode (0640), and the ability to handle input and output redirection correctly.

Output terminal commands: 
    ./test > test_output
    pwd > pathCheck
    ./test > output < input (redirects output of test into output)

Pipelines
    ./test | grep Hello
    ls | grep .txt 
    sort < input | uniq > output
    cat < input | grep "pattern" > output
    ls | grep "pattern" < input
    input correctly overrides the pipe
    ls | grep "pattern" > output
Evaluated combinations of piping and redirection, ensuring logical operation.

Simple pipe: ls | grep .txt
Combined redirection and pipe: cat < input | grep "pattern" > output

Wildcards
Tested by creating files matching wildcard patterns and verifying correct file selection.

Wildcard usage: ls *bar.txt, ls *.txt

Batch Mode and Conditionals
Using a script (batchtests.sh) to test batch mode execution and conditional commands based on previous command outcomes.
    echo Hello!
    echo Test2
    cd testcases
    ./test
    cd ..
    cd
    then cd testcases
    pwd

Comparison with Bash
Ensured MyShell's behavior aligns with bash by comparing output and execution results across various commands.

Special Token Handling
Verified commands with and without whitespace around redirection and pipe symbols to test tokenization.

Edge Cases and Considerations
Single-level Piping Only: Assumes at most one pipe in a command.
Wildcard Position: Assumes wildcards do not immediately follow redirection symbols and handles multiple wildcards within a single command.
Precedence: Redirection operations are prioritized over pipeline execution within command processing.
Special Character Handling: Special characters (<, >, |) are processed as distinct tokens, regardless of surrounding whitespace.

Conclusion
MyShell is a comprehensive shell implementation designed to replicate and extend basic shell functionalities with additional features like wildcard handling and conditional execution.
Through meticulous testing, we have ensured that MyShell behaves consistently with traditional Unix shells, providing a robust platform for command execution and script processing.