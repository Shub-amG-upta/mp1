#ifndef CSHELL_LEXER_H
#define CSHELL_LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_WORD,
    TOKEN_OP_PIPE,
    TOKEN_OP_AMP,
    TOKEN_OP_SEMI,
    TOKEN_OP_LT,
    TOKEN_OP_GT,
    TOKEN_OP_GTGT
} TokenType;

typedef struct {
    TokenType type;
    char *text;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

void token_list_init(TokenList *tokens);
void token_list_destroy(TokenList *tokens);
int token_list_push(TokenList *tokens, TokenType type, const char *text,
                    size_t text_length);
int lex_line(const char *line, TokenList *tokens);
const char *token_type_name(TokenType type);

#endif
