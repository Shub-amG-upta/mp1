#include "func.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *path;
    long long count;
    long long last;
} Visit;

static void free_visits(Visit *visits,size_t count){
    for(size_t i=0;i<count;i++) free(visits[i].path);
    free(visits);
}

static char *history_path(ShellState *state){
    const char *home=getenv("HOME");
    char *path;
    size_t len;

    if(home==NULL) home=state->home_dir;
    len=strlen(home)+strlen("/.cshell_frecency")+1;
    path=malloc(len);
    if(path==NULL) return NULL;
    snprintf(path,len,"%s/.cshell_frecency",home);
    return path;

}

static int load_visits(const char *file,Visit **result,size_t *count,
                       long long *sequence){
    FILE *input;
    Visit *visits=NULL;
    char *line=NULL;
    size_t line_size=0;
    size_t used=0;

    *result=NULL;
    *count=0;
    *sequence=0;
    input=fopen(file,"r");
    if(input==NULL) return 0;

    while(getline(&line,&line_size,input)!=-1){
        char *first=strchr(line,'\t');
        char *second;
        char *end;
        Visit *temp;

        if(first==NULL) continue;
        *first='\0';
        second=strchr(first+1,'\t');
        if(second==NULL) continue;
        *second='\0';
        end=strchr(second+1,'\n');
        if(end!=NULL) *end='\0';

        temp=realloc(visits,(used+1)*sizeof(Visit));
        if(temp==NULL){
            free(line);
            fclose(input);
            free_visits(visits,used);
            return -1;
        }
        visits=temp;
        visits[used].count=strtoll(line,NULL,10);
        visits[used].last=strtoll(first+1,NULL,10);
        visits[used].path=strdup(second+1);
        if(visits[used].path==NULL){
            free(line);
            fclose(input);
            free_visits(visits,used);
            return -1;
        }
        if(visits[used].last>*sequence) *sequence=visits[used].last;
        used++;
    }

    free(line);
    fclose(input);
    *result=visits;
    *count=used;
    return 0;
}

static void save_visits(const char *file,Visit *visits,size_t count){
    FILE *output=fopen(file,"w");

    if(output==NULL) return;
    for(size_t i=0;i<count;i++){
        fprintf(output,"%lld\t%lld\t%s\n",visits[i].count,
                visits[i].last,visits[i].path);
    }
    fclose(output);
}

static void record_visit(ShellState *state,const char *path){
    Visit *visits;
    size_t count;
    long long sequence;
    char *file=history_path(state);
    int found=0;

    if(file==NULL) return;
    if(load_visits(file,&visits,&count,&sequence)!=0){
        free(file);
        return;
    }
    sequence++;

    for(size_t i=0;i<count;i++){
        if(strcmp(visits[i].path,path)==0){
            visits[i].count++;
            visits[i].last=sequence;
            found=1;
            break;
        }
    }

    if(found==0){
        Visit *temp=realloc(visits,(count+1)*sizeof(Visit));
        if(temp!=NULL){
            visits=temp;
            visits[count].path=strdup(path);
            if(visits[count].path!=NULL){
                visits[count].count=1;
                visits[count].last=sequence;
                count++;
            }
        }
    }

    save_visits(file,visits,count);
    free_visits(visits,count);
    free(file);
}

static char *find_visit(ShellState *state,const char *name){
    Visit *visits;
    size_t count;
    long long sequence;
    char *file=history_path(state);
    char *best=NULL;
    long long best_count=-1;
    long long best_last=-1;

    if(file==NULL) return NULL;
    if(load_visits(file,&visits,&count,&sequence)!=0){
        free(file);
        return NULL;
    }

    for(size_t i=0;i<count;i++){
        int better=0;

        if(access(visits[i].path,F_OK)!=0 || strstr(visits[i].path,name)==NULL){
            continue;
        }
        if(visits[i].count>best_count) better=1;
        else if(visits[i].count==best_count && visits[i].last>best_last){
            better=1;
        }else if(visits[i].count==best_count && visits[i].last==best_last &&
                (best==NULL || strcmp(visits[i].path,best)<0)){
            better=1;
        }
        if(better){
            free(best);
            best=strdup(visits[i].path);
            best_count=visits[i].count;
            best_last=visits[i].last;
        }
    }

    free_visits(visits,count);
    free(file);
    return best;
}

static int change_directory(ShellState *state, const char *path)
{
    char *old_dir;
    char *new_dir;

    old_dir = getcwd(NULL, 0);
    if (old_dir == NULL || chdir(path) != 0) {
        free(old_dir);
        return -1;
    }

    new_dir = getcwd(NULL, 0);
    if (new_dir == NULL) {
        free(old_dir);
        return -1;
    }

    if (strcmp(old_dir, new_dir) != 0) {
        free(state->previous_dir);
        state->previous_dir = old_dir;
        old_dir = NULL;
        record_visit(state,new_dir);
    }

    free(old_dir);
    free(new_dir);
    return 0;
}

static int do_hop(ShellState *state, const char *argument)
{
    const char *path;
    char *found;
    int result;

    if (strcmp(argument, "~") == 0) {
        path = state->home_dir;
    } else if (strcmp(argument, ".") == 0) {
        return 0;
    } else if (strcmp(argument, "-") == 0) {
        if (state->previous_dir == NULL) {
            return 0;
        }
        path = state->previous_dir;
    } else {
        path = argument;
        if(change_directory(state,path)==0) return 0;
        found=find_visit(state,argument);
        if(found==NULL) return -1;
        result=change_directory(state,found);
        free(found);
        return result;
    }

    return change_directory(state, path);
}

int run_hop(ShellState *state, const TokenList *tokens)
{
    size_t i;

    if (tokens->count == 0 || tokens->items[0].type != TOKEN_WORD ||
        strcmp(tokens->items[0].text, "hop") != 0) {
        return 0;
    }

    if (tokens->count == 1) {
        if (do_hop(state, "~") != 0) {
            fputs("hop: no such directory\n", stderr);
        }
        return 1;
    }

    for (i = 1; i < tokens->count; i++) {
        if (tokens->items[i].type != TOKEN_WORD) {
            break;
        }
        if (do_hop(state, tokens->items[i].text) != 0) {
            fputs("hop: no such directory\n", stderr);
        }
    }

    return 1;
}
