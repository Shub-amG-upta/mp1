#include "ping.h"
#include "d1d2.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int parse_signal(const char *text,int *value){
    int result=0;

    if(text[0]=='\0') return -1;
    for(size_t i=0;text[i]!='\0';i++){
        if(text[i]<'0' || text[i]>'9') return -1;
        result=(result*10+(text[i]-'0'))%64;
    }
    *value=result;
    return 0;
}

static int parse_id(const char *text,long *value){
    char *end;

    if(text[0]<'0' || text[0]>'9') return -1;
    errno=0;
    *value=strtol(text,&end,10);
    if(errno!=0 || *end!='\0') return -1;
    return 0;
}

int run_ping(ShellState *state,const TokenList *tokens){
    const char *target;
    const char *number;
    int signal_value;
    long id;
    int result;

    (void)state;
    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
       strcmp(tokens->items[0].text,"ping")!=0) return 0;

    if(tokens->count!=3 || tokens->items[1].type!=TOKEN_WORD ||
       tokens->items[2].type!=TOKEN_WORD){
        fputs("ping: invalid syntax\n",stderr);
        return 1;
    }

    target=tokens->items[1].text;
    number=tokens->items[2].text;

   
    if(parse_signal(number,&signal_value)!=0){
        fputs("ping: invalid syntax\n",stderr);
        return 1;
    }

    if(target[0]=='%'){
        pid_t pgid;

        if(parse_id(target+1,&id)!=0 || id<=0 || id>100000 ||
           find_job((int)id,&pgid)<0){
            fputs("ping: no such process found\n",stderr);
            return 1;
        }
        result=kill(-pgid,signal_value);
    }
    else{
        if(parse_id(target,&id)!=0 || id<=0 || id>100000000 ||
           is_tracked_pid((pid_t)id)==0){
            fputs("ping: no such process found\n",stderr);
            return 1;
        }
        result=kill((pid_t)id,signal_value);
    }

    if(result!=0){
        fputs("ping: no such process found\n",stderr);
        return 1;
    }

    printf("Sent signal %s to %s\n",number,target);
    fflush(stdout);
    return 1;
}