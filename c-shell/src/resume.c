#include "resume.h"
#include "d1d2.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define WAIT_DONE 0
#define WAIT_STOPPED 1
#define WAIT_TIMEOUT 2

static int parse_number(const char *text,long *value){
    char *end;

    if(text[0]<'0' || text[0]>'9') return -1;
    errno=0;
    *value=strtol(text,&end,10);
    if(errno!=0 || *end!='\0') return -1;
    return 0;
}

/* SIGCHLD is blocked by run_tokens, so only this loop reaps the group */
static int wait_group(pid_t pgid,long timeout){
    int status;
    int interrupted=0;

    if(timeout>0) start_job_timer(timeout);

    for(;;){
        pid_t result;

        if(timeout>0 && job_timer_fired()){
            stop_job_timer();
            kill(-pgid,SIGTERM);
            return WAIT_TIMEOUT;
        }

        result=waitpid(-pgid,&status,WUNTRACED);

        if(result>0){
            if(WIFSTOPPED(status)){
                if(timeout>0) stop_job_timer();
                return WAIT_STOPPED;
            }
            if(WIFSIGNALED(status) && WTERMSIG(status)==SIGINT){
                interrupted=1;
            }
            continue;
        }

        if(errno==EINTR) continue;

        /* ECHILD: every process in the group has finished */
        if(timeout>0) stop_job_timer();
        if(interrupted){
            putchar('\n');
            fflush(stdout);
        }
        return WAIT_DONE;
    }
}

int run_resume(ShellState *state,const TokenList *tokens){
    long number;
    long timeout=0;
    int foreground;
    int index;
    int outcome;
    pid_t pgid;

    (void)state;
    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
       strcmp(tokens->items[0].text,"resume")!=0) return 0;

    for(size_t i=0;i<tokens->count;i++){
        if(tokens->items[i].type!=TOKEN_WORD){
            fputs("resume: invalid syntax\n",stderr);
            return 1;
        }
    }

    if((tokens->count!=3 && tokens->count!=5) ||
       tokens->items[1].text[0]!='%' ||
       parse_number(tokens->items[1].text+1,&number)!=0){
        fputs("resume: invalid syntax\n",stderr);
        return 1;
    }

    if(strcmp(tokens->items[2].text,"fg")==0) foreground=1;
    else if(strcmp(tokens->items[2].text,"bg")==0) foreground=0;
    else{
        fputs("resume: invalid syntax\n",stderr);
        return 1;
    }

    if(tokens->count==5){
        if(foreground==0 ||
           strcmp(tokens->items[3].text,"--timeout")!=0 ||
           parse_number(tokens->items[4].text,&timeout)!=0 ||
           timeout<=0){
            fputs("resume: invalid syntax\n",stderr);
            return 1;
        }
    }

    index=(number>0 && number<=100000) ? find_job((int)number,&pgid) : -1;
    if(index<0){
        fputs("resume: no such job\n",stderr);
        return 1;
    }

    if(foreground==0){
        set_job_stopped(index,0);
        kill(-pgid,SIGCONT);
        printf("[%d] + Running   %s\n",index+1,job_command_text(index));
        fflush(stdout);
        return 1;
    }

    /* inside a background job there is no terminal to hand over */
    if(is_background_child()){
        fputs("resume: invalid syntax\n",stderr);
        return 1;
    }

    printf("%s\n",job_command_text(index));
    fflush(stdout);

    giveterminal(pgid);
    set_job_stopped(index,0);
    kill(-pgid,SIGCONT);

    outcome=wait_group(pgid,timeout);

    giveterminal(getpgrp());

    if(outcome==WAIT_STOPPED){
        set_job_stopped(index,1);
        print_stopped(index+1);
    }
    else{
        finish_job(index);
        if(outcome==WAIT_TIMEOUT){
            fputs("resume: job timed out\n",stderr);
        }
    }
    return 1;
}