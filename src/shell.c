#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <sys/wait.h>
#include <ctype.h>

extern pid_t bg_jobs[MAX_JOBS];
extern int bg_count;
extern char* history[HISTORY_SIZE];
extern int history_count;

// ---------- Tokenize command into arguments ----------
char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0') return NULL;

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip whitespace
        if (*cp == '\0') break;

        if (*cp == '<' || *cp == '>' || *cp == '|' || *cp == '&') {
            arglist[argnum][0] = *cp;
            arglist[argnum][1] = '\0';
            argnum++;
            cp++;
        } else {
            char* start = cp;
            int len = 0;
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' &&
                   *cp != '<' && *cp != '>' && *cp != '|' && *cp != '&') {
                cp++;
                len++;
            }
            strncpy(arglist[argnum], start, len);
            arglist[argnum][len] = '\0';
            argnum++;
        }
    }

    if (argnum == 0) {
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

// ---------- Built-in commands ----------
int handle_builtin(char** arglist) {
    if (arglist[0] == NULL) return 0;

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else if (chdir(arglist[1]) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        exit(0);
    }

    if (strcmp(arglist[0], "history") == 0) {
        for (int i = 0; i < history_count; i++)
            printf("%d %s\n", i + 1, history[i]);
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        if (bg_count == 0) {
            printf("No background jobs.\n");
        } else {
            int i = 0;
            while (i < bg_count) {
                int status;
                pid_t pid = waitpid(bg_jobs[i], &status, WNOHANG);
                if (pid == 0) {
                    printf("[Running] PID: %d\n", bg_jobs[i]);
                    i++;
                } else if (pid > 0) {
                    printf("[Done] PID: %d\n", bg_jobs[i]);
                    for (int j = i; j < bg_count - 1; j++)
                        bg_jobs[j] = bg_jobs[j + 1];
                    bg_count--;
                } else {
                    perror("waitpid");
                    i++;
                }
            }
        }
        return 1;
    }

    return 0; // Not a built-in
}

// ---------- Execute command with optional background ----------
void execute_command(char** arglist, int background) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) { // Child
        execvp(arglist[0], arglist);
        perror("execvp"); // If execvp fails
        exit(1);
    } else { // Parent
        if (background) {
            bg_jobs[bg_count++] = pid;
            printf("[Background] PID: %d\n", pid);
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

// ---------- Split input line by ; and handle & ----------
void process_input(char* line) {
    char* cmd = strtok(line, ";");
    while (cmd != NULL) {
        // Trim whitespace
        while (*cmd == ' ' || *cmd == '\t') cmd++;

        // Check if ends with &
        int background = 0;
        int len = strlen(cmd);
        if (len > 0 && cmd[len - 1] == '&') {
            background = 1;
            cmd[len - 1] = '\0'; // Remove &
        }

        // Tokenize and execute
        char** args = tokenize(cmd);
        if (args != NULL) {
            if (!handle_builtin(args)) {
                execute_command(args, background);
            }
            for (int i = 0; i < MAXARGS + 1 && args[i] != NULL; i++)
                free(args[i]);
            free(args);
        }

        cmd = strtok(NULL, ";");
    }

    // Reap any finished background jobs
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

