#ifndef CSHELL_REDIRECTION_H
#define CSHELL_REDIRECTION_H

#include "lexer.h"

#include <stdio.h>

int join_inputs(int first,int second);
int collect_command(const TokenList *tokens,char **argv,
                    size_t *argument_count,FILE **input_stream);

#endif