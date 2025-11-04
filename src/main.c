#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* history[HISTORY_SIZE];
int history_count = 0;

void add_to_history(const char* cmdline) {
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

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
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

        // Store command in history
        if (strlen(cmdline) > 0) {
            add_to_history(cmdline);
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            // Check built-in commands first
            if (!handle_builtin(arglist)) {
                execute(arglist); // external commands
            }

            // Free memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++)
                free(arglist[i]);
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");

    // Free history
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }

    return 0;
}
