#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <bits/posix2_lim.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define INIT_ARR(arrStruct, arrField) \
    arrStruct.size = 0;               \
    arrStruct.capacity = 2;           \
    arrStruct.arrField = malloc(sizeof(char *) * arrStruct.capacity);

#define REALLOC(arrStruct, arrField)                                                           \
    if (arrStruct.size == arrStruct.capacity)                                                  \
    {                                                                                          \
        arrStruct.capacity *= 2;                                                               \
        arrStruct.arrField = realloc(arrStruct.arrField, sizeof(char *) * arrStruct.capacity); \
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
    // char *workingDir = getcwd(NULL, 0);
    // char cat[strlen(workingDir) + strlen(filename) + 1];
    // strcpy(cat, workingDir);
    // free(workingDir);
    // strcat(cat, "/");
    // strcat(cat, filename);
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
        // delete all entries in path
        for (int i = 0; i < path.size; i++)
        {
            free(path.entries[i]);
        }
        path.size = 0;

        // copy all paths from command to path
        for (int i = 1; i < command.size; i++)
        {
            path.entries[path.size] = malloc(strlen(command.args[i]) * sizeof(char));
            strcpy(path.entries[path.size++], command.args[i]);
        }
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
        FILE *outputFile = NULL; // maybe global?

        // Busca el símbolo de redirección en los argumentos del comando
        int redirectIndex = -1;
        for (int i = 0; i < command.size; i++)
        {
            if (strcmp(command.args[i], ">") == 0)
            {
                redirectIndex = i;
                break;
            }
        }

        // Si se encuentra el símbolo de redirección
        if (redirectIndex != -1)
        {
            // Verifica si hay un nombre de archivo de salida después del símbolo de redirección
            if (command.size - (redirectIndex + 1) != 1)
            {
                fprintf(stderr, "An error has occurred\n");
                return 1;
            }

            // Abre el archivo de salida para redireccionar la salida estándar
            outputFile = freopen(command.args[redirectIndex + 1], "w+", stdout);
            if (outputFile == NULL)
            {
                fprintf(stderr, "error: Cannot open output file '%s': %s\n", command.args[redirectIndex + 1], strerror(errno));
                return 1;
            }

            command.args[redirectIndex] = NULL; // to call execv (see docs)
        }

        pid_t child = fork();
        if (child == -1)
        {
            fprintf(stderr, "error: %s\n", strerror(errno));
            exitCode = 1;
            return 0;
        }
        if (child == 0)
            execv(filePath, command.args);
        else
            waitpid(child, NULL, 0);

        if (redirectIndex != -1 && outputFile != NULL)
        {
            // Cierra el archivo de salida después de terminar de usarlo
            fclose(outputFile);

            // Restaura la salida estándar
            freopen("/dev/tty", "w", stdout);
        }
    }
    else
    {
        if (batchProcessing)
            fprintf(stderr, "error:\n\tcommand '%s' not found\n", command.args[0]);
        else
            fprintf(stderr, "%serror:\n\tcommand '%s' not found\n", RED, command.args[0]);
    }

    return 1; // Indica que el comando se procesó correctamente
}

void runShell(FILE *inputFile)
{
    while (1)
    {
        command.size = 0;

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
        if (token == NULL)
            continue;

        while (token != NULL)
        {
            command.args[command.size] = token;
            command.size++;

            REALLOC(command, args);

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
        // if (!batchProcessing)
        //     printf("\n");
    }
}

int main(int argc, char const *argv[])
{
    inputLine = malloc(sizeof(char) * LINE_MAX);
    filePath = malloc(sizeof(char) * LINE_MAX);

    INIT_ARR(command, args);
    INIT_ARR(path, entries);
    path.entries[path.size] = malloc(strlen("/bin") * sizeof(char));
    strcpy(path.entries[path.size++], "/bin");

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
    free(command.args);
    free(path.entries);
    // FREE_ARR(command, args);
    // FREE_ARR(path, entries);
    free(inputLine);
    free(filePath);
    exit(exitCode);
}