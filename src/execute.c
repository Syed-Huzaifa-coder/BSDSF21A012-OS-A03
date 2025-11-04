#include "shell.h"
#include <string.h>  // Add this for strcmp function
#include <stdlib.h>  // Add this for exit function

int execute(char* arglist[]) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;  // Nothing to execute
    }

    // Built-in command: exit
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting FCIT Shell...\n");
        exit(0);
    }

    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            execvp(arglist[0], arglist);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            return 0;
    }
}

