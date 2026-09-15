#include "redirection.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int copy_fd(int from,int to){
    char buffer[4096];
    ssize_t bytes;

    while((bytes=read(from,buffer,sizeof(buffer)))!=0){
        if(bytes<0){
            if(errno==EINTR) continue;
            return -1;
        }
        for(ssize_t done=0;done<bytes;){
            ssize_t put=write(to,buffer+done,(size_t)(bytes-done));
            if(put<0){
                if(errno==EINTR) continue;
                return -1;
            }
            done+=put;
        }
    }
    return 0;
}

/* Returns a new fd, positioned at the start, holding first's content
   followed by second's. Both inputs are closed. */
int join_inputs(int first,int second){
    char name[]="/tmp/cshell-inputXXXXXX";
    int joined=mkstemp(name);

    if(joined>=0){
        unlink(name);
        if(copy_fd(first,joined)!=0 || copy_fd(second,joined)!=0 ||
           lseek(joined,0,SEEK_SET)<0){
            close(joined);
            joined=-1;
        }
    }
    close(first);
    close(second);
    return joined;
}

int collect_command(const TokenList *tokens,char **argv,
                    size_t *argument_count,FILE **input_stream){
    size_t i=0;
    size_t count=0;
    int input=-1;
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
                if(input>=0) close(input);
                return -2;
            }
            {
                /* C2: open every file in order, stdin is all of them */
                int fd=open(tokens->items[i+1].text,O_RDONLY);

                if(fd<0){
                    if(input>=0) close(input);
                    return -1;
                }
                input=(input<0) ? fd : join_inputs(input,fd);
                if(input<0) return -1;
            }
            i+=2;
            continue;
        }

        if(type==TOKEN_OP_GT || type==TOKEN_OP_GTGT){
            if(i+1>=tokens->count || tokens->items[i+1].type!=TOKEN_WORD){
                if(input>=0) close(input);
                return -2;
            }
            i+=2;
            continue;
        }
        i++;
    }

    if(input>=0){
        stream=fdopen(input,"r");
        if(stream==NULL){
            close(input);
            return -1;
        }
    }

    argv[count]=NULL;
    *argument_count=count;
    *input_stream=stream;
    return 0;
}