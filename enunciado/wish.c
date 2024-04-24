#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <bits/posix2_lim.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define WHITE "\x1B[37m"
#define RESET "\x1B[0m"
#define BOLD "\x1B[1m"
#define ITALIC "\x1B[3m"
#define UNDERLINE "\x1B[4m"

char *path[] = {"/usr/bin"};

char *inputLine;
char *filePath;
char *token;

struct
{
    char **args;
    int size;
    int capacity;
} command;

int exitCode;
int batchProcessing;

/* 0 false, 1 true */
int checkAccess(char *filename, char **filePath)
{
    int canAccess;
    char *workingDir = getcwd(NULL, 0);
    char cat[strlen(workingDir) + strlen(filename) + 1];
    strcpy(cat, workingDir);
    free(workingDir);
    strcat(cat, "/");
    strcat(cat, filename);
    canAccess = 1 + access(cat, X_OK);
    if (canAccess)
    {
        strcpy(*filePath, cat);
        return canAccess;
    }

    for (int i = 0; i < sizeof(path) / sizeof(path[0]); i++)
    {
        char cat[strlen(path[i]) + strlen(filename) + 1];
        strcpy(cat, path[i]);
        strcat(cat, "/");
        strcat(cat, filename);
        canAccess = 1 + access(cat, X_OK);
        if (canAccess)
        {
            strcpy(*filePath, cat);
            return canAccess;
        }
    }
    return 0;
}

/* 0 stop shell, 1 continue*/
int processCommand()
{
    if (strcmp(command.args[0], "cd") == 0)
    {
        if (command.size != 2)
        {
            fprintf(stderr, "error:\n\tcommand 'cd' must have exactly one argument\n");
            return 1;
        }
        if (chdir(command.args[1]) == -1)
        {
            fprintf(stderr, "error:\n\tcannot execute command 'cd': %s\n", strerror(errno));
            return 1;
        }
    }

    else if (strcmp(command.args[0], "path") == 0)
    {
        fprintf(stderr, "error:\n\tcommand 'path' is not implemented\n");
    }
    else if (strcmp(command.args[0], "cls") == 0)
    {
        pid_t child = fork();
        if (child == 0)
            execv("/usr/bin/clear", command.args);
        else
            waitpid(child, NULL, 0);
    }
    else if (checkAccess(command.args[0], &filePath))
    {
        pid_t child = fork();
        if (child == -1)
        {
            if (batchProcessing)
                fprintf(stderr, "error: %s\n", strerror(errno));
            else
                fprintf(stderr, "%serror: %s%s\n", RED, strerror(errno), RESET);
            exitCode = 1;
            return 0;
        }
        if (child == 0)
            execv(filePath, command.args);
        else
            waitpid(child, NULL, 0);
    }
    else
    {
        if (batchProcessing)
            fprintf(stderr, "error:\n\tcommand '%s' not found\n", command.args[0]);
        else
            fprintf(stderr, "%serror:\n\tcommand '%s' not found\n", RED, command.args[0]);
    }
    return 1;
}

void runShell(FILE *inputFile)
{
    while (1)
    {
        command.size = 0;

        if (!batchProcessing)
            printf("%swish> %s", MAGENTA, RESET);

        fgets(inputLine, LINE_MAX, inputFile);

        if (inputLine == NULL || feof(inputFile))
        {
            if (batchProcessing)
            {
                break;
            }
            fprintf(stderr, "error: %s", strerror(errno));
            exitCode = 1;
            break;
        }

        token = strtok(inputLine, " \n\t");
        if (token == NULL)
            continue;

        while (token != NULL)
        {
            command.args[command.size] = token;
            command.size++;

            if (command.size == command.capacity)
            {
                command.capacity *= 2;
                command.args = realloc(command.args, sizeof(char *) * command.capacity);
            }

            token = strtok(NULL, " \n\t");
        }
        command.args[command.size] = NULL;

        if (strcmp(command.args[0], "exit") == 0)
        {
            if (command.size > 1)
            {
                fprintf(stderr, "error:\n\tcommand 'exit' does not accept arguments\n");
                continue;
            }
            if (!batchProcessing)
                printf("%sbyeee\n%s", CYAN, RESET);
            break;
        }
        else
        {
            if (!processCommand())
            {
                break;
            }
        }
    }
}

int main(int argc, char const *argv[])
{
    inputLine = malloc(sizeof(char) * LINE_MAX);
    filePath = malloc(sizeof(char) * LINE_MAX);

    command.size = 0;
    command.capacity = 10;
    command.args = malloc(command.capacity * sizeof(char *));

    exitCode = 0;

    batchProcessing = argc == 2;
    if (argc > 2)
    {
        fprintf(stderr, "unimplemented");
        exitCode = 1;
        goto exit;
    }

    if (batchProcessing)
    {
        FILE *commandsFile = fopen(argv[1], "r");
        if (commandsFile == NULL)
        {
            fprintf(stderr, "error:\n\tcannot open file '%s': %s\n", argv[1], strerror(errno));
            exitCode = 1;
        }
        else
        {
            runShell(commandsFile);
            fclose(commandsFile);
        }
    }
    else
    {
        runShell(stdin);
    }

exit:
    free(inputLine);
    free(filePath);
    exit(exitCode);
}
