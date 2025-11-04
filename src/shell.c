#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>

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

        if (*cp == '<' || *cp == '>' || *cp == '|') {
            arglist[argnum][0] = *cp;
            arglist[argnum][1] = '\0';
            argnum++;
            cp++;
        } else {
            char* start = cp;
            int len = 0;
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' && *cp != '<' && *cp != '>' && *cp != '|') {
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
int handle_builtin(char **arglist) {
    if (!arglist || !arglist[0]) return 0;

    if (strcmp(arglist[0], "exit") == 0) { printf("Exiting shell...\n"); exit(0); }
    if (strcmp(arglist[0], "cd") == 0) {
        if (!arglist[1]) fprintf(stderr, "cd: missing argument\n");
        else if (chdir(arglist[1]) != 0) perror("cd");
        return 1;
    }
    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n  exit, cd, help, jobs, history, !n\n"); return 1;
    }
    if (strcmp(arglist[0], "jobs") == 0) { printf("Job control not implemented.\n"); return 1; }
    if (strcmp(arglist[0], "history") == 0) {
        for (int i = 0; i < history_count; i++) printf("%d %s\n", i + 1, history[i]);
        return 1;
    }
    return 0;
}

