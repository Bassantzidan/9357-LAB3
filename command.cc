#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include <string>
#include <iostream>
#include "command.h"
#include "tokenizer.h"

// SIGCHLD handler: reap finished children and write a line in shell.log
void childHandler(int sig)
{
    int saved = errno;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "Child process %d terminated\n", (int)pid);
        int fd = open("shell.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0)
        {
            write(fd, buf, n);
            close(fd);
        }
    }
    errno = saved;
}

// Print an error, free everything built so far and give up on this line
static void parseError(const char *msg, Command &cmd, SimpleCommand *simple)
{
    fprintf(stderr, "myshell: %s\n", msg);
    cmd.insertSimpleCommand(simple);
    cmd.clear();
}

static bool isWord(Token &t)
{
    return t.type == TOKEN_COMMAND || t.type == TOKEN_ARGUMENT;
}

void parse(std::vector<Token> &tokens)
{
    // only the EOF token -> empty line
    if (tokens.size() <= 1)
        return;

    Command currentCommand;
    SimpleCommand *currentSimpleCommand = new SimpleCommand();

    for (size_t i = 0; i < tokens.size(); i++)
    {
        Token token = tokens[i];

        if (isWord(token))
        {
            std::string value = token.value;
            char q = value[0];

            // the tokenizer splits on spaces, so join the pieces of a quoted string
            if (q == '"' || q == '\'')
            {
                bool closed = value.size() > 1 && value[value.size() - 1] == q;
                while (!closed && i + 1 < tokens.size() && isWord(tokens[i + 1]))
                {
                    i++;
                    value += " " + tokens[i].value;
                    closed = value[value.size() - 1] == q;
                }
                value = value.substr(1);
                if (closed)
                    value = value.substr(0, value.size() - 1);
            }
            currentSimpleCommand->insertArgument(strdup(value.c_str()));
        }
        else if (token.type == TOKEN_REDIRECT || token.type == TOKEN_APPEND ||
                 token.type == TOKEN_INPUT || token.type == TOKEN_ERROR ||
                 token.type == TOKEN_REDIRECT_AND_ERROR)
        {
            // the next token must be a file name
            if (i + 1 >= tokens.size() || !isWord(tokens[i + 1]))
            {
                parseError("missing file name after redirection", currentCommand, currentSimpleCommand);
                return;
            }
            i++;
            char *file = strdup(tokens[i].value.c_str());

            if (token.type == TOKEN_INPUT)
            {
                if (currentCommand._numberOfSimpleCommands > 0)
                {
                    free(file);
                    parseError("input redirection is only allowed in the first command", currentCommand, currentSimpleCommand);
                    return;
                }
                free(currentCommand._inputFile);
                currentCommand._inputFile = file;
            }
            else if (token.type == TOKEN_ERROR)
            {
                free(currentCommand._errFile);
                currentCommand._errFile = file;
            }
            else
            {
                free(currentCommand._outFile);
                currentCommand._outFile = file;
                currentCommand._append = (token.type == TOKEN_REDIRECT) ? 0 : 1;
                currentCommand._out_error = (token.type == TOKEN_REDIRECT_AND_ERROR) ? 1 : 0;
            }
        }
        else if (token.type == TOKEN_PIPE)
        {
            if (currentSimpleCommand->_numberOfArguments == 0)
            {
                parseError("missing command before '|'", currentCommand, currentSimpleCommand);
                return;
            }
            if (currentCommand._outFile != NULL)
            {
                parseError("output redirection is only allowed in the last command", currentCommand, currentSimpleCommand);
                return;
            }
            currentCommand.insertSimpleCommand(currentSimpleCommand);
            currentSimpleCommand = new SimpleCommand();
        }
        else if (token.type == TOKEN_BACKGROUND)
        {
            if (!(i + 1 < tokens.size() && tokens[i + 1].type == TOKEN_EOF))
            {
                parseError("'&' must be at the end of the command", currentCommand, currentSimpleCommand);
                return;
            }
            currentCommand._background = 1;
        }
    }

    if (currentSimpleCommand->_numberOfArguments == 0)
    {
        if (currentCommand._numberOfSimpleCommands > 0)
            parseError("missing command after '|'", currentCommand, currentSimpleCommand);
        else
            parseError("missing command", currentCommand, currentSimpleCommand);
        return;
    }

    currentCommand.insertSimpleCommand(currentSimpleCommand);
    currentCommand.execute();
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
    _arguments[0] = NULL;
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        delete _simpleCommands[i];
    }

    free(_outFile);
    free(_inputFile);
    free(_errFile);

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
        printf("\n");
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default",
           _out_error == 1 ? _outFile : "default",
           _background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    print();

    if (_numberOfSimpleCommands == 0)
    {
        clear();
        return;
    }

    // built-in commands run inside the shell itself (no fork)
    if (_numberOfSimpleCommands == 1)
    {
        char **args = _simpleCommands[0]->_arguments;

        if (strcmp(args[0], "exit") == 0)
        {
            clear();
            printf("\nGood bye!!\n");
            exit(0);
        }

        if (strcmp(args[0], "cd") == 0)
        {
            const char *dir = args[1];
            if (dir == NULL)
                dir = getenv("HOME");

            if (dir == NULL || chdir(dir) == -1)
                fprintf(stderr, "cd: %s: %s\n", dir ? dir : "HOME not set", strerror(errno));

            clear();
            return;
        }
    }

    int inputFd = STDIN_FILENO;
    std::vector<pid_t> childPids;

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        int fd[2];
        bool last = (i == _numberOfSimpleCommands - 1);

        if (!last)
        {
            if (pipe(fd) == -1)
            {
                perror("pipe failed");
                break;
            }
        }

        pid_t pid = fork();

        if (pid == 0)
        {
            // ctrl-c must kill the child, only the shell ignores it
            signal(SIGINT, SIG_DFL);

            if (inputFd != STDIN_FILENO)
            {
                dup2(inputFd, STDIN_FILENO);
                close(inputFd);
            }

            if (!last)
            {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            // input redirection (first command only)
            if (i == 0 && _inputFile != NULL)
            {
                int file = open(_inputFile, O_RDONLY);
                if (file == -1)
                {
                    fprintf(stderr, "myshell: %s: %s\n", _inputFile, strerror(errno));
                    _exit(EXIT_FAILURE);
                }
                dup2(file, STDIN_FILENO);
                close(file);
            }

            // output redirection (last command only)
            if (last && _outFile != NULL)
            {
                int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
                int file = open(_outFile, flags, 0644);
                if (file == -1)
                {
                    fprintf(stderr, "myshell: %s: %s\n", _outFile, strerror(errno));
                    _exit(EXIT_FAILURE);
                }
                dup2(file, STDOUT_FILENO);
                close(file);
            }

            // error redirection (last command only)
            if (last && _errFile != NULL)
            {
                int file = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (file == -1)
                {
                    fprintf(stderr, "myshell: %s: %s\n", _errFile, strerror(errno));
                    _exit(EXIT_FAILURE);
                }
                dup2(file, STDERR_FILENO);
                close(file);
            }

            // >>& : stderr goes where stdout goes
            if (last && _out_error == 1)
            {
                dup2(STDOUT_FILENO, STDERR_FILENO);
            }

            execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);

            fprintf(stderr, "myshell: %s: %s\n", _simpleCommands[i]->_arguments[0], strerror(errno));
            _exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            childPids.push_back(pid);

            if (inputFd != STDIN_FILENO)
                close(inputFd);

            if (!last)
            {
                close(fd[1]);
                inputFd = fd[0];
            }
        }
        else
        {
            perror("fork failed");
            if (!last)
            {
                close(fd[0]);
                close(fd[1]);
            }
            break;
        }
    }

    if (inputFd != STDIN_FILENO && childPids.size() < (size_t)_numberOfSimpleCommands)
        close(inputFd);

    // wait only for this command's children, unless it runs in background
    if (!_background)
    {
        for (size_t k = 0; k < childPids.size(); k++)
        {
            // wait without reaping, so the SIGCHLD handler logs and reaps it
            waitid(P_PID, childPids[k], NULL, WEXITED | WNOWAIT);
        }
    }

    clear();
}

void Command::prompt()
{
    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);

        if (!std::getline(std::cin, input))
        {
            printf("\nGood bye!!\n");
            break;
        }

        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    // the shell ignores ctrl-c
    signal(SIGINT, SIG_IGN);

    // log every finished child
    struct sigaction sa;
    sa.sa_handler = childHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    Command::_currentCommand.prompt();
    return 0;
}
