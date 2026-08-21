#include "lexer.h"

#include <stdlib.h>
#include <string.h>

void token_list_init(TokenList *tokens)
{
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}

void token_list_destroy(TokenList *tokens)
{
    size_t i;

    for (i = 0; i < tokens->count; i++) {
        free(tokens->items[i].text);
    }
    free(tokens->items);
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
}

int token_list_push(TokenList *tokens, TokenType type, const char *text,
                    size_t text_length)
{
    Token *new_items;
    char *copy;
    size_t new_capacity;

    if (tokens->count == tokens->capacity) {
        new_capacity = tokens->capacity == 0 ? 8 : tokens->capacity * 2;
        new_items = realloc(tokens->items, new_capacity * sizeof(Token));
        if (new_items == NULL) {
            return -1;
        }
        tokens->items = new_items;
        tokens->capacity = new_capacity;
    }

    copy = malloc(text_length + 1);
    if (copy == NULL) {
        return -1;
    }
    memcpy(copy, text, text_length);
    copy[text_length] = '\0';

    tokens->items[tokens->count].type = type;
    tokens->items[tokens->count].text = copy;
    tokens->count++;
    return 0;
}

const char *token_type_name(TokenType type)
{
    switch (type) {
    case TOKEN_WORD:
        return "WORD";
    case TOKEN_OP_PIPE:
        return "OP_PIPE";
    case TOKEN_OP_AMP:
        return "OP_AMP";
    case TOKEN_OP_SEMI:
        return "OP_SEMI";
    case TOKEN_OP_LT:
        return "OP_LT";
    case TOKEN_OP_GT:
        return "OP_GT";
    case TOKEN_OP_GTGT:
        return "OP_GTGT";
    }

    return "UNKNOWN";
}
