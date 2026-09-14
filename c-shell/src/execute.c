#include "execute.h"
#include "c3c4.h"
#include "redirection.h"
#include "d1d2.h"
#include <errno.h>
#include <signal.h>
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

char *find_executable(const char *name,int path_only){
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

int simple_external(const TokenList *tokens){
    if(tokens->count==0) return 0;

    for(size_t i=0;i<tokens->count;i++){
        if(tokens->items[i].type!=TOKEN_WORD){
            return 0;
        }
    }
    return 1;
}



int run_external(ShellState *state,const TokenList *tokens){
    size_t count;
    char **argv;
    char *name;
    char *path;
    int path_only=0;
    FILE *input_stream=NULL;
    OutputRedirect output;
    pid_t child;
    pid_t group;
    int status;


    int stopped=0;

    (void)state;
    output_redirect_init(&output);

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

    if(prepare_output(tokens,&output)!=0){
        free(argv);
        if(input_stream!=NULL) fclose(input_stream);
        fputs("cshell: unable to create file for writing\n",stderr);
        output_redirect_destroy(&output);
        return 1;
    }

    name=tokens->items[0].text;
    if(name[0]=='%'){
        path_only=1;
        name++;
    }
    path=find_executable(name,path_only);
    if(path==NULL){
        fprintf(stderr,"cshell: command not found (%s)\n",name);
        free(argv);
        if(input_stream!=NULL) fclose(input_stream);
        output_redirect_destroy(&output);
        return 1;
    }

    argv[0]=name;
    group=is_background_child() ? getpgrp() : 0;
    child=fork();
    if(child==0){

        setpgid(0,group);

        signal(SIGINT,SIG_DFL);
        signal(SIGTSTP,SIG_DFL);
        signal(SIGTTOU,SIG_DFL);


        if(input_stream!=NULL &&
           dup2(fileno(input_stream),STDIN_FILENO)<0){
            perror("cshell: input redirection failed");
            _exit(1);
        }
        if(output.count>0 &&
           dup2(output.write_fd,STDOUT_FILENO)<0){
            perror("cshell: output redirection failed");
            _exit(1);
        }
        output_redirect_close_child(&output);
        execve(path,argv,environ);
        fprintf(stderr,"cshell: command not found (%s)\n",name);
        _exit(127);
    }
    if(child<0){
        perror("cshell: fork failed");
    }else{

        setpgid(child,group==0 ? child : group);

        if(is_background_child()==0){
            giveterminal(child);
        }

        output_redirect_close_parent_write(&output);

        wait_with_relay(child,&output,&status);

        if(is_background_child()==0){

            if(WIFSTOPPED(status)) stopped=1;

            giveterminal(getpgrp());

            if(stopped){
                int job_number=add_stopped_job(child,tokens);
                if(job_number>0) print_stopped(job_number);
            }
        }
    }




    free(argv);
    free(path);

    if (input_stream!=NULL) fclose(input_stream);

    output_redirect_destroy(&output);
    return 1;

}