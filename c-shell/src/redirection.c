#include "redirection.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int copy_file(int input_fd,int output_fd){
    char buffer[4096];
    ssize_t bytes_read;

    for(;;){
        bytes_read=read(input_fd,buffer,sizeof(buffer));
        if(bytes_read==0) return 0;
        if(bytes_read<0){
            if(errno==EINTR) continue;
            return -1;
        }
        {
            ssize_t written=0;
            while(written<bytes_read){
                ssize_t result=write(output_fd,buffer+written,
                                     (size_t)(bytes_read-written));
                if(result<0){
                    if(errno==EINTR) continue;
                    return -1;
                }
                written+=result;
            }
        }
    }
}

int collect_command(const TokenList *tokens,char **argv,
                    size_t *argument_count,FILE **input_stream){
    size_t i=0;
    size_t count=0;
    FILE *stream=NULL;

    while(i<tokens->count){
        if(tokens->items[i].type==TOKEN_WORD){
            argv[count++]=tokens->items[i].text;
            i++;
            continue;
        }
        if(tokens->items[i].type==TOKEN_OP_LT){
            int input_fd;
            if(i+1>=tokens->count ||
               tokens->items[i+1].type!=TOKEN_WORD){
                if(stream!=NULL) fclose(stream);
                return -2;
            }
            if(stream==NULL){
                stream=tmpfile();
                if(stream==NULL){
                    perror("cshell: unable to prepare input");
                    return -1;
                }
            }
            input_fd=open(tokens->items[i+1].text,O_RDONLY);
            if(input_fd<0){
                fclose(stream);
                return -1;
            }
            if(copy_file(input_fd,fileno(stream))!=0){
                close(input_fd);
                fclose(stream);
                return -1;
            }
            close(input_fd);
            i+=2;
            continue;
        }
        break;
    }

    if(stream!=NULL && lseek(fileno(stream),0,SEEK_SET)==(off_t)-1){
        fclose(stream);
        return -1;
    }
    argv[count]=NULL;
    *argument_count=count;
    *input_stream=stream;
    return 0;
}
