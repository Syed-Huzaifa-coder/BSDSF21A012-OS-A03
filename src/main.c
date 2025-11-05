#include <fcntl.h>
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_JOBS 50

// ---------- Custom History ----------
char* history[HISTORY_SIZE];
int history_count = 0;

void add_to_history_custom(const char* cmdline) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmdline);
    } else {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++)
            history[i - 1] = history[i];
        history[HISTORY_SIZE - 1] = strdup(cmdline);
    }
}

// ---------- Built-in Command Completion ----------
const char* builtin_commands[] = {
    "exit", "cd", "help", "jobs", "history", NULL
};

char* command_generator(const char* text, int state) {
    static int list_index, len;
    const char* name;

    if (!state) {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = builtin_commands[list_index++]) != NULL) {
        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }
    return NULL;
}

char** my_completion(const char* text, int start, int end) {
    if (start == 0)
        return rl_completion_matches(text, command_generator);
    return rl_completion_matches(text, rl_filename_completion_function);
}

// ---------- Background Jobs ----------
pid_t bg_jobs[MAX_JOBS];
int bg_count = 0;

void reap_background_jobs() {
    int status;
    for (int i = 0; i < bg_count;) {
        pid_t pid = waitpid(bg_jobs[i], &status, WNOHANG);
        if (pid > 0) {
            printf("[Done] PID: %d\n", bg_jobs[i]);
            for (int j = i; j < bg_count - 1; j++)
                bg_jobs[j] = bg_jobs[j + 1];
            bg_count--;
        } else {
            i++;
        }
    }
}

// ---------- Strip Quotes for Echo ----------
void strip_quotes(char* str) {
    int len = strlen(str);
    if ((str[0] == '"' && str[len - 1] == '"') ||
        (str[0] == '\'' && str[len - 1] == '\'')) {
        str[len - 1] = '\0';
        memmove(str, str + 1, len - 1);
    }
}

// ---------- Display Background Jobs ----------
void builtin_jobs() {
    if (bg_count == 0) {
        printf("No background jobs.\n");
    } else {
        for (int i = 0; i < bg_count; i++)
            printf("[Running] PID: %d\n", bg_jobs[i]);
    }
}

// ---------- Execute Command and Return Exit Code ----------
int execute_command_with_status(char* command) {
    pid_t pid = fork();
    if (pid == 0) { // Child
        char* args[MAXARGS + 1];
        char* input_file = NULL;
        char* output_file = NULL;

        parse_redirection(command, args, &input_file, &output_file);

        // No pipes here for if condition — straightforward command
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) exit(1);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) exit(1);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(args[0], args);
        perror("execvp");  // Only reached if execvp fails
        exit(1);           // Failed execution => return non-zero
    }
    else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
    else {
        perror("fork");
        return -1;
    }
}

// ---------- Main Shell ----------
int main() {
    rl_attempted_completion_function = my_completion;
    char* cmdline;

    while ((cmdline = readline(PROMPT)) != NULL) {
        if (strlen(cmdline) == 0) {
            free(cmdline);
            continue;
        }

        reap_background_jobs();

        // Handle history execution like !3
        if (cmdline[0] == '!') {
            int index = atoi(&cmdline[1]) - 1;
            if (index < 0 || index >= history_count) {
                printf("Error: No such command in history\n");
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(history[index]);
            printf("%s\n", cmdline);
        }

        add_history(cmdline);
        add_to_history_custom(cmdline);

        // ---------- IF-THEN-ELSE-FI BLOCK ----------
        if (strncmp(cmdline, "if ", 3) == 0) {
            char* if_cmd = strdup(cmdline + 3);
            char* then_block = NULL;
            char* else_block = NULL;
            int has_then = 0;
            int has_else = 0;

            while (1) {
                char* next_line = readline("> ");
                if (!next_line) break;

                if (strcmp(next_line, "then") == 0) {
                    has_then = 1;
                    then_block = strdup("");
                } else if (strcmp(next_line, "else") == 0) {
                    if (!has_then) {
                        printf("Error: 'then' expected.\n");
                        free(next_line);
                        break;
                    }
                    has_else = 1;
                    else_block = strdup("");
                } else if (strcmp(next_line, "fi") == 0) {
                    free(next_line);
                    break;
                } else {
                    char** target_block = has_else ? &else_block : &then_block;
                    if (*target_block) {
                        *target_block = realloc(*target_block, strlen(*target_block) + strlen(next_line) + 2);
                        strcat(*target_block, next_line);
                        strcat(*target_block, "\n");
                    }
                }
                free(next_line);
            }

            int status = execute_command_with_status(if_cmd);
            if (status == 0 && then_block) {
                execute_io_pipe(then_block);
            } else if (status != 0 && has_else && else_block) {
                execute_io_pipe(else_block);
            }

            free(if_cmd);
            free(then_block);
            free(else_block);
            free(cmdline);
            continue;
        }

        // ---------- Split commands by ';' ----------
        char* cmd_copy = strdup(cmdline);
        char* token = strtok(cmd_copy, ";");

        while (token != NULL) {
            while (*token == ' ') token++;
            char* end = token + strlen(token) - 1;
            while (end > token && (*end == ' ' || *end == '\n')) *end-- = '\0';

            if (strlen(token) > 0) {
                int background = 0;
                end = token + strlen(token) - 1;

                if (*end == '&') {
                    background = 1;
                    *end = '\0';
                    while (end > token && (*(end - 1) == ' ')) *(--end) = '\0';
                }

                char** arglist = tokenize(token);
                if (arglist != NULL) {
                    if (!handle_builtin(arglist)) {
                        if (strcmp(arglist[0], "echo") == 0 && arglist[1])
                            strip_quotes(arglist[1]);

                        pid_t pid = fork();
                        if (pid == 0) {
                            execute_io_pipe(token);
                            exit(0);
                        } else if (pid > 0) {
                            if (background) {
                                printf("[Background] PID: %d\n", pid);
                                if (bg_count < MAX_JOBS)
                                    bg_jobs[bg_count++] = pid;
                            } else {
                                waitpid(pid, NULL, 0);
                            }
                        } else {
                            perror("fork");
                        }

                        for (int i = 0; arglist[i] != NULL; i++)
                            free(arglist[i]);
                        free(arglist);
                    } else if (strcmp(arglist[0], "jobs") == 0) {
                        builtin_jobs();
                    }
                }
            }
            token = strtok(NULL, ";");
        }

        free(cmd_copy);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    for (int i = 0; i < history_count; i++)
        free(history[i]);

    return 0;
}

