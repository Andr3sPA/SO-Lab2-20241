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

char *path[] = {"/usr/bin/"};

char *inputLine;
char *filePath;
char *token;

struct
{
    char **args;
    int size;
    int capacity;
} command;

int exitCode = 0;

/* 0 false, 1 true */
int checkAccess(char *filename, char **filePath)
{
    int canAccess;
    for (int i = 0; i < sizeof(path) / sizeof(path[0]); i++)
    {
        char cat[strlen(path[i]) + strlen(filename)];
        strcpy(cat, path[i]);
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

int main(int argc, char const *argv[])
{
    inputLine = malloc(sizeof(char) * LINE_MAX);
    filePath = malloc(sizeof(char) * LINE_MAX);

    command.size = 0;
    command.capacity = 10;
    command.args = malloc(command.capacity * sizeof(char *));

    while (1)
    {
        // memset(command.args, NULL, sizeof(char) * command.size);
        command.size = 0;

        printf("%swish> %s", MAGENTA, RESET);

        fgets(inputLine, LINE_MAX, stdin);

        if (inputLine == NULL)
        {
            fprintf(stderr, "error: %s", strerror(errno));
            exitCode = 1;
            break;
        }

        token = strtok(inputLine, " \n");
        if (token == NULL)
            continue;

        while (token != NULL)
        {
            if (command.size == 0)
            {
                command.args[0] = token;
                command.size++;
                token = strtok(NULL, " \n");
                continue;
            }
            command.args[command.size] = token;
            command.size++;

            if (command.size == command.capacity)
            {
                command.capacity *= 2;
                command.args = realloc(command.args, sizeof(char *) * command.capacity);
            }

            printf("DEBUG: arg found: %s\n", token);
            token = strtok(NULL, " \n");
        }
        command.args[command.size] = NULL;

        if (strcmp(command.args[0], "exit") == 0)
        {
            printf("%sbyeee\n%s", CYAN, RESET);
            break;
        }
        else if (checkAccess(command.args[0], &filePath))
        {
            pid_t child = fork();
            if (child == -1)
            {
                fprintf(stderr, "%serror: %s%s\n", RED, strerror(errno), RESET);
                exitCode = 1;
                break;
            }
            if (child == 0)
                execv(filePath, command.args);
            else
                waitpid(child, NULL, 0);
        }
        else
        {
            fprintf(stderr, "%serror:\n\tcommand '%s' not found\n", RED, command.args[0]);
        }
    }

    free(inputLine);
    free(filePath);
    exit(exitCode);
}
