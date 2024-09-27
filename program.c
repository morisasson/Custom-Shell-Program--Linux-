//208067587

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

#define D 1025

// Structure to represent an alias
struct Alias {
    char name[D];
    char command[D];
    struct Alias *next;
};

// Structure to represent a background job
struct Job {
    int id;
    pid_t pid;
    char command[D];
    struct Job *next;
};

// Global variables
int successful_commands = 0;
int active_aliases = 0;
int total_script_lines = 0;
int quoted_commands = 0;
int next_job_id = 1;
struct Alias *alias_list = NULL;
struct Job *job_list = NULL;
int checking = 0;

// Function prototypes
void handle_command(char *command, int from_script, int check, char *stderr_filename);

// Function to add an alias to the alias list
void add_alias(char *name, char *command) {
    struct Alias *current_alias = alias_list;
    while (current_alias != NULL) {
        if (strcmp(current_alias->name, name) == 0) {
            strcpy(current_alias->command, command);
            return;
        }
        current_alias = current_alias->next;
    }

    struct Alias *new_alias = (struct Alias *)malloc(sizeof(struct Alias));
    if (new_alias == NULL) {
        fprintf(stderr, "Memory allocation failed for alias.\n");
        exit(1);
    }
    strcpy(new_alias->name, name);
    strcpy(new_alias->command, command);
    new_alias->next = alias_list;
    alias_list = new_alias;
    active_aliases++;
}

// Function to find an alias in the alias list
char *find_alias(char *name) {
    struct Alias *current_alias = alias_list;
    while (current_alias != NULL) {
        if (strcmp(current_alias->name, name) == 0) {
            return current_alias->command;
        }
        current_alias = current_alias->next;
    }
    return NULL;
}

void remove_alias(char *name) {
    struct Alias *current_alias = alias_list;
    struct Alias *previous_alias = NULL;
    while (current_alias != NULL) {
        if (strcmp(current_alias->name, name) == 0) {
            if (previous_alias == NULL) {
                alias_list = current_alias->next;
            } else {
                previous_alias->next = current_alias->next;
            }
            free(current_alias);
            active_aliases--;
            successful_commands++;
            return;
        }
        previous_alias = current_alias;
        current_alias = current_alias->next;
    }
    printf("Alias '%s' not found.\n", name);
}


// Function to print all defined aliases or execute alias command if "alias" alias exists
void print_or_execute_alias() {
    struct Alias *current_alias = alias_list;
    char *alias_command = find_alias("alias");

    if (alias_command != NULL) {
        system(alias_command);
    } else {
        while (current_alias != NULL) {
            printf("%s='%s'\n", current_alias->name, current_alias->command);
            current_alias = current_alias->next;
        }
    }
}

// Function to parse the command into arguments
void parse_command(char *command, char **args) {
    int arg_count = 0;
    char *ptr = command;
    int in_quotes = 0;
    char *start = NULL;

    while (*ptr != '\0') {
        while (isspace(*ptr)) {
            ptr++;
        }
        if (*ptr == '\0') {
            break;
        }

        if (*ptr == '\"' || *ptr == '\'') {
            in_quotes = 1;
            char quote_type = *ptr;
            ptr++;
            start = ptr;
            while (*ptr != '\0' && (*ptr != quote_type || in_quotes)) {
                if (*ptr == quote_type) {
                    in_quotes = 0;
                    *ptr = '\0';
                    ptr++;
                    break;
                }
                ptr++;
            }
            args[arg_count++] = start;
        } else {
            start = ptr;
            while (*ptr != '\0' && !isspace(*ptr)) {
                ptr++;
            }
            if (*ptr != '\0') {
                *ptr = '\0';
                ptr++;
            }
            args[arg_count++] = start;
        }
    }

    args[arg_count] = NULL;
}

void process_alias_command(char *command) {
    // Remove "alias" prefix and leading spaces
    char *alias_definition = command + 6; // Skip the "alias " part
    while (isspace(*alias_definition)) {
        alias_definition++;
    }

    // Find '=' and ensure it's surrounded by a valid alias name and command
    char *equal_sign_position = strchr(alias_definition, '=');
    if (equal_sign_position == NULL) {
        fprintf(stderr, "Invalid alias command format.\n");
        return;
    }

    // Split alias name and command
    *equal_sign_position = '\0';
    char *alias_name = alias_definition;
    char *alias_command = equal_sign_position + 1;

    // Trim spaces around alias name
    while (isspace(*alias_name)) {
        alias_name++;
    }
    char *end_alias_name = alias_name + strlen(alias_name) - 1;
    while (end_alias_name > alias_name && isspace(*end_alias_name)) {
        *end_alias_name-- = '\0';
    }

    // Trim spaces around alias command
    while (isspace(*alias_command)) {
        alias_command++;
    }
    char *end_alias_command = alias_command + strlen(alias_command) - 1;
    while (end_alias_command > alias_command && isspace(*end_alias_command)) {
        *end_alias_command-- = '\0';
    }

    // Ensure alias command is properly quoted
    if (*alias_command != '\'' || *end_alias_command != '\'') {
        fprintf(stderr, "Invalid alias command format. Alias command must be properly quoted.\n");
        return;
    }

    alias_command++; // Remove starting quote
    *end_alias_command = '\0'; // Remove ending quote

    add_alias(alias_name, alias_command);

    quoted_commands++;
    successful_commands++;
}

// Function to expand aliases in the command
void expand_aliases(char *command) {
    char *first_space = strchr(command, ' ');
    char alias_name[D];

    if (first_space != NULL) {
        size_t alias_length = first_space - command;
        strncpy(alias_name, command, alias_length);
        alias_name[alias_length] = '\0';
    } else {
        strcpy(alias_name, command);
    }

    char *expanded_command = find_alias(alias_name);
    if (expanded_command != NULL) {
        if (first_space != NULL) {
            char new_command[D];
            snprintf(new_command, D, "%s%s", expanded_command, first_space);
            strcpy(command, new_command);
        } else {
            strcpy(command, expanded_command);
        }
    }
}


// Function to add a job to the job list
void add_job(pid_t pid, char *command) {
    struct Job *new_job = (struct Job *)malloc(sizeof(struct Job));
    if (new_job == NULL) {
        fprintf(stderr, "Memory allocation failed for job.\n");
        exit(1);
    }

    new_job->id = next_job_id++;
    new_job->pid = pid;
    // Store the full command, trimming any leading/trailing spaces
    snprintf(new_job->command, D, "%s", command);
    new_job->next = job_list;
    job_list = new_job;
}

// Function to clean up finished jobs
void clean_up_jobs() {
    struct Job *current_job = job_list;
    struct Job *prev_job = NULL;

    while (current_job != NULL) {
        int status;
        pid_t result = waitpid(current_job->pid, &status, WNOHANG);

        if (result == 0) {
            prev_job = current_job;
            current_job = current_job->next;
        } else if (result == current_job->pid) {
            // Remove the finished job from the list
            if (prev_job == NULL) {
                job_list = current_job->next;
            } else {
                prev_job->next = current_job->next;
            }
            free(current_job);
            current_job = (prev_job == NULL) ? job_list : prev_job->next;
        } else {
            perror("waitpid");
            return;
        }
    }
}

// Function to print the list of background jobs in the correct order
void print_jobs() {
    clean_up_jobs(); // Clean up finished jobs before printing

    // Count the number of jobs
    int job_count = 0;
    struct Job *current_job = job_list;
    while (current_job != NULL) {
        job_count++;
        current_job = current_job->next;
    }

    // Create an array to hold job pointers for sorted display
    struct Job *job_array[job_count];
    current_job = job_list;
    for (int i = 0; i < job_count; i++) {
        job_array[i] = current_job;
        current_job = current_job->next;
    }

    // Sort jobs by their IDs in ascending order
    for (int i = 0; i < job_count - 1; i++) {
        for (int j = i + 1; j < job_count; j++) {
            if (job_array[i]->id > job_array[j]->id) {
                struct Job *temp = job_array[i];
                job_array[i] = job_array[j];
                job_array[j] = temp;
            }
        }
    }

    // Print the jobs in sorted order with full command
    for (int i = 0; i < job_count; i++) {
        printf("[%d]               %s&\n", job_array[i]->id, job_array[i]->command);
    }
    successful_commands++;
}

// Function to set and display a custom error message
void set_and_perror(const char *message) {
    char custom_error[1024];
    strncpy(custom_error, message, sizeof(custom_error) - 1);
    custom_error[sizeof(custom_error) - 1] = '\0';

    errno = EINVAL;
    fprintf(stderr, "%s\n", custom_error);
}

// Function to handle commands within parentheses
void handle_parentheses(char *command, int check, char *stderr_filename) {
    char *open_paren = strchr(command, '(');
    char *close_paren = strchr(command, ')');

    if (open_paren != NULL && close_paren != NULL && close_paren > open_paren) {
        *open_paren = '\0';
        *close_paren = '\0';

        char inner_command[D];
        snprintf(inner_command, D, "%s", open_paren + 1);

        handle_command(inner_command, 0, check, stderr_filename);

        char new_command[D];
        snprintf(new_command, D, "%s %s", command, close_paren + 1);
        strcpy(command, new_command);
    }
}

void handle_command(char *command, int from_script, int check, char *stderr_filename) {
    char *args[D / 2 + 1]; // Maximum number of arguments
    int command_success = 1;

    // Validate if the command contains matching quotes
    int double_quotes_count = 0;
    int single_quotes_count = 0;
    char *ptr = command;
    while (*ptr != '\0') {
        if (*ptr == '\"') {
            double_quotes_count++;
        } else if (*ptr == '\'') {
            single_quotes_count++;
        }
        ptr++;
    }

    if (double_quotes_count % 2 != 0 || single_quotes_count % 2 != 0) {
        fprintf(stderr, "Illegal input: unmatched quotes\n");
        return;
    }

    // Handle alias definitions early
    if (strncmp(command, "alias ", 6) == 0) {
        process_alias_command(command);
        return;
    }

    // Handle unalias command early
    if (strncmp(command, "unalias ", 8) == 0) {
        char *alias_name = command + 8;
        while (isspace(*alias_name)) {
            alias_name++;
        }
        remove_alias(alias_name);
        return;
    }

    // Handle jobs command early
    if (strcmp(command, "jobs") == 0) {
        print_jobs();
        return;
    }

    // Handle parentheses in the command
    if (strchr(command, '(') != NULL && strchr(command, ')') != NULL) {
        handle_parentheses(command, check, stderr_filename);
    }

    // Check for '&' at the end of the command
    int background = 0;
    size_t len = strlen(command);
    if (len > 0 && command[len - 1] == '&') {
        background = 1;
        command[len - 1] = '\0';
    }

    // Make a copy of the command to keep the original unmodified for background job storage
    char full_command[D];
    snprintf(full_command, D, "%s", command);

    // Split command into parts by '&&' and '||'
    char *cmd = command;
    char *next_cmd = NULL;
    char *operator = NULL;
    int execute_next = 1;

    while (cmd != NULL && execute_next) {
        char *and_pos = strstr(cmd, "&&");
        if(and_pos != NULL){
            checking = 1;
        }
        char *or_pos = strstr(cmd, "||");
        if(or_pos == NULL){
            checking = 0;
        }

        if (and_pos != NULL && (or_pos == NULL || and_pos < or_pos)) {
            *and_pos = '\0';
            next_cmd = and_pos + 2;
            operator = "&&";
        } else if (or_pos != NULL) {
            *or_pos = '\0';
            next_cmd = or_pos + 2;
            operator = "||";
        } else {
            next_cmd = NULL;
            operator = NULL;
        }

        while (isspace(*cmd)) cmd++;
        char *end_cmd = cmd + strlen(cmd) - 1;
        while (end_cmd > cmd && isspace(*end_cmd)) *end_cmd-- = '\0';

        expand_aliases(cmd);

        parse_command(cmd, args);

        pid_t pid;
        int argcount = 0;
        for (int i = 0; args[i] != NULL; i++) {
            argcount++;
        }

        // Handle internal commands directly to avoid execvp call
        if (args[0] != NULL && strcmp(args[0], "jobs") == 0) {
            print_jobs();
            command_success = 1; // Consider this a success
        } else if (argcount > 5) {
            if (check) {
                // Open the stderr file in "w" mode to clear its contents before appending the new error
                FILE *file = fopen(stderr_filename, "w");
                if (file != NULL) {
                    fprintf(file, "ERR: too many arguments\n");
                    fclose(file);
                } else {
                    perror("fopen");
                }
            } else {
                printf("ERR: too many arguments\n");
            }
            command_success = 0;
        } else {
            if (args[0] == NULL) {
                command_success = 0;
            } else {
                pid = fork();
                if (pid < 0) {
                    perror("fork");
                    exit(1);
                } else if (pid == 0) { // Child process
                    if (check && stderr_filename != NULL) {
                        // Open the stderr file in "w" mode to clear its contents before redirecting stderr
                        FILE *file = fopen(stderr_filename, "w");
                        if (file == NULL) {
                            perror("fopen");
                            exit(1);
                        }
                        // Redirect stderr to the file
                        dup2(fileno(file), STDERR_FILENO);
                        fclose(file);
                    }

                    execvp(args[0], args);
                    perror("execvp");
                    _exit(1);
                } else { // Parent process
                    if (background) {
                        add_job(pid, full_command); // Add the job with the full command
                        successful_commands++; // Increment successful_commands for background jobs
                    } else {
                        int status;
                        if (waitpid(pid, &status, 0) == -1) {
                            perror("wait");
                            command_success = 0;
                        } else {
                            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                                successful_commands++;
                                if (double_quotes_count + single_quotes_count > 0) {
                                    quoted_commands++;
                                }
                                command_success = 1;
                            } else {
                                command_success = 0;
                            }
                        }
                    }
                }
            }
        }

        if (operator != NULL) {
            if (strcmp(operator, "&&") == 0) {
                execute_next = command_success;
            } else if (strcmp(operator, "||") == 0) {
                execute_next = !command_success;
            }
        }
        if(checking == 1){
            if(strcmp(operator, "||") == 0){
                checking = 0;
                cmd = next_cmd;
                continue;
            }
            if(execute_next == 1){
                checking = 0;
                cmd = next_cmd;
                continue;
            }
            checking = 0;
            execute_next = 1;
            next_cmd = or_pos +2;
        }
        cmd = next_cmd;
    }
}



void process_script_file(char *filename, int check, char *stderr_filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Failed to open script file");
        printf("#cmd:%d|#alias:%d|#script lines:%d> ", successful_commands, active_aliases, total_script_lines);
        return;
    }

    char command[D];
    int line_number = 0;

    if (fgets(command, D, file) == NULL || strcmp(command, "#!/bin/bash\n") != 0) {
        printf("ERR\n");
        fclose(file);
        return;
    }

    while (fgets(command, D, file) != NULL) {
        line_number++;
        total_script_lines++;

        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

        // Check for '2>' redirection in the script command
        char *stderr_redirect = strstr(command, "2>");
        if (stderr_redirect != NULL) {
            *stderr_redirect = '\0'; // Split the command at '2>'
            stderr_filename = stderr_redirect + 2; // The filename follows '2>'

            // Trim leading spaces from the filename
            while (isspace(*stderr_filename)) {
                stderr_filename++;
            }

            check = 1; // Enable redirection flag
        }

        handle_command(command, 1, check, stderr_filename);
    }

    fclose(file);
}


// Function to free the alias list memory
void free_alias_list() {
    struct Alias *current_alias = alias_list;
    while (current_alias != NULL) {
        struct Alias *temp = current_alias;
        current_alias = current_alias->next;
        free(temp);
    }
    alias_list = NULL;
}

// Function to free the job list memory
void free_job_list() {
    struct Job *current_job = job_list;
    while (current_job != NULL) {
        struct Job *temp = current_job;
        current_job = current_job->next;
        free(temp); // Free the memory allocated for the job
    }
    job_list = NULL;
}


int main() {
    int stderr_redirection = 0;
    char *stderr_filename = NULL;
    while (1) {
        printf("#cmd:%d|#alias:%d|#script lines:%d> ", successful_commands, active_aliases, total_script_lines);
        fflush(stdout);

        char command[D];
        command[1024] = '\0';

        if (fgets(command, D, stdin) == NULL) {
            break;
        }

        size_t len = strlen(command);
        if (len == 1024 && command[1023] != '\n') {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
            printf("Input too long. Please enter a shorter command.\n");
            continue;
        }

        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

        if (strlen(command) == 0) {
            continue;
        }

        if (strcmp(command, "exit_shell") == 0) {
            printf("%d", quoted_commands);
            break;
        }

        if (strncmp(command, "source ", 7) == 0) {
            char *filename = command + 7;
            while (isspace(*filename)) {
                filename++;
            }
            process_script_file(filename, stderr_redirection, stderr_filename);
            continue;
        }

        // Make a copy of the command to keep the original unmodified for background job storage
        char full_command[D];
        snprintf(full_command, D, "%s", command);

        // Check for '2>' redirection
        char *stderr_redirect = strstr(command, "2>");
        stderr_filename = NULL;
        stderr_redirection = 0;

        if (stderr_redirect != NULL) {
            *stderr_redirect = '\0'; // Split the command at '2>'
            stderr_filename = stderr_redirect + 2; // The filename follows '2>'

            // Trim leading spaces from the filename
            while (isspace(*stderr_filename)) {
                stderr_filename++;
            }

            stderr_redirection = 1;
        }

        handle_command(command, 0, stderr_redirection, stderr_filename);
    }

    free_alias_list();
    free_job_list();

    return 0;
}