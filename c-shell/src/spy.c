#include "spy.h"
#include "d1d2.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SPY_PATH 4096

static const char *type_name(const char *path){
    struct stat info;

    if(stat(path,&info)!=0) return "unknown";
    if(S_ISREG(info.st_mode)) return "REG";
    if(S_ISDIR(info.st_mode)) return "DIR";
    if(S_ISCHR(info.st_mode)) return "CHR";
    if(S_ISBLK(info.st_mode)) return "BLK";
    if(S_ISFIFO(info.st_mode)) return "FIFO";
    if(S_ISSOCK(info.st_mode)) return "SOCK";
    if(S_ISLNK(info.st_mode)) return "LINK";
    return "a_inode";
}

static void print_row(pid_t pid,const char *fd,const char *type,
                      const char *path){
    printf("%-8d %-8s %-8s %s\n",(int)pid,fd,type,path);
}

static void print_link(pid_t pid,const char *entry,const char *label,
                       char *target,size_t size){
    char link[64];
    ssize_t length;

    snprintf(link,sizeof(link),"/proc/%d/%s",(int)pid,entry);
    length=readlink(link,target,size-1);
    if(length<0){
        target[0]='\0';
        print_row(pid,label,"unknown",
                  errno==EACCES ? "(permission denied)" : "(unreadable)");
        return;
    }
    target[length]='\0';
    print_row(pid,label,type_name(link),target);
}

static int seen_before(char **paths,size_t count,const char *path){
    for(size_t i=0;i<count;i++){
        if(strcmp(paths[i],path)==0) return 1;
    }
    return 0;
}

static void print_mem(pid_t pid,const char *exe){
    char name[64];
    char *line=NULL;
    size_t line_size=0;
    char **paths=NULL;
    size_t count=0;
    FILE *maps;

    snprintf(name,sizeof(name),"/proc/%d/maps",(int)pid);
    maps=fopen(name,"r");
    if(maps==NULL) return;

    while(getline(&line,&line_size,maps)!=-1){
        unsigned long inode;
        int offset=0;
        char *path;
        char **temp;

        if(sscanf(line,"%*s %*s %*s %*s %lu %n",&inode,&offset)!=1) continue;
        if(inode==0 || offset==0) continue;

        path=line+offset;
        path[strcspn(path,"\n")]='\0';
        if(path[0]!='/') continue;
        if(strcmp(path,exe)==0 || seen_before(paths,count,path)) continue;

        temp=realloc(paths,(count+1)*sizeof(char *));
        if(temp==NULL) break;
        paths=temp;
        paths[count]=strdup(path);
        if(paths[count]==NULL) break;
        count++;

        print_row(pid,"mem",type_name(path),path);
    }

    for(size_t i=0;i<count;i++) free(paths[i]);
    free(paths);
    free(line);
    fclose(maps);
}

static int compare_fds(const void *first,const void *second){
    int a=*(const int *)first;
    int b=*(const int *)second;

    return (a>b)-(a<b);
}

static void print_fds(pid_t pid){
    char name[64];
    DIR *directory;
    struct dirent *entry;
    int *fds=NULL;
    size_t count=0;
    int own=-1;

    snprintf(name,sizeof(name),"/proc/%d/fd",(int)pid);
    directory=opendir(name);
    if(directory==NULL) return;

    if(pid==getpid()) own=dirfd(directory);

    while((entry=readdir(directory))!=NULL){
        int *temp;
        int fd;

        if(entry->d_name[0]<'0' || entry->d_name[0]>'9') continue;
        fd=atoi(entry->d_name);
        if(fd==own) continue;

        temp=realloc(fds,(count+1)*sizeof(int));
        if(temp==NULL) break;
        fds=temp;
        fds[count++]=fd;
    }
    closedir(directory);

    qsort(fds,count,sizeof(int),compare_fds);

    for(size_t i=0;i<count;i++){
        char link[64];
        char label[16];
        char target[SPY_PATH];
        ssize_t length;

        snprintf(link,sizeof(link),"/proc/%d/fd/%d",(int)pid,fds[i]);
        length=readlink(link,target,sizeof(target)-1);
        if(length<0) continue;
        target[length]='\0';

        snprintf(label,sizeof(label),"%d",fds[i]);
        print_row(pid,label,type_name(link),target);
    }
    free(fds);
}

int run_spy(ShellState *state,const TokenList *tokens){
    pid_t pid=get_shell_pid();
    char proc[64];
    char exe[SPY_PATH];
    char cwd[SPY_PATH];
    struct stat info;

    (void)state;
    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
       strcmp(tokens->items[0].text,"spy")!=0) return 0;

    for(size_t i=1;i<tokens->count;i++){
        if(tokens->items[i].type!=TOKEN_WORD){
            fputs("spy: invalid syntax\n",stderr);
            return 1;
        }
    }

    if(tokens->count>2){
        fputs("spy: invalid syntax\n",stderr);
        return 1;
    }

    if(tokens->count==2){
        const char *text=tokens->items[1].text;
        char *end;
        long value;

        errno=0;
        value=strtol(text,&end,10);
        if(text[0]<'0' || text[0]>'9' || *end!='\0' || errno!=0 ||
           value<=0 || value>100000000){
            fputs("spy: no such process\n",stderr);
            return 1;
        }
        pid=(pid_t)value;
    }

    snprintf(proc,sizeof(proc),"/proc/%d",(int)pid);
    if(stat(proc,&info)!=0){
        fputs("spy: no such process\n",stderr);
        return 1;
    }

    printf("%-8s %-8s %-8s %s\n","PID","FD","TYPE","PATH");
    print_link(pid,"cwd","cwd",cwd,sizeof(cwd));
    print_link(pid,"exe","txt",exe,sizeof(exe));
    print_mem(pid,exe);
    print_fds(pid);
    fflush(stdout);
    return 1;
}