#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <bits/posix2_lim.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INIT_ARR(arrStruct, arrField, arrType) \
    arrStruct.size = 0;                        \
    arrStruct.capacity = 2;                    \
    arrStruct.arrField = calloc(sizeof(arrType), arrStruct.capacity);

#define REALLOC(arrStruct, arrField, arrType)                                                   \
    if (arrStruct.size == arrStruct.capacity)                                                   \
    {                                                                                           \
        arrStruct.capacity *= 2;                                                                \
        arrStruct.arrField = realloc(arrStruct.arrField, sizeof(arrType) * arrStruct.capacity); \
    }

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

struct
{
    char **entries;
    int size;
    int capacity;
} path;

char *inputLine;
char *filePath;
char *token;

struct Command
{
    char **args;
    int size;
    int capacity;
};

struct
{
    struct Command **singleCommands;
    int size;
    int capacity;
} commands;

int exitCode;
int batchProcessing;

/* 0 false, 1 true */
int checkAccess(char *filename, char **filePath)
{
    int canAccess;
    canAccess = 1 + access(filename, X_OK);
    if (canAccess)
    {
        strcpy(*filePath, filename);
        return canAccess;
    }

    for (int i = 0; i < path.size; i++)
    {
        char cat[strlen(path.entries[i]) + strlen(filename) + 1];
        strcpy(cat, path.entries[i]);
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
int processCommand(struct Command *command)
{
    if (strcmp(command->args[0], "exit") == 0)
    {
        if (command->size > 1)
        {
            fprintf(stderr, "error:\n\tcommand 'exit' does not accept arguments\n");
            return 1;
        }
        if (!batchProcessing)
            printf("%sbyeee\n%s", CYAN, RESET);
        return 0;
    }

    else if (strcmp(command->args[0], "cd") == 0)
    {
        if (command->size != 2)
        {
            fprintf(stderr, "error:\n\tcommand 'cd' must have exactly one argument\n");
            return 1;
        }
        if (chdir(command->args[1]) == -1)
        {
            fprintf(stderr, "error:\n\tcannot execute command 'cd': %s\n", strerror(errno));
            return 1;
        }
    }

    else if (strcmp(command->args[0], "path") == 0)
    {
        // delete all entries in path
        for (int i = 0; i < path.size; i++)
        {
            free(path.entries[i]);
        }
        path.size = 0;

        // copy all paths from command to path
        for (int i = 1; i < command->size; i++)
        {
            path.entries[path.size] = malloc(strlen(command->args[i]) * sizeof(char));
            strcpy(path.entries[path.size++], command->args[i]);
        }
    }
    else if (strcmp(command->args[0], "cls") == 0)
    {
        pid_t child = fork();
        if (child == 0)
            execv("/usr/bin/clear", command->args);
        else
            waitpid(child, NULL, 0);
    }
    else
    {
        // Busca el símbolo de redirección en los argumentos del comando
        int redirectIndex = -1;
        for (int i = 0; i < command->size; i++)
        {

            // Si se encuentra el símbolo de redirección
            if (strcmp(command->args[i], ">") == 0)
            {
                redirectIndex = i;
                if (redirectIndex == 0)
                {
                    fprintf(stderr, "error:\n\tthe redirection must have a command to redirect its output\n");
                    return 1;
                }

                // Verifica si hay un nombre de archivo de salida después del símbolo de redirección
                if (command->size - (redirectIndex + 1) != 1)
                {
                    fprintf(stderr, "error:\n\tthe redirection must have only one output file\n");
                    return 1;
                }

                command->args[redirectIndex] = NULL; // to call execv (see docs)
                break;
            }
        }

        // check if command can be executed in any path entry
        if (checkAccess(command->args[0], &filePath))
        {

            pid_t child = fork();
            if (child == -1)
            {
                fprintf(stderr, "error: %s\n", strerror(errno));
                exitCode = 1;
                return 0;
            }
            if (child == 0)
            {
                if (redirectIndex != -1)
                {
                    int redirectFileDescriptor = open(command->args[redirectIndex + 1], O_RDWR | O_CREAT | S_IRUSR | S_IWUSR);
                    dup2(redirectFileDescriptor, 1); // stdout
                    dup2(redirectFileDescriptor, 2); // stderr
                    close(redirectFileDescriptor);
                }
                execv(filePath, command->args);
            }
            // else
            // waitpid(child, NULL, 0);
        }
        else
        {
            if (batchProcessing)
                fprintf(stderr, "error:\n\tcommand '%s' not found\n", command->args[0]);
            else
                fprintf(stderr, "%serror:\n\tcommand '%s' not found\n", RED, command->args[0]);
        }
    }

    return 1; // Indica que el comando se procesó correctamente
}

void runShell(FILE *inputFile)
{
    while (1)
    {
        commands.size = 0;

        if (!batchProcessing)
        {
            char *cwd = getcwd(NULL, 0);
            printf("%s%s%s\n", CYAN, cwd, RESET);
            printf("%swish> %s", MAGENTA, RESET);
            free(cwd);
        }

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
        if (token == NULL || strcmp(token, "&") == 0)
            continue;

        struct Command *singleCommand = commands.singleCommands[commands.size++]; // start with first single command

        // if first command is not allocated
        if (singleCommand == NULL)
        {
            singleCommand = malloc(sizeof(struct Command));
            INIT_ARR((*singleCommand), args, char *);
            commands.singleCommands[commands.size - 1] = singleCommand;
        }

        // reset first single command
        singleCommand->size = 0;

        while (token != NULL)
        {
            // close current single command and build next one
            if (strcmp(token, "&") == 0)
            {
                // get more space for more single commands if necessary
                REALLOC(commands, singleCommands, struct Command *);

                singleCommand->args[singleCommand->size] = NULL; // to be able to call execv (see man)
                singleCommand = commands.singleCommands[commands.size++];

                // if single command is not allocated
                if (singleCommand == NULL)
                {
                    singleCommand = malloc(sizeof(struct Command));
                    INIT_ARR((*singleCommand), args, char *);
                    commands.singleCommands[commands.size - 1] = singleCommand;
                }

                // reset single command to 0 arguments
                singleCommand->size = 0;

                // next token
                token = strtok(NULL, " \n\t");
                continue;
            }

            singleCommand->args[singleCommand->size] = token;
            singleCommand->size++;

            // realloc single command's arguments if necessary
            REALLOC((*singleCommand), args, char *);

            // next token
            token = strtok(NULL, " \n\t");
        }
        singleCommand->args[singleCommand->size] = NULL; // to be able to call execv (see man)

        // if last single command is empty, ignore it
        if (singleCommand->size == 0)
            commands.size--;

        int truncated = 0;
        for (int i = 0; i < commands.size; i++)
        {
            singleCommand = commands.singleCommands[i];

            if (!processCommand(singleCommand))
            {
                truncated = 1;
                break;
            }
        }
        if (truncated)
            break;

        // wait for all children
        for (int i = 0; i < commands.size; i++)
        {
            wait(NULL);
        }

        // if (!batchProcessing)
        //     printf("\n");
    }
}

int main(int argc, char const *argv[])
{
    inputLine = malloc(sizeof(char) * LINE_MAX);
    filePath = malloc(sizeof(char) * LINE_MAX);

    INIT_ARR(commands, singleCommands, struct Command *);
    INIT_ARR(path, entries, char *);
    path.entries[path.size] = malloc(strlen("/bin") * sizeof(char));
    strcpy(path.entries[path.size++], "/bin");

    exitCode = 0;

    batchProcessing = argc == 2;
    if (argc > 2)
    {
        fprintf(stderr, "error:\n\twish only accepts one file to batch process\n");
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
    // free all path entries
    for (int i = 0; i < path.size; i++)
    {
        free(path.entries[i]);
    }
    free(path.entries);

    // free all single commands that were allocated
    for (int i = 0; i < commands.capacity; i++)
    {
        if (commands.singleCommands[i] != NULL)
        {
            free(commands.singleCommands[i]->args);
            free(commands.singleCommands[i]);
        }
    }
    free(commands.singleCommands);

    free(inputLine);
    free(filePath);
    exit(exitCode);
}