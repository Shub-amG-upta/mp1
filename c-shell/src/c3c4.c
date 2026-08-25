#include "c3c4.h"
#include "execute.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int write_all(int fd,const char *buffer,size_t length){
    size_t written=0;

    while(written<length){
        ssize_t result=write(fd,buffer+written,length-written);
        if(result<0){
            if(errno==EINTR) continue;
            return -1;
        }
        written+=(size_t)result;
    }
    return 0;
}

void output_redirect_init(OutputRedirect *output){
    output->files=NULL;
    output->count=0;
    output->read_fd=-1;
    output->write_fd=-1;
}

static void close_output_files(OutputRedirect *output){
    for(size_t i=0;i<output->count;i++) close(output->files[i]);
    free(output->files);
    output->files=NULL;
    output->count=0;
}

int prepare_output(const TokenList *tokens,OutputRedirect *output){
    size_t i=0;

    while(i<tokens->count){
        int flags;
        int fd;
        int *temp;

        if(tokens->items[i].type!=TOKEN_OP_GT &&
           tokens->items[i].type!=TOKEN_OP_GTGT){
            if(tokens->items[i].type==TOKEN_OP_PIPE ||
               tokens->items[i].type==TOKEN_OP_SEMI ||
               tokens->items[i].type==TOKEN_OP_AMP) break;
            i++;
            continue;
        }
        if(i+1>=tokens->count ||
           tokens->items[i+1].type!=TOKEN_WORD) return -2;
        flags=O_WRONLY|O_CREAT;
        if(tokens->items[i].type==TOKEN_OP_GT) flags|=O_TRUNC;
        else flags|=O_APPEND;
        fd=open(tokens->items[i+1].text,flags,0644);
        if(fd<0){
            close_output_files(output);
            return -1;
        }
        temp=realloc(output->files,(output->count+1)*sizeof(int));
        if(temp==NULL){
            close(fd);
            close_output_files(output);
            return -1;
        }
        output->files=temp;
        output->files[output->count]=fd;
        output->count++;
        i+=2;
    }
    if(output->count>0){
        int pipe_fds[2];
        if(pipe(pipe_fds)!=0){
            close_output_files(output);
            return -1;
        }
        output->read_fd=pipe_fds[0];
        output->write_fd=pipe_fds[1];
    }
    return 0;
}

void output_redirect_close_child(OutputRedirect *output){
    if(output->read_fd>=0) close(output->read_fd);
    if(output->write_fd>=0) close(output->write_fd);
    for(size_t i=0;i<output->count;i++) close(output->files[i]);
}

void output_redirect_close_parent_write(OutputRedirect *output){
    if(output->write_fd>=0){
        close(output->write_fd);
        output->write_fd=-1;
    }
}

int output_redirect_relay(OutputRedirect *output){
    char buffer[4096];
    ssize_t bytes_read;
    int result=0;

    while((bytes_read=read(output->read_fd,buffer,sizeof(buffer)))!=0){
        if(bytes_read<0){
            if(errno==EINTR) continue;
            result=-1;
            break;
        }
        for(size_t i=0;i<output->count;i++){
            if(write_all(output->files[i],buffer,(size_t)bytes_read)!=0){
                result=-1;
            }
        }
    }
    close(output->read_fd);
    output->read_fd=-1;
    return result;
}

void output_redirect_destroy(OutputRedirect *output){
    if(output->read_fd>=0) close(output->read_fd);
    if(output->write_fd>=0) close(output->write_fd);
    close_output_files(output);
}

typedef struct {
    size_t start;
    size_t end;
    char **args;
    size_t count;
    int input;
    int output;
} Stage;

static void close_pipes(int (*pipes)[2],size_t count){
    for(size_t i=0;i<count;i++){
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static void free_stages(Stage *stages,size_t count){
    if(stages==NULL) return;
    for(size_t i=0;i<count;i++){
        free(stages[i].args);
        if(stages[i].input>=0) close(stages[i].input);
        if(stages[i].output>=0) close(stages[i].output);
    }
    free(stages);
}

static int stage_args(const TokenList *tokens,Stage *stage){
    size_t i=stage->start;
    stage->args=calloc(stage->end-stage->start+1,sizeof(char *));
    if(stage->args==NULL) return -3;
    while(i<stage->end){
        TokenType type=tokens->items[i].type;
        int flags;
        int fd;
        if(type==TOKEN_WORD){
            stage->args[stage->count++]=tokens->items[i].text;
            i++;
            continue;
        }
        if(type!=TOKEN_OP_LT && type!=TOKEN_OP_GT &&
           type!=TOKEN_OP_GTGT){
            i++;
            continue;
        }
        if(i+1>=stage->end ||
           tokens->items[i+1].type!=TOKEN_WORD) return -2;
        if(type==TOKEN_OP_LT){
            fd=open(tokens->items[i+1].text,O_RDONLY);
            if(fd<0) return -1;
            if(stage->input>=0) close(stage->input);
            stage->input=fd;
        }else{
            flags=O_WRONLY|O_CREAT;
            if(type==TOKEN_OP_GT) flags|=O_TRUNC;
            else flags|=O_APPEND;
            fd=open(tokens->items[i+1].text,flags,0644);
            if(fd<0) return -4;
            if(stage->output>=0) close(stage->output);
            stage->output=fd;
        }
        i+=2;
    }
    stage->args[stage->count]=NULL;
    return 0;
}

static int make_stages(const TokenList *tokens,Stage **result,
                       size_t *stage_count){
    size_t end=tokens->count;
    size_t count=1;
    size_t start=0;
    Stage *stages;
    for(size_t i=0;i<tokens->count;i++){
        if(tokens->items[i].type==TOKEN_OP_SEMI ||
           tokens->items[i].type==TOKEN_OP_AMP){
            end=i;
            break;
        }
    }
    for(size_t i=0;i<end;i++){
        if(tokens->items[i].type==TOKEN_OP_PIPE) count++;
    }
    if(count==1) return 0;
    stages=calloc(count,sizeof(Stage));
    if(stages==NULL) return -3;
    for(size_t i=0;i<count;i++){
        size_t stop=end;
        for(size_t j=start;j<end;j++){
            if(tokens->items[j].type==TOKEN_OP_PIPE){
                stop=j;
                break;
            }
        }
        stages[i].start=start;
        stages[i].end=stop;
        stages[i].input=-1;
        stages[i].output=-1;
        {
            int result=stage_args(tokens,&stages[i]);
            if(result!=0){
                free_stages(stages,count);
                return result;
            }
        }
        start=stop+1;
    }
    *result=stages;
    *stage_count=count;
    return 1;
}

int run_pipeline(ShellState *state,const TokenList *tokens){
    Stage *stages=NULL;
    size_t count=0;
    int (*pipes)[2]=NULL;
    pid_t *children=NULL;
    size_t made=0;
    int result;
    int status;
    (void)state;
    result=make_stages(tokens,&stages,&count);
    if(result==0) return 0;
    if(result<0){
        if(result==-1) fputs("cshell: no such file or directory\n",stderr);
        else if(result==-4){
            fputs("cshell: unable to create file for writing\n",stderr);
        }else fputs("cshell: invalid syntax\n",stderr);
        return 1;
    }
    pipes=calloc(count-1,sizeof(int[2]));
    children=calloc(count,sizeof(pid_t));
    if(pipes==NULL || children==NULL){
        free(pipes);
        free(children);
        free_stages(stages,count);
        perror("cshell: memory allocation failed");
        return 1;
    }
    for(size_t i=0;i+1<count;i++){
        if(pipe(pipes[i])!=0){
            close_pipes(pipes,i);
            free(pipes);
            free(children);
            free_stages(stages,count);
            perror("cshell: pipe failed");
            return 1;
        }
    }
    for(size_t i=0;i<count;i++){
        pid_t child=fork();
        if(child==0){
            char *name=stages[i].args[0];
            char *path;
            int path_only=0;
            if(i>0) dup2(pipes[i-1][0],STDIN_FILENO);
            if(i+1<count) dup2(pipes[i][1],STDOUT_FILENO);
            if(stages[i].input>=0) dup2(stages[i].input,STDIN_FILENO);
            if(stages[i].output>=0) dup2(stages[i].output,STDOUT_FILENO);
            close_pipes(pipes,count-1);
            if(stages[i].input>=0) close(stages[i].input);
            if(stages[i].output>=0) close(stages[i].output);
            if(name[0]=='%'){
                path_only=1;
                name++;
                stages[i].args[0]=name;
            }
            path=find_executable(name,path_only);
            if(path==NULL){
                fprintf(stderr,"cshell: command not found (%s)\n",name);
                _exit(127);
            }
            execve(path,stages[i].args,environ);
            fprintf(stderr,"cshell: command not found (%s)\n",name);
            free(path);
            _exit(127);
        }
        if(child<0) break;
        children[made++]=child;
    }
    close_pipes(pipes,count-1);
    for(size_t i=0;i<made;i++){
        while(waitpid(children[i],&status,0)<0 && errno==EINTR){
        }
    }
    free(pipes);
    free(children);
    free_stages(stages,count);
    return 1;
}
