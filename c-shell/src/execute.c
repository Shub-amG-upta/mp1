#include "execute.h"
#include "redirection.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int executable_file(const char *path){
    return access(path,X_OK)==0;
}

static char *make_path(const char *directory,const char *name){
    char *path;
    size_t length=strlen(directory)+strlen(name)+2;

    path=malloc(length);
    if(path==NULL) return NULL;
    snprintf(path,length,"%s/%s",directory,name);
    return path;
}

static char *find_command(const char *name,int path_only){
    char *path_copy;
    char *part;
    char *saveptr=NULL;
    char *candidate;
    const char *path_value;

    if(strchr(name,'/')!=NULL){
        if(executable_file(name)) return strdup(name);
        return NULL;
    }

    if(path_only==0){
        candidate=make_path(".",name);
        if(candidate!=NULL && executable_file(candidate)) return candidate;
        free(candidate);
    }

    path_value=getenv("PATH");
    if(path_value==NULL) return NULL;
    path_copy=strdup(path_value);
    if(path_copy==NULL) return NULL;

    part=strtok_r(path_copy,":",&saveptr);
    while(part!=NULL){
        candidate=make_path(part[0]=='\0' ? "." : part,name);
        if(candidate!=NULL && executable_file(candidate)){
            free(path_copy);
            return candidate;
        }
        free(candidate);
        part=strtok_r(NULL,":",&saveptr);
    }

    free(path_copy);
    return NULL;
}

int run_external(ShellState *state,const TokenList *tokens){
    size_t count;
    char **argv;
    char *name;
    char *path;
    int path_only=0;
    FILE *input_stream=NULL;
    pid_t child;
    int status;

    (void)state;
    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD) return 0;
    argv=calloc(tokens->count+1,sizeof(char *));
    if(argv==NULL){
        perror("cshell: memory allocation failed");
        return 1;
    }
    if(collect_command(tokens,argv,&count,&input_stream)!=0){
        free(argv);
        fputs("cshell: no such file or directory\n",stderr);
        return 1;
    }
    if(count==0){
        free(argv);
        if(input_stream!=NULL) fclose(input_stream);
        return 0;
    }

    name=tokens->items[0].text;
    if(name[0]=='%'){
        path_only=1;
        name++;
    }
    path=find_command(name,path_only);
    if(path==NULL){
        fprintf(stderr,"cshell: command not found (%s)\n",name);
        free(argv);
        if(input_stream!=NULL) fclose(input_stream);
        return 1;
    }

    argv[0]=name;
    child=fork();
    if(child==0){
        if(input_stream!=NULL &&
           dup2(fileno(input_stream),STDIN_FILENO)<0){
            perror("cshell: input redirection failed");
            _exit(1);
        }
        execve(path,argv,environ);
        fprintf(stderr,"cshell: command not found (%s)\n",name);
        _exit(127);
    }
    if(child<0){
        perror("cshell: fork failed");
    }else{
        while(waitpid(child,&status,0)<0 && errno==EINTR){
        }
    }

    free(argv);
    free(path);
    if(input_stream!=NULL) fclose(input_stream);
    return 1;
}
