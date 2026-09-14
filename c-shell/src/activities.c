#include "activities.h"
#include "d1d2.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct{
    pid_t pid;
    char name[64];
    char state;
} member;

static int readstat(pid_t pid, char *name, size_t size, char *state, pid_t *group){

    char path[64];
    snprintf(path,sizeof(path),"/proc/%d/stat",pid);
    FILE *file=fopen(path,"r");
    if(file==NULL) return -1;
    char buffer[512];
    if(fgets(buffer,sizeof(buffer),file)==NULL){
        fclose(file);
        return -1;
    }
    fclose(file);

    char *start=strchr(buffer,'(');
    char *end=strrchr(buffer,')');
    if(start==NULL || end==NULL || end<=start){
        return -1;
    }

    size_t len=end-start-1;
    if(len>=size) len=size-1;
    memcpy(name,start+1,len);
    name[len]='\0';
    int ppid,pgrp;
    if(sscanf(end+2,"%c %d %d",state,&ppid,&pgrp)!=3){
        return -1;
    }
    *group=pgrp;
    return 0;

}

static int comp(const void *a,const void *b){
    const member *ma=(const member *)a;
    const member *mb=(const member *)b;
    if(ma->pid<mb->pid) return -1;
    if(ma->pid>mb->pid) return 1;
    return 0;
}

static int collectgrp(pid_t grp,member *mems,int max,const char *self){

    DIR *directory;
    struct  dirent *entry;
    int count=0;
    directory=opendir("/proc");
    if(directory==NULL) return 0;

    while ((entry=readdir(directory))!=NULL && count<max)
    {
        pid_t pid;
        pid_t pgrp;
        char name[64];
        char state;
        /* code */

        if(entry->d_name[0]<'0' || entry->d_name[0]>'9') continue;
        pid=(pid_t)atoi(entry->d_name);
        if(readstat(pid,name,sizeof(name),&state,&pgrp)!=0) continue;
        if(pgrp!=grp) continue;
        if(state=='Z') continue;
        if(pid==grp && strcmp(name,self)==0) continue;

        mems[count].pid=pid;
        snprintf(mems[count].name,64,"%s",name);
        mems[count].state=state;
        count++;

    }

    closedir(directory);
    qsort(mems,count,sizeof(member),comp);
    return count;

}

static const char *statename(char state){
    if(state=='T' || state=='t'){
        return "Stopped";
    }
    return "Running";
}


int runactivity(ShellState *state,const TokenList *tokens)
{


(void)state;

if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
    strcmp(tokens->items[0].text,"activities")!=0) return 0;
if(tokens->count>1){
    fputs("activities: invalid syntax\n", stderr);
    return 1;
}

char self[64];
char selfbuff;
pid_t selfgrp;

if(readstat(getpid(),self,sizeof(self),&selfbuff,&selfgrp)!=0) self[0]='\0';
int total=getjobcount();

for(int i=0;i<total;i++){
    pid_t pgid;
    char job_name[32];
    int live;

    member mems[128];
    int count;

    if(getjobinfo(i,&pgid,job_name,&live)!=0) continue;
    if(!live) continue;

    count=collectgrp(pgid,mems,128,self);
    if(count<=0) continue;

    printf("[%d] pgid %d\n",i+1,(int)pgid);

    for(int j=0;j<count;j++){
        printf("  %d %s %s\n",(int)mems[j].pid,mems[j].name,statename(mems[j].state));
    }

}
return 1;

}

