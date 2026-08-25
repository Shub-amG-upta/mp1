#ifndef CSHELL_FUNC_H
#define CSHELL_FUNC_H

#include "lexer.h"
#include "shell.h"

int run_hop(ShellState *state, const TokenList *tokens);
int run_locate(ShellState *state, const TokenList *tokens);
int run_peek(ShellState *state, const TokenList *tokens);

#endif
