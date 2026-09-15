#include "reveal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char *name;
    int directory;
} RevealEntry;

static void free_entries(RevealEntry *entries,size_t count){
    for(size_t i=0;i<count;i++) free(entries[i].name);
    free(entries);
}

static int compare_entries(const void *first,const void *second){
    const RevealEntry *a=first;
    const RevealEntry *b=second;

    return strcmp(a->name,b->name);
}

static char *absolute_path(const char *path){
    return realpath(path,NULL);
}

static char *join_path(const char *directory,const char *name){
    char *path;
    size_t len=strlen(directory)+strlen(name)+2;

    path=malloc(len);
    if(path==NULL) return NULL;
    snprintf(path,len,"%s/%s",directory,name);
    return path;
}

static char *resolve_reveal_path(ShellState *state,const char *argument){
    char *path;

    if(argument==NULL || strcmp(argument,".")==0){
        return absolute_path(".");
    }
    if(strcmp(argument,"~")==0){
        return absolute_path(state->home_dir);
    }
    if(strcmp(argument,"-")==0){
        if(state->previous_dir==NULL) return NULL;
        return absolute_path(state->previous_dir);
    }

    path=absolute_path(argument);
    return path;
}

static int read_entries(const char *path,int show_hidden,
                        RevealEntry **result,size_t *count){
    DIR *directory;
    struct dirent *entry;
    RevealEntry *entries=NULL;
    size_t used=0;

    directory=opendir(path);
    if(directory==NULL) return -1;

    while((entry=readdir(directory))!=NULL){
        char *child_path;
        struct stat info;
        RevealEntry *temp;

        if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0){
            continue;
        }
        if(show_hidden==0 && entry->d_name[0]=='.') continue;

        child_path=join_path(path,entry->d_name);
        if(child_path==NULL){
            closedir(directory);
            free_entries(entries,used);
            return -1;
        }

        if(lstat(child_path,&info)!=0){
            free(child_path);
            continue;
        }

        temp=realloc(entries,(used+1)*sizeof(RevealEntry));
        if(temp==NULL){
            free(child_path);
            closedir(directory);
            free_entries(entries,used);
            return -1;
        }
        entries=temp;
        entries[used].name=strdup(entry->d_name);
        entries[used].directory=S_ISDIR(info.st_mode);
        free(child_path);

        if(entries[used].name==NULL){
            closedir(directory);
            free_entries(entries,used);
            return -1;
        }
        used++;
    }

    closedir(directory);
    qsort(entries,used,sizeof(RevealEntry),compare_entries);
    *result=entries;
    *count=used;
    return 0;
}

static void print_directory(const char *path,const char *shown_path,
                            int show_hidden,int recursive){
    RevealEntry *entries=NULL;
    size_t count=0;

    if(read_entries(path,show_hidden,&entries,&count)!=0) return;

    for(size_t i=0;i<count;i++){
        char *child_path=join_path(path,entries[i].name);
        char *child_shown;

        if(shown_path[0]=='\0'){
            child_shown=strdup(entries[i].name);
        }else{
            child_shown=join_path(shown_path,entries[i].name);
        }

        if(child_path==NULL || child_shown==NULL){
            free(child_path);
            free(child_shown);
            continue;
        }

        if(entries[i].directory){
            /* B2 #6: only -t shows the trailing '/' */
            printf(recursive ? "%s/\n" : "%s\n",child_shown);
            if(recursive){
                print_directory(child_path,child_shown,show_hidden,recursive);
            }
        }else{
            printf("%s\n",child_shown);
        }

        free(child_path);
        free(child_shown);
    }

    free_entries(entries,count);
}

int run_reveal(ShellState *state,const TokenList *tokens){
    size_t i=1;
    int show_hidden=0;
    int recursive=0;
    const char *argument=NULL;
    char *path;
    struct stat info;

    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
       strcmp(tokens->items[0].text,"reveal")!=0) return 0;

    while(i<tokens->count && tokens->items[i].type==TOKEN_WORD){
        const char *flag=tokens->items[i].text;

        if(flag[0]!='-' || strcmp(flag,"-")==0) break;
        for(size_t j=1;flag[j]!='\0';j++){
            if(flag[j]=='a') show_hidden=1;
            else if(flag[j]=='t') recursive=1;
            else{
                fputs("reveal: invalid syntax\n",stderr);
                return 1;
            }
        }
        i++;
    }

    if(i<tokens->count){
        if(tokens->items[i].type!=TOKEN_WORD){
            fputs("reveal: invalid syntax\n",stderr);
            return 1;
        }
        argument=tokens->items[i].text;
        i++;
    }
    if(i<tokens->count && tokens->items[i].type==TOKEN_WORD){
        fputs("reveal: invalid syntax\n",stderr);
        return 1;
    }

    path=resolve_reveal_path(state,argument);
    if(path==NULL || stat(path,&info)!=0){
        free(path);
        fputs("reveal: no such directory\n",stderr);
        return 1;
    }
    if(!S_ISDIR(info.st_mode)){
        free(path);
        fputs("reveal: no such directory\n",stderr);
        return 1;
    }

    print_directory(path,"",show_hidden,recursive);
    free(path);
    return 1;
}