
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

    if(_numberOfSimpleCommands == 0) /*check en fy command*/
    {
        clear();
        return;
    }
    
   for(int i =0; i<_numberOfSimpleCommands; i++) 
   {
    pid_t pid =fork();
    
    if(pid == 0)
    {
        if(_outFile != NULL)
        {
            if(_append)
            {
                int fd=open(_outFile,O_WRONLY | O_CREAT | O_APPEND, 0644);
                if(fd == -1)
                {
                    perror("open failed");
                    _exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else
            {
            int fd=open(_outFile,O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd == -1)
            {
                perror("open failed");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            }
        }

        if(_inputFile != NULL)
        {
            int fd=open(_inputFile,O_RDONLY);
            if(fd == -1)
            {
                perror("open failed");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if(_errFile != NULL)
        {
            int fd=open(_errFile,O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd == -1)
            {
                perror("open failed");
                _exit(EXIT_FAILURE);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        if(_out_error == 1)
        {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        }

     execvp(_simpleCommands[0]->_arguments[0], _simpleCommands[0]->_arguments);
     perror("execvp failed"); 
     /*user typed command not found ,zy ma fel shell byktb command not found*/
     /*perror dy bttb3 elstring ely hktbo + el linux error*/
     _exit(EXIT_FAILURE); /*h2fl elchild process*/
    }
    else if (pid > 0)
    {
        // Parent process
        if (!_background)
        {
            waitpid(pid, nullptr, 0); // Wait for the child process to finish if not in background
        }
    }
    else
    {
        perror("fork failed");
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
