#include "shell.h"
#include "d1d2.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    ShellState state = {0};
    char input[CSHELL_MAX_INPUT + 2];
    int eof_after_stopped=0;

    if (shell_state_init(&state) != 0) {
        fprintf(stderr, "cshell: failed to initialize shell\n");
        return 1;
    }

    buff();

    for (;;) {
        if (print_prompt(&state) != 0) {
            shell_state_destroy(&state);
            return 1;
        }

        errno=0;

        if (fgets(input, sizeof(input), stdin) == NULL) {

            if(take_interrupt() || errno==EINTR){
                clearerr(stdin);
                putchar('\n');
                continue;
            }

            if(has_stopped_jobs() && eof_after_stopped==0){

                fputs("cshell: there are stopped jobs\n",stderr);

                eof_after_stopped=1;

                clearerr(stdin);

                continue;
            }

            break;
        }

        eof_after_stopped=0;

        if (input[0] == '\0') {
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        if (input[0] == '\0') {
            continue;
        }

        run(input, &state);
    }

    putchar('\n');
    shutdown_jobs();
    shell_state_destroy(&state);
    return 0;
}