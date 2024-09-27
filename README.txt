
Custom Shell Program

Authored by Mori Sason

---

Program Overview

This custom shell program, written in C, supports advanced command management through aliases and background job handling. It also features command chaining (`&&`, `||`), background execution (`&`), and error redirection (`2>`), along with script file execution and job management.

---

Key Features

- Alias Management: Define, list, and remove command shortcuts.
- Background Execution: Run commands in the background using `&` while continuing other tasks.
- Command Chaining: Chain commands with `&&` (logical AND) and `||` (logical OR).
- Error Redirection: Redirect error output using `2>` to a specified file.
- Job Management: Track and display background jobs using the `jobs` command.
- Script Execution: Source and execute commands from `.sh` script files within the shell.

---

Functions Overview

Alias Management:
- `add_alias(char *name, char *command)`: Adds or updates an alias.
- `find_alias(char *name)`: Retrieves the command for a given alias.
- `remove_alias(char *name)`: Deletes a specified alias.
- `print_or_execute_alias()`: Lists all defined aliases or executes the "alias" alias if defined.

Command Processing:
- `parse_command(char *command, char **args)`: Parses a command into arguments.
- `process_alias_command(char *command)`: Adds a valid alias to the list.
- `expand_aliases(char *command)`: Replaces aliases in the command with their definitions.
- `handle_command(char *command, int from_script, int check, char *stderr_filename)`: Executes a command, managing background jobs and error redirection.

Script Execution:
- `process_script_file(char *filename, int check, char *stderr_filename)`: Reads and processes commands from a script file.

Job Management:
- `add_job(pid_t pid, char *command)`: Adds a background job to the job list.
- `print_jobs()`: Displays all running background jobs.
- `clean_up_jobs()`: Cleans up finished jobs from the job list.

---

Running Instructions

Provided Files:
- `program.c`: Source code for the custom shell.
- `run_me.sh`: Script to compile and run the shell.

How to Run:
1. Open a terminal.
2. Navigate to the directory containing `program.c` and `run_me.sh`.
3. Run the following command:
    ./run_me.sh

---

License
This project is licensed under the MIT License. See the LICENSE file for more details.

---

Author
Mori Sason  
LinkedIn: https://www.linkedin.com/in/mori-sason-9a4811281  
Email: 8mori8@gmail.com
