#ifndef CSHELL_C3C4_H
#define CSHELL_C3C4_H

#include "lexer.h"
#include "shell.h"

#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>

typedef struct {
    int *files;
    size_t count;
    int read_fd;
    int write_fd;
} OutputRedirect;

void output_redirect_init(OutputRedirect *output);
int prepare_output(const TokenList *tokens,OutputRedirect *output);
void output_redirect_close_child(OutputRedirect *output);
void output_redirect_close_parent_write(OutputRedirect *output);
int output_redirect_relay(OutputRedirect *output);
int wait_with_relay(pid_t child,OutputRedirect *output,int *status);
void output_redirect_destroy(OutputRedirect *output);

int run_pipeline(ShellState *state,const TokenList *tokens);

#endif