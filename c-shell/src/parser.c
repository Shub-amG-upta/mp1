#include "parser.h"

static int is_redirection(TokenType type){
    return type==TOKEN_OP_LT|| type==TOKEN_OP_GT|| type==TOKEN_OP_GTGT;
}

static int is_separator(TokenType type){
    return type==TOKEN_OP_PIPE 
    || type==TOKEN_OP_SEMI || type==TOKEN_OP_AMP;
}

int parse_tokens(const TokenList *tokens){
    size_t i=0;
    int want_command=1;

    if(tokens->count==0) return 0;

    while(i<tokens->count){
        TokenType type=tokens->items[i].type;

        if(want_command!=0){
            if(type!=TOKEN_WORD) return -1;
            want_command=0;
            i++;
            continue;
        }

        if(type==TOKEN_WORD){
            i++;
            continue;
        }

        if(is_redirection(type)){
            i++;
            if(i==tokens->count || tokens->items[i].type!=TOKEN_WORD){
                return -1;
            }
            i++;
            continue;
        }

        if(is_separator(type)){
            i++;
            if(i==tokens->count){
                if(type==TOKEN_OP_AMP) return 0;
                return -1;
            }
            want_command=1;
            continue;
        }

        return -1;
    }

    if(want_command==0) return 0;
    return -1;
}
