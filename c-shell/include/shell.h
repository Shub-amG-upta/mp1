#ifndef CSHELL_SHELL_H
#define CSHELL_SHELL_H

#define CSHELL_MAX_INPUT 1024

typedef struct {
    char *home_dir;
    char *previous_dir;
} ShellState;

int shell_state_init(ShellState *state);
void shell_state_destroy(ShellState *state);
int print_prompt(const ShellState *state);

#endif
