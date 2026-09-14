#include "redirection.h"
#include <stdio.h>

int collect_command(const TokenList *tokens,char **argv,
                    size_t *argument_count,FILE **input_stream){
    size_t i=0;
    size_t count=0;
    const char *input_name=NULL;
    FILE *stream=NULL;

    while(i<tokens->count){
        TokenType type=tokens->items[i].type;

        if(type==TOKEN_WORD){
            argv[count++]=tokens->items[i].text;
            i++;
            continue;
        }

        if(type==TOKEN_OP_LT){
            if(i+1>=tokens->count || tokens->items[i+1].type!=TOKEN_WORD){
                return -2;
            }
            input_name=tokens->items[i+1].text; 
            i+=2;
            continue;
        }

        if(type==TOKEN_OP_GT || type==TOKEN_OP_GTGT){
            if(i+1>=tokens->count || tokens->items[i+1].type!=TOKEN_WORD){
                return -2;
            }
            i+=2;
            continue;
        }
        i++;
    }

    if(input_name!=NULL){
        stream=fopen(input_name,"r");
        if(stream==NULL) return -1;
    }

    argv[count]=NULL;
    *argument_count=count;
    *input_stream=stream;
    return 0;
}