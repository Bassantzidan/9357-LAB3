
/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"


void parse(std::vector<Token> &tokens)
{
    Command currentCommand;
    SimpleCommand *currentSimpleCommand = new SimpleCommand();
    for(int i = 0; i<tokens.size(); i++)
    {
        Token token = tokens[i];
        if(token.type == TOKEN_COMMAND || token.type == TOKEN_ARGUMENT) 
        {
            currentSimpleCommand->insertArgument(strdup(token.value.c_str()));
        }
        else if(token.type == TOKEN_REDIRECT)
        {
            i++;
            if(i < tokens.size())
            {
                currentCommand._outFile = strdup(tokens[i].value.c_str());
            }
        }
        else if(token.type == TOKEN_APPEND)
        {
            i++;
            if(i < tokens.size())
            {
                currentCommand._outFile = strdup(tokens[i].value.c_str());
                currentCommand._append = 1;
            }
        }
        else if(token.type == TOKEN_INPUT)
        {
            i++;
            if(i < tokens.size())
            {
                currentCommand._inputFile = strdup(tokens[i].value.c_str());
            }
        }
        else if(token.type == TOKEN_ERROR)
        {
            i++;
            if(i < tokens.size())
            {
                currentCommand._errFile = strdup(tokens[i].value.c_str());
                currentCommand._out_error = 0;
            }
        }
        else if(token.type == TOKEN_REDIRECT_AND_ERROR)
        {
            i++;
            if(i < tokens.size())
            {
                currentCommand._outFile= strdup(tokens[i].value.c_str());
                currentCommand._out_error = 1;
                currentCommand._append = 1; 
            }
        }
        else if(token.type == TOKEN_PIPE)
        {
            currentCommand.insertSimpleCommand(currentSimpleCommand);
            currentSimpleCommand = new SimpleCommand();
        }
        else if(token.type == TOKEN_BACKGROUND)
        {
            currentCommand._background = 1;
        }
    }  
     currentCommand.insertSimpleCommand(currentSimpleCommand);
    currentCommand.execute();
    /*
      Implement this function 
      you must parse for all possible tokens scenario
      right now command.execute only print the format , you will inject it in your logic.

    */
}

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
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
        /*or we could initialize _numberOfSimpleCommands to a value we want to save space for  */

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0; //initialize _append to 0 to indicate that we are not appending by default
    _out_error = 0; //initialize _out_error to 0 to indicate that we are not redirecting error by default
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
        free(_simpleCommands[i]);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0; //reset _append to 0 when clearing the command 
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
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
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

    int inputFd = STDIN_FILENO;
    std::vector<pid_t> childPids;

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        int fd[2];

        // Create a pipe if there is another command
        if (i < _numberOfSimpleCommands - 1)
        {
            if (pipe(fd) == -1)
            {
                perror("pipe failed");

                if (inputFd != STDIN_FILENO)
                    close(inputFd);

                for (pid_t child : childPids)
                    waitpid(child, NULL, 0);

                clear();
                return;
            }
        }

        pid_t pid = fork();

        if (pid == 0)
        {
            // Read from the previous pipe
            if (inputFd != STDIN_FILENO)
            {
                dup2(inputFd, STDIN_FILENO);
                close(inputFd);
            }

            // Write to the next pipe
            if (i < _numberOfSimpleCommands - 1)
            {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            // Input redirection <
            // Only for the first command
            if (i == 0 && _inputFile != NULL)
            {
                int file = open(_inputFile, O_RDONLY);

                if (file == -1)
                {
                    perror("input open failed");
                    _exit(EXIT_FAILURE);
                }

                dup2(file, STDIN_FILENO);
                close(file);
            }

            // Output redirection > and >>
            // Only for the last command
            if (i == _numberOfSimpleCommands - 1 && _outFile != NULL)
            {
                int flags = O_WRONLY | O_CREAT;

                if (_append == 1)
                    flags = flags | O_APPEND;
                else
                    flags = flags | O_TRUNC;

                int file = open(_outFile, flags, 0644);

                if (file == -1)
                {
                    perror("output open failed");
                    _exit(EXIT_FAILURE);
                }

                dup2(file, STDOUT_FILENO);
                close(file);
            }

            // Error redirection 2>
            // Only for the last command
            if (i == _numberOfSimpleCommands - 1 && _errFile != NULL)
            {
                int file = open(_errFile,
                                O_WRONLY | O_CREAT | O_TRUNC,
                                0644);

                if (file == -1)
                {
                    perror("error open failed");
                    _exit(EXIT_FAILURE);
                }

                dup2(file, STDERR_FILENO);
                close(file);
            }

            // Redirect output and error together >>&
            if (i == _numberOfSimpleCommands - 1 && _out_error == 1)
            {
                dup2(STDOUT_FILENO, STDERR_FILENO);
            }

            // Execute the current command
            execvp(_simpleCommands[i]->_arguments[0],
                   _simpleCommands[i]->_arguments);

            perror("execvp failed!");
            _exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            childPids.push_back(pid);

            // Close the previous pipe input
            if (inputFd != STDIN_FILENO)
            {
                close(inputFd);
            }

            // Keep the read end for the next command
            if (i < _numberOfSimpleCommands - 1)
            {
                close(fd[1]);
                inputFd = fd[0];
            }
            else
            {
                inputFd = STDIN_FILENO;
            }
        }
        else
        {
            perror("fork failed");

            if (inputFd != STDIN_FILENO)
                close(inputFd);

            if (i < _numberOfSimpleCommands - 1)
            {
                close(fd[0]);
                close(fd[1]);
            }

            for (pid_t child : childPids)
                waitpid(child, NULL, 0);

            clear();
            return;
        }
    }

    // Wait only for this command's children
    if (!_background)
    {
        for (pid_t child : childPids)
        {
            waitpid(child, NULL, 0);
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
        std::getline(std::cin, input); 
        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    Command::_currentCommand.prompt();
    return 0;
}
