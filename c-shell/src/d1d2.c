#include "d1d2.h"
#include "c3c4.h"
#include "execute.h"
#include "func.h"
#include "reveal.h"
#include "activities.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include<string.h>

extern char **environ;  
#define MAX_JOBS 128


static pid_t job_pid[MAX_JOBS];
static char job_name[MAX_JOBS][32];
static volatile sig_atomic_t job_live[MAX_JOBS];
static int job_count=0;
static sigset_t child_mask;



static void report(const char *name,pid_t pid,int normal){
    char line[96];
    char digits[16];
    unsigned long value=(unsigned long)pid;
    size_t n=0;
    size_t d=0;
    const char *tail=normal ? " exited normally\n" : " exited abnormally\n";

    for(size_t i=0;name[i]!='\0' && i<32;i++) line[n++]=name[i];
    for(const char *p=" with pid ";*p!='\0';p++) line[n++]=*p;
    if(value==0) digits[d++]='0';
    while(value>0){ digits[d++]=(char)('0'+value%10); value/=10; }
    while(d>0) line[n++]=digits[--d];
    for(const char *p=tail;*p!='\0';p++) line[n++]=*p;
    if(write(STDOUT_FILENO,line,n)<0) return;
}

static void handle_sigchld(int signal_number){
    int saved=errno;
    int status;
    pid_t pid;

    (void)signal_number;
    while((pid=waitpid(-1,&status,WNOHANG))>0){
        for(int i=0;i<job_count;i++){
            if(job_live[i] && job_pid[i]==pid){
                if(WIFEXITED(status) || WIFSIGNALED(status)){
                    job_live[i]=0;
                    report(job_name[i],pid,WIFEXITED(status));
                }
                break;
            }
        }
    }
    errno=saved;
}

void buff(void){
    struct sigaction action;

    sigemptyset(&child_mask);
    sigaddset(&child_mask,SIGCHLD);
    action.sa_handler=handle_sigchld;
    sigemptyset(&action.sa_mask);
    action.sa_flags=SA_RESTART|SA_NOCLDSTOP;
    if(sigaction(SIGCHLD,&action,NULL)!=0) perror("cshell: sigaction failed");
}


static int not_found(const TokenList *tokens){
    const char *name;
    char *path;
    int path_only=0;

    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD) return 0;
    name=tokens->items[0].text;
    if(name[0]=='%'){ path_only=1; name++; }
    path=find_executable(name,path_only);
    if(path==NULL) return 1;
    free(path);
    return 0;
}

int runcomm(ShellState *state,const TokenList *tokens){
    if(run_pipeline(state,tokens)==0 && 
    run_hop(state,tokens)==0
    && run_locate(state,tokens)==0 
    && run_peek(state,tokens)==0 
    && run_reveal(state,tokens)==0
    && runactivity(state,tokens)==0){
        int missing=not_found(tokens);
        run_external(state,tokens);
        return missing ? -1 : 1;
    }

    return 1;
}

// static int externalbuff(const TokenList*tokens){
//     static const char* builtins[]={"hop","reveal","peek","locate","activities"};
//     if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD) return 0;
//     for(size_t i=0;i<tokens->count;i++){
//         if(tokens->items[i].type!=TOKEN_WORD) return 0;
//     for(size_t i=0;i<5;i++){
//         if(strcmp(tokens->items[0].text,builtins[i])==0) return 0;
//     }
    
//     }
//     return 1;
// }


int runbuff(ShellState *state,const TokenList *tokens){
    sigset_t previous;
    const char *name;
    pid_t child;
    int i;

    if(job_count==MAX_JOBS){
        fputs("cshell: too many background jobs\n",stderr);
        return 1;
    }

    sigprocmask(SIG_BLOCK,&child_mask,&previous);
    child=fork();
    if(child<0){
        sigprocmask(SIG_SETMASK,&previous,NULL);
        perror("cshell: fork failed");
        return 1;
    }

    if(child==0){
        setpgid(0,0);
        signal(SIGCHLD,SIG_DFL);
        sigprocmask(SIG_SETMASK,&previous,NULL);
        if(simple_external(tokens)){
            char *argv[64];
            const char *cmd=tokens->items[0].text;
            char *path;
            size_t n=0;
            int path_only=0;
 
            while(n<tokens->count && n<63){ argv[n]=tokens->items[n].text; n++; }
            argv[n]=NULL;
            if(cmd[0]=='%'){ path_only=1; cmd++; argv[0]=(char *)cmd; }
            path=find_executable(cmd,path_only);
            if(path!=NULL) execve(path,argv,environ);
            fprintf(stderr,"cshell: command not found (%s)\n",cmd);
            _exit(127);
        }
        runcomm(state,tokens);
        _exit(0);
    }

    setpgid(child,child);

    name=tokens->items[0].text;
    if(name[0]=='%') name++;
    for(i=0;name[i]!='\0' && i<31;i++) job_name[job_count][i]=name[i];
    job_name[job_count][i]='\0';
    job_pid[job_count]=child;
    job_live[job_count]=1;
    job_count++;

    printf("[%d] %d\n",job_count,(int)child);
    fflush(stdout);
    sigprocmask(SIG_SETMASK,&previous,NULL);
    return 1;
}

int run(ShellState *state,const TokenList *tokens){
    size_t start=0;

    for(size_t i=0;i<=tokens->count;i++){

        if(i==tokens->count || tokens->items[i].type==TOKEN_OP_SEMI || tokens->items[i].type==TOKEN_OP_AMP){

            TokenList sublist;
            if(i>start){

            sublist.items=tokens->items+start;
            sublist.count=i-start;
            sublist.capacity=sublist.count;

            if(i<tokens->count && tokens->items[i].type==TOKEN_OP_AMP){
                runbuff(state,&sublist);
            }
            else{
                sigset_t previous;
                int result;

                sigprocmask(SIG_BLOCK,&child_mask,&previous);
                result=runcomm(state,&sublist);
                sigprocmask(SIG_SETMASK,&previous,NULL);
                if(result<0) return 1;
            }
            } 
            start=i+1;
        }
        }
    return 1;
    
    }

int getjobcount(void) {
    return job_count;
}

int getjobinfo(int index, pid_t *pid, char *pid_name, int *live) {
    if (index < 0 || index >= job_count) return -1;

    *pid = job_pid[index];
    strncpy(pid_name, job_name[index], 32);

    pid_name[31]='\0';
    *live = job_live[index];
    return 0;
}