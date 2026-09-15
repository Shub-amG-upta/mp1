#ifndef CSHELL_EXECUTE_H
#define CSHELL_EXECUTE_H

#include "lexer.h"
#include "shell.h"

#include "execute.h"

int run_external(ShellState *state,const TokenList *tokens);
char *find_executable(const char *name,int path_only);
int simple_external(const TokenList *tokens);
void exec_command(const char *path,char **argv);
#endif