#include "lexer.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} WordBuffer;

static int is_space(char c){
    return c==' ' || c=='\t' || c=='\n' || c=='\r';
}

static int is_special(char c){
    return c=='|' || c=='&' || c==';' || c=='<' || c=='>';
}

static void free_word(WordBuffer *word){
    free(word->data);
    word->data=NULL;
    word->len=0;
    word->cap=0;
}

static int add_char(WordBuffer *word,char c){
    char *temp;
    size_t new_cap;

    if(word->len==word->cap){
        new_cap=word->cap==0 ? 16 : word->cap*2;
        temp=realloc(word->data,new_cap);
        if(temp==NULL) return -1;
        word->data=temp;
        word->cap=new_cap;
    }

    word->data[word->len]=c;
    word->len++;
    return 0;
}

static int save_word(TokenList *tokens,WordBuffer *word,int *has_word){
    if(*has_word==0) return 0;

    if(word->data==NULL){
        if(token_list_push(tokens,TOKEN_WORD,"",0)!=0) return -1;
    }else{
        if(token_list_push(tokens,TOKEN_WORD,word->data,word->len)!=0) return -1;
    }

    word->len=0;
    *has_word=0;
    return 0;
}

void token_list_init(TokenList *tokens){
    tokens->items=NULL;
    tokens->count=0;
    tokens->capacity=0;
}

void token_list_destroy(TokenList *tokens){
    for(size_t i=0;i<tokens->count;i++) free(tokens->items[i].text);
    free(tokens->items);
    tokens->items=NULL;
    tokens->count=0;
    tokens->capacity=0;
}

int token_list_push(TokenList *tokens,TokenType type,const char *text,
                    size_t text_length){
    Token *temp;
    char *copy;
    size_t new_cap;

    if(tokens->count==tokens->capacity){
        new_cap=tokens->capacity==0 ? 8 : tokens->capacity*2;
        temp=realloc(tokens->items,new_cap*sizeof(Token));
        if(temp==NULL) return -1;
        tokens->items=temp;
        tokens->capacity=new_cap;
    }

    copy=malloc(text_length+1);
    if(copy==NULL) return -1;
    memcpy(copy,text,text_length);
    copy[text_length]='\0';

    tokens->items[tokens->count].type=type;
    tokens->items[tokens->count].text=copy;
    tokens->count++;
    return 0;
}

int lex_line(const char *line,TokenList *tokens){
    WordBuffer word={0};
    size_t i=0;
    int has_word=0;

    while(line[i]!='\0'){
        TokenType type=TOKEN_WORD;
        size_t op_len=1;

        if(is_space(line[i])){
            if(save_word(tokens,&word,&has_word)!=0){
                free_word(&word);
                return -1;
            }
            i++;
            continue;
        }

        if(is_special(line[i])){
            if(save_word(tokens,&word,&has_word)!=0){
                free_word(&word);
                return -1;
            }

            if(line[i]=='|') type=TOKEN_OP_PIPE;
            else if(line[i]=='&') type=TOKEN_OP_AMP;
            else if(line[i]==';') type=TOKEN_OP_SEMI;
            else if(line[i]=='<') type=TOKEN_OP_LT;
            else if(line[i]=='>'){
                type=TOKEN_OP_GT;
                if(line[i+1]=='>'){
                    type=TOKEN_OP_GTGT;
                    op_len=2;
                }
            }

            if(token_list_push(tokens,type,line+i,op_len)!=0){
                free_word(&word);
                return -1;
            }
            i+=op_len;
            continue;
        }

        has_word=1;
        if(line[i]=='\\'){
            i++;
            if(line[i]=='\0' || add_char(&word,line[i])!=0){
                free_word(&word);
                return -1;
            }
            i++;
        }else if(line[i]=='\''){
            i++;
            while(line[i]!='\''){
                if(line[i]=='\0' || add_char(&word,line[i])!=0){
                    free_word(&word);
                    return -1;
                }
                i++;
            }
            i++;
        }else if(line[i]=='"'){
            i++;
            while(line[i]!='"'){
                if(line[i]=='\0'){
                    free_word(&word);
                    return -1;
                }
                if(line[i]=='\\'){
                    i++;
                    if(line[i]=='\0'){
                        free_word(&word);
                        return -1;
                    }
                    if(line[i]!='"' && line[i]!='\\' &&
                       add_char(&word,'\\')!=0){
                        free_word(&word);
                        return -1;
                    }
                }
                if(add_char(&word,line[i])!=0){
                    free_word(&word);
                    return -1;
                }
                i++;
            }
            i++;
        }else{
            if(add_char(&word,line[i])!=0){
                free_word(&word);
                return -1;
            }
            i++;
        }
    }

    if(save_word(tokens,&word,&has_word)!=0){
        free_word(&word);
        return -1;
    }
    free_word(&word);
    return 0;
}

const char *token_type_name(TokenType type){
    if(type==TOKEN_WORD) return "WORD";
    if(type==TOKEN_OP_PIPE) return "OP_PIPE";
    if(type==TOKEN_OP_AMP) return "OP_AMP";
    if(type==TOKEN_OP_SEMI) return "OP_SEMI";
    if(type==TOKEN_OP_LT) return "OP_LT";
    if(type==TOKEN_OP_GT) return "OP_GT";
    if(type==TOKEN_OP_GTGT) return "OP_GTGT";
    return "UNKNOWN";
}
