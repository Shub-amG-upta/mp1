#include "snoop.h"
#include "d1d2.h"
#include "execute.h"
#include "syscalls.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_ENTRIES 512

extern char **environ;

typedef struct {
    long number;
    long calls;
    double seconds;
} Entry;

static Entry entries[MAX_ENTRIES];
static size_t entry_count;

static double now_seconds(void){
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC,&t);
    return (double)t.tv_sec+(double)t.tv_nsec/1e9;
}

static Entry *find_entry(long number){
    for(size_t i=0;i<entry_count;i++){
        if(entries[i].number==number) return &entries[i];
    }
    if(entry_count==MAX_ENTRIES) return NULL;
    entries[entry_count]=(Entry){number,0,0};
    return &entries[entry_count++];
}

static void detach(pid_t pid){
    int status;

    kill(pid,SIGSTOP);
    while(waitpid(pid,&status,0)>0 && WIFSTOPPED(status) &&
          WSTOPSIG(status)!=SIGSTOP){
        ptrace(PTRACE_CONT,pid,0,0);
    }
    ptrace(PTRACE_DETACH,pid,0,0);
    kill(pid,SIGCONT);
    putchar('\n');
}

static int trace(pid_t pid,int attached,int *final){
    struct __ptrace_syscall_info info;
    Entry *open_call=NULL;
    double started=0;
    int status;
    int deliver=0;

    for(;;){
        if(ptrace(PTRACE_SYSCALL,pid,0,deliver)!=0) return 0;

        while(waitpid(pid,&status,0)<0){
            if(errno!=EINTR) return 0;
            if(attached && take_interrupt()){
                detach(pid);
                return 0;
            }
        }

        if(WIFEXITED(status) || WIFSIGNALED(status)){
            if(WIFSIGNALED(status) && WTERMSIG(status)==SIGINT) putchar('\n');
            *final=status;
            return 1;
        }

        deliver=WSTOPSIG(status);
        if(deliver!=(SIGTRAP|0x80)) continue;
        deliver=0;

        if(ptrace(PTRACE_GET_SYSCALL_INFO,pid,sizeof(info),&info)<=0) continue;

        if(info.op==PTRACE_SYSCALL_INFO_ENTRY){
            open_call=find_entry((long)info.entry.nr);
            if(open_call!=NULL) open_call->calls++;
            started=now_seconds();
        }
        else if(info.op==PTRACE_SYSCALL_INFO_EXIT && open_call!=NULL){
            open_call->seconds+=now_seconds()-started;
            open_call=NULL;
        }
    }
}

static void print_summary(void){
    for(size_t i=1;i<entry_count;i++){
        Entry key=entries[i];
        size_t j=i;

        for(;j>0 && entries[j-1].calls<key.calls;j--) entries[j]=entries[j-1];
        entries[j]=key;
    }

    printf("%-20s %-8s %s\n","syscall","calls","time");
    for(size_t i=0;i<entry_count;i++){
        const char *name=syscall_name(entries[i].number);
        char unknown[32];

        if(name==NULL){
            snprintf(unknown,sizeof(unknown),"syscall_%ld",entries[i].number);
            name=unknown;
        }
        printf("%-20s %-8ld %.3fs\n",name,entries[i].calls,entries[i].seconds);
    }
    fflush(stdout);
}

static int snoop_pid(const char *text){
    char *end;
    long pid;
    int status;

    errno=0;
    pid=strtol(text,&end,10);

    if(text[0]<'0' || text[0]>'9' || *end!='\0' || pid<=0 || pid>100000000 ||
       ptrace(PTRACE_ATTACH,(pid_t)pid,0,0)!=0){
        fputs(errno==EPERM ? "snoop: permission denied\n"
                           : "snoop: no such process\n",stderr);
        return -1;
    }
    while(waitpid((pid_t)pid,&status,0)<0 && errno==EINTR){
    }

    take_interrupt();
    ptrace(PTRACE_SETOPTIONS,(pid_t)pid,0,PTRACE_O_TRACESYSGOOD);

    if(trace((pid_t)pid,1,&status)) note_traced_exit((pid_t)pid,status);
    return 0;
}

static int snoop_command(const TokenList *tokens){
    char *name=tokens->items[1].text;
    char *argv[tokens->count];
    char *path;
    int path_only=(name[0]=='%');
    int status=0;
    pid_t group=is_background_child() ? getpgrp() : 0;
    pid_t child;

    name+=path_only;

    path=find_executable(name,path_only);
    if(path==NULL){
        fputs("snoop: command not found\n",stderr);
        return -1;
    }

    argv[0]=name;
    for(size_t i=2;i<tokens->count;i++) argv[i-1]=tokens->items[i].text;
    argv[tokens->count-1]=NULL;

    fflush(stdout);
    child=fork();
    if(child==0){
        setpgid(0,group);

        signal(SIGINT,SIG_DFL);
        signal(SIGTSTP,SIG_DFL);
        signal(SIGTTOU,SIG_DFL);
        reset_child_mask();

        ptrace(PTRACE_TRACEME,0,0,0);
        raise(SIGSTOP);
        exec_command(path,argv);
        _exit(127);
    }
    free(path);
    if(child<0){
        perror("snoop: fork failed");
        return -1;
    }

    setpgid(child,group==0 ? child : group);
    while(waitpid(child,&status,0)<0 && errno==EINTR){
    }

    ptrace(PTRACE_SETOPTIONS,child,0,
           PTRACE_O_TRACESYSGOOD|PTRACE_O_TRACEEXEC|PTRACE_O_EXITKILL);

    if(group==0) giveterminal(child);
    trace(child,0,&status);
    if(group==0) giveterminal(getpgrp());
    return 0;
}

int run_snoop(ShellState *state,const TokenList *tokens){
    int is_pid;

    (void)state;
    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
       strcmp(tokens->items[0].text,"snoop")!=0) return 0;

    is_pid=tokens->count>1 && strcmp(tokens->items[1].text,"-p")==0;

    if(tokens->count<2 || (is_pid && tokens->count!=3)){
        fputs("snoop: invalid syntax\n",stderr);
        return 1;
    }

    entry_count=0;
    if((is_pid ? snoop_pid(tokens->items[2].text)
               : snoop_command(tokens))==0){
        print_summary();
    }
    return 1;
}