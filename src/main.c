#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>

// Custom history storage
char* history[HISTORY_SIZE];
int history_count = 0;

// Add command to custom history storage
void add_to_history_custom(const char* cmdline) {
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmdline);
    } else {
        free(history[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history[i - 1] = history[i];
        }
        history[HISTORY_SIZE - 1] = strdup(cmdline);
    }
}

// List of built-in commands for completion
const char* builtin_commands[] = {
    "exit",
    "cd",
    "help",
    "jobs",
    "history",
    NULL
};

// ---------- Custom completion for built-in commands and filenames ----------
char* command_generator(const char* text, int state) {
    static int list_index, len;
    const char* name;

    if (!state) {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = builtin_commands[list_index++]) != NULL) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    return NULL;
}

char **my_completion(const char *text, int start, int end) {
    char **matches = NULL;

    // If at the beginning of the line, complete commands
    if (start == 0) {
        matches = rl_completion_matches(text, command_generator);
    } else {
        // Otherwise, complete filenames
        matches = rl_completion_matches(text, rl_filename_completion_function);
    }

    return matches;
}

// ---------- Main Shell ----------
int main() {
    rl_attempted_completion_function = my_completion;

    char* cmdline;
    char** arglist;

    while ((cmdline = readline(PROMPT)) != NULL) {
        if (strlen(cmdline) == 0) {
            free(cmdline);
            continue;
        }

        // Check for !n syntax before storing to history
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

        // Store command in Readline and custom history
        add_history(cmdline);
        add_to_history_custom(cmdline);

        // Tokenize and execute
        if ((arglist = tokenize(cmdline)) != NULL) {
            if (!handle_builtin(arglist)) {
                execute(arglist); // external commands
            }

            for (int i = 0; arglist[i] != NULL; i++)
                free(arglist[i]);
            free(arglist);
        }

        free(cmdline);
    }

    printf("\nShell exited.\n");

    // Free custom history
    for (int i = 0; i < history_count; i++)
        free(history[i]);

    return 0;
}
