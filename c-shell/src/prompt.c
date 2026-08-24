#include "shell.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CSHELL_HOSTNAME_MAX 255

int shell_state_init(ShellState *state)
{
    state->home_dir = getcwd(NULL, 0);
    state->previous_dir = NULL;
    return state->home_dir == NULL ? -1 : 0;
}

void shell_state_destroy(ShellState *state)
{
    free(state->home_dir);
    free(state->previous_dir);
    state->home_dir = NULL;
    state->previous_dir = NULL;
}

static const char *display_path(const ShellState *state, const char *cwd)
{
    size_t home_len = strlen(state->home_dir);

    if (strncmp(cwd, state->home_dir, home_len) == 0 &&
        (cwd[home_len] == '\0' || cwd[home_len] == '/')) {
        return cwd + home_len;
    }
    return NULL;
}

int print_prompt(const ShellState *state)
{
    char *cwd;
    char hostname[CSHELL_HOSTNAME_MAX + 1];
    struct passwd *password;
    const char *relative_path;
    int flush_failed;

    cwd = getcwd(NULL, 0);
    if (cwd == NULL || gethostname(hostname, sizeof(hostname)) != 0) {
        free(cwd);
        return -1;
    }
    hostname[sizeof(hostname) - 1] = '\0';

    password = getpwuid(getuid());
    if (password == NULL || password->pw_name == NULL) {
        free(cwd);
        return -1;
    }

    relative_path = display_path(state, cwd);
    if (relative_path != NULL) {
        printf("<%s@%s:~%s> ", password->pw_name, hostname, relative_path);
    } else {
        printf("<%s@%s:%s> ", password->pw_name, hostname, cwd);
    }
    free(cwd);

    flush_failed = fflush(stdout) == EOF;
    return flush_failed ? -1 : 0;
}
