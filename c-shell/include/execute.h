#ifndef CSHELL_EXECUTE_H
#define CSHELL_EXECUTE_H

#include "lexer.h"
#include "shell.h"

int run_external(ShellState *state,const TokenList *tokens);
char *find_executable(const char *name,int path_only);

#endif
