#include "shell.h"
#include "c3c4.h"
#include "func.h"
#include "lexer.h"
#include "parser.h"
#include "reveal.h"
#include "execute.h"
#include "d1d2.h"
#include <stdio.h>
#include <string.h>



int main(void)
{
    ShellState state = {0};
    char input[CSHELL_MAX_INPUT + 2];

    if (shell_state_init(&state) != 0) {
        perror("cshell: initialization failed");
        return 1;
    }
    for (;;) {
        
        void_reap_background_processes();

        if (print_prompt(&state) != 0) {
            perror("cshell: prompt failed");
            break;
        }
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {
            }
            fputs("cshell: input too long\n", stderr);
            continue;
        }
        input[strcspn(input, "\n")] = '\0';

        {
            TokenList tokens;
            int invalid;

            token_list_init(&tokens);
            invalid = lex_line(input, &tokens);
            if (invalid == 0) {
                invalid = parse_tokens(&tokens);
            }
            if (invalid != 0) {
                fputs("cshell: invalid syntax\n", stderr);
            } else {
                run(&state, &tokens);
            }
            token_list_destroy(&tokens);
        }
    }

    putchar('\n');
    shell_state_destroy(&state);
    return 0;
}
