#include "d1d2.h"
#include "c3c4.h"
#include "execute.h"
#include "func.h"
#include "reveal.h"
#include "activities.h"
#include "parser.h"
#include "resume.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include<string.h>
#include <fcntl.h>
#include <sys/time.h>                            

extern char **environ;  
#define MAX_JOBS 128

static pid_t job_pid[MAX_JOBS];
static char job_name[MAX_JOBS][32];
static volatile sig_atomic_t job_live[MAX_JOBS];
static volatile sig_atomic_t job_stopped[MAX_JOBS];
static volatile sig_atomic_t job_leader_done[MAX_JOBS];        
static char job_command[MAX_JOBS][256];                        
static int job_count=0;
static sigset_t child_mask;

static int shellterminal=-1;
static pid_t shellpgid=-1;

static int backgroundchild=0;
static volatile sig_atomic_t interrupted=0;
static volatile sig_atomic_t alarm_fired=0;


static void handle_interrupt(int signal_number){
    (void)signal_number;
    interrupted=1;
}


static void handle_alarm(int signal_number){
    (void)signal_number;
    alarm_fired=1;
}


void start_job_timer(long seconds){
    struct itimerval timer;

    alarm_fired=0;
    timer.it_value.tv_sec=seconds;
    timer.it_value.tv_usec=0;
    timer.it_interval.tv_sec=0;
    timer.it_interval.tv_usec=50000;
    setitimer(ITIMER_REAL,&timer,NULL);
}


void stop_job_timer(void){
    struct itimerval timer={{0,0},{0,0}};
    setitimer(ITIMER_REAL,&timer,NULL);
}


int job_timer_fired(void){
    return alarm_fired;
}


/* children inherit the blocked SIGCHLD from run_tokens, execve keeps it */
void reset_child_mask(void){
    sigset_t unblock;

    signal(SIGCHLD,SIG_DFL);
    signal(SIGALRM,SIG_DFL);
    signal(SIGTTIN,SIG_DFL);

    sigemptyset(&unblock);
    sigaddset(&unblock,SIGCHLD);
    sigprocmask(SIG_UNBLOCK,&unblock,NULL);
}


int take_interrupt(void){
    int value=interrupted;
    interrupted=0;
    return value;
}


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

    while((pid=waitpid(-1,&status,WNOHANG|WUNTRACED|WCONTINUED))>0){

        for(int i=0;i<job_count;i++){

            if(job_live[i] && job_pid[i]==pid){

                if(WIFSTOPPED(status)){
                    job_stopped[i]=1;
                }
                else if(WIFCONTINUED(status)){
                    job_stopped[i]=0;
                }
                else if(job_leader_done[i]==0){

                    job_leader_done[i]=1;

                    report(job_name[i],
                          pid,
                          WIFEXITED(status));
                }

                break;
            }
        }
    }

    /* a job stays listed until every process in its group is gone */
    for(int i=0;i<job_count;i++){

        if(job_live[i] && job_leader_done[i] && kill(-job_pid[i],0)!=0){
            job_live[i]=0;
            job_stopped[i]=0;
        }
    }

    errno=saved;
}


static void initterminal(void){    

    shellterminal=open("/dev/tty",O_RDWR);

    if(shellterminal<0){
        perror("cshell: unable to open terminal");
        return;
    }

    shellpgid=getpgrp();

    if(tcsetpgrp(shellterminal,shellpgid)!=0){
        perror("cshell: tcsetpgrp failed");
    }

    struct sigaction action;

    action.sa_handler=handle_interrupt;
    sigemptyset(&action.sa_mask);
    action.sa_flags=0;

    sigaction(SIGINT,&action,NULL);
    sigaction(SIGTSTP,&action,NULL);

    signal(SIGTTOU,SIG_IGN);

    action.sa_handler=handle_alarm;
    sigaction(SIGALRM,&action,NULL);
}


int getshellterminal(void){ 
    return shellterminal;
}


void giveterminal(pid_t pgid){ 
    if(shellterminal>=0){
        tcsetpgrp(shellterminal,pgid);
    }
}


int is_background_child(void){
    return backgroundchild;
}


void set_background_child(int value){
    backgroundchild=value;
}


static void save_command(int index,const TokenList *tokens){
    size_t pos=0;

    job_command[index][0]='\0';

    for(size_t i=0;i<tokens->count && pos<255;i++){

        if(pos>0 && pos<255){
            job_command[index][pos++]=' ';
        }

        for(size_t j=0;
            tokens->items[i].text[j]!='\0' && pos<255;
            j++){

            job_command[index][pos++]=tokens->items[i].text[j];
        }
    }

    job_command[index][pos]='\0';
}


int add_stopped_job(pid_t pgid,const TokenList *tokens){

    if(job_count==MAX_JOBS){
        fputs("cshell: too many jobs\n",stderr);
        return -1;
    }

    job_pid[job_count]=pgid;
    job_live[job_count]=1;
    job_stopped[job_count]=1;
    job_leader_done[job_count]=0;

    const char *name;

    if(tokens->count>0){
        name=tokens->items[0].text;
    }
    else{
        name="unknown";
    }

    if(name[0]=='%'){
        name++;
    }

    int i;

    for(i=0;name[i]!='\0' && i<31;i++){
        job_name[job_count][i]=name[i];
    }

    job_name[job_count][i]='\0';

    save_command(job_count,tokens);

    job_count++;

    return job_count;
}


void print_stopped(int number){
    if(number<1 || number>job_count) return;

    printf("\n[%d] + Stopped   %s\n",number,job_command[number-1]);
    fflush(stdout);
}


int has_stopped_jobs(void){

    for(int i=0;i<job_count;i++){

        if(job_live[i] && job_stopped[i]){
            return 1;
        }
    }

    return 0;
}


void shutdown_jobs(void){

    for(int i=0;i<job_count;i++){

        if(job_live[i]){
            kill(-job_pid[i],SIGHUP);
            kill(-job_pid[i],SIGCONT);
        }
    }
}


void buff(void){

    initterminal();

    struct sigaction action;

    sigemptyset(&child_mask);
    sigaddset(&child_mask,SIGCHLD);

    action.sa_handler=handle_sigchld;
    sigemptyset(&action.sa_mask);

    action.sa_flags=SA_RESTART;

    if(sigaction(SIGCHLD,&action,NULL)!=0){
        perror("cshell: sigaction failed");
    }
}


static int not_found(const TokenList *tokens){
    const char *name;
    char *path;
    int path_only=0;

    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD) return 0;

    name=tokens->items[0].text;

    if(name[0]=='%'){
        path_only=1;
        name++;
    }

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
    && runactivity(state,tokens)==0
    && run_resume(state,tokens)==0){

        int missing=not_found(tokens);

        run_external(state,tokens);

        return missing ? -1 : 1;
    }

    return 1;
}


int is_builtin(const char *name){
    return strcmp(name,"hop")==0 || strcmp(name,"reveal")==0 ||
           strcmp(name,"peek")==0 || strcmp(name,"locate")==0 ||
           strcmp(name,"activities")==0 || strcmp(name,"resume")==0;
}


static int has_pipe(const TokenList *tokens){
    for(size_t i=0;i<tokens->count;i++){
        if(tokens->items[i].type==TOKEN_OP_PIPE) return 1;
    }
    return 0;
}


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

    if(has_pipe(tokens)){
        child=launch_background_pipeline(state,tokens);
    }
    else{
        child=fork();
    }

    if(child<0){

        sigprocmask(SIG_SETMASK,&previous,NULL);

        if(has_pipe(tokens)==0){
            perror("cshell: fork failed");
        }

        return 1;
    }

    if(child==0){

        setpgid(0,0);

        set_background_child(1);

        reset_child_mask();

        signal(SIGINT,SIG_DFL);
        signal(SIGTSTP,SIG_DFL);
        signal(SIGTTOU,SIG_DFL);

        sigprocmask(SIG_SETMASK,&previous,NULL);

        if(simple_external(tokens)){

            char *argv[64];
            const char *cmd=tokens->items[0].text;
            char *path;
            size_t n=0;
            int path_only=0;

            while(n<tokens->count && n<63){
                argv[n]=tokens->items[n].text;
                n++;
            }

            argv[n]=NULL;

            if(cmd[0]=='%'){
                path_only=1;
                cmd++;
                argv[0]=(char *)cmd;
            }

            path=find_executable(cmd,path_only);

            if(path!=NULL){
                execve(path,argv,environ);
            }

            fprintf(stderr,
                    "cshell: command not found (%s)\n",
                    cmd);

            _exit(127);
        }

        runcomm(state,tokens);

        _exit(0);
    }

    setpgid(child,child);

    name=tokens->items[0].text;

    if(name[0]=='%'){
        name++;
    }

    for(i=0;name[i]!='\0' && i<31;i++){
        job_name[job_count][i]=name[i];
    }

    job_name[job_count][i]='\0';

    job_pid[job_count]=child;
    job_live[job_count]=1;
    job_stopped[job_count]=0;
    job_leader_done[job_count]=0;

    save_command(job_count,tokens);

    job_count++;

    printf("[%d] %d\n",
           job_count,
           (int)child);

    fflush(stdout);

    sigprocmask(SIG_SETMASK,&previous,NULL);

    return 1;
}


static int run_tokens(ShellState *state,const TokenList *tokens){
    size_t start=0;

    for(size_t i=0;i<=tokens->count;i++){

        if(i==tokens->count || 
        tokens->items[i].type==TOKEN_OP_SEMI || 
        tokens->items[i].type==TOKEN_OP_AMP){

            TokenList sublist;

            if(i>start){

                sublist.items=tokens->items+start;
                sublist.count=i-start;
                sublist.capacity=sublist.count;

                if(i<tokens->count && 
                tokens->items[i].type==TOKEN_OP_AMP){

                    runbuff(state,&sublist);
                }
                else{

                    sigset_t previous;
                    int result;

                    sigprocmask(SIG_BLOCK,
                                &child_mask,
                                &previous);

                    result=runcomm(state,&sublist);

                    sigprocmask(SIG_SETMASK,
                                &previous,
                                NULL);

                    if(result<0) return 1;
                }
            }

            start=i+1;
        }
    }

    return 1;
}


int run(const char *line,ShellState *state){
    TokenList tokens;
    int invalid;

    token_list_init(&tokens);
    invalid=lex_line(line,&tokens);
    if(invalid==0) invalid=parse_tokens(&tokens);

    if(invalid!=0){
        fputs("cshell: invalid syntax\n",stderr);
    }else{
        run_tokens(state,&tokens);
    }

    token_list_destroy(&tokens);
    return 1;
}


int getjobcount(void){
    return job_count;
}


int getjobinfo(int index,pid_t *pid,char *pid_name,int *live){

    if(index<0 || index>=job_count) return -1;

    *pid=job_pid[index];

    strncpy(pid_name,
            job_name[index],
            32);

    pid_name[31]='\0';

    *live=job_live[index];

    return 0;
}


int find_job(int number,pid_t *pgid){
    int index=number-1;

    if(index<0 || index>=job_count || job_live[index]==0) return -1;

    *pgid=job_pid[index];

    return index;
}


void set_job_stopped(int index,int value){
    if(index>=0 && index<job_count) job_stopped[index]=value;
}


void finish_job(int index){
    if(index>=0 && index<job_count){
        job_live[index]=0;
        job_stopped[index]=0;
    }
}


const char *job_command_text(int index){
    if(index<0 || index>=job_count) return "";
    return job_command[index];
}