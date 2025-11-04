#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int execute(char* arglist[]) {
    if (arglist == NULL || arglist[0] == NULL)
        return 0;

    int status;
    pid_t cpid = fork();

    if (cpid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (cpid == 0) {
        // Child process executes external command
        execvp(arglist[0], arglist);
        perror("Command not found");
        exit(1);
    } else {
        // Parent waits for child to finish
        waitpid(cpid, &status, 0);
    }

    return 0;
}
