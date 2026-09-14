#ifndef D1D2_H
#define D1D2_H

#include "shell.h"
#include "lexer.h"
#include <sys/types.h>

int run(ShellState *state,const TokenList *tokens);
void buff();

int getjobcount(void);
int getjobinfo(int index,pid_t *pid,char *name,int *live);


#endif 