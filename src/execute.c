#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_CMDS 10

// Helper: split command by pipe '|'
int split_commands(char* cmdline, char* commands[]) {
    int count = 0;
    char* token = strtok(cmdline, "|");
    while (token && count < MAX_CMDS) {
        while (*token == ' ') token++; // Trim leading spaces
        commands[count++] = token;
        token = strtok(NULL, "|");
    }
    return count;
}

// Parse input/output redirection in a single command
void parse_redirection(char* cmd, char** args, char** input_file, char** output_file) {
    *input_file = NULL;
    *output_file = NULL;
    int argnum = 0;

    char* token = strtok(cmd, " \t\n");
    while (token) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\n");
            *input_file = token;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\n");
            *output_file = token;
        } else {
            args[argnum++] = token;
        }
        token = strtok(NULL, " \t\n");
    }
    args[argnum] = NULL;
}

int execute_io_pipe(char* cmdline) {
    char* commands[MAX_CMDS];
    int num_cmds = split_commands(cmdline, commands);
    int pipefd[2], prev_fd = -1;

    for (int i = 0; i < num_cmds; i++) {
        char* args[MAXARGS + 1];
        char* input_file;
        char* output_file;

        parse_redirection(commands[i], args, &input_file, &output_file);

        if (i < num_cmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }

        if (pid == 0) { // Child process
            // Input redirection
            if (input_file) {
                int fd = open(input_file, O_RDONLY);
                if (fd < 0) { perror("open"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            } else if (prev_fd != -1) { // Read from previous pipe
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            // Output redirection
            if (output_file) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("open"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else if (i < num_cmds - 1) { // Write to next pipe
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            // Close unused pipe ends in child
            if (pipefd[0]) close(pipefd[0]);
            if (pipefd[1]) close(pipefd[1]);

            // Execute command
            if (execvp(args[0], args) < 0) {
                perror("execvp");
                exit(1);
            }
        } else { // Parent process
            if (prev_fd != -1) close(prev_fd);
            if (i < num_cmds - 1) {
                close(pipefd[1]); // close write end
                prev_fd = pipefd[0]; // read end for next command
            }
            waitpid(pid, NULL, 0);
        }
    }

    return 0;
}

