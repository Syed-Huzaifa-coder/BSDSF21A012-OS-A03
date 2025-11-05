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

        // ---------- Split commands by ';' ----------
        char* cmd_copy = strdup(cmdline);
        char* token = strtok(cmd_copy, ";");

        while (token != NULL) {
            while (*token == ' ') token++; // Trim leading spaces
            char* end = token + strlen(token) - 1;
            while (end > token && (*end == ' ' || *end == '\n')) *end-- = '\0';

            if (strlen(token) > 0) {
                int background = 0;
                end = token + strlen(token) - 1;

                if (*end == '&') { // Background execution
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
                        if (pid == 0) { // Child
                            execute_io_pipe(token); // Handles pipes & redirection
                            exit(0);
                        } else if (pid > 0) { // Parent
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

