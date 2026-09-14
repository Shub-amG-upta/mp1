#ifndef ACTIVITIES_H
#define ACTIVITIES_H

#include "lexer.h"
#include "shell.h"
#include "parser.h" 
#include "execute.h"
#include "d1d2.h"


int runactivity(ShellState *state,const TokenList *tokens);

#endif