#include "func.h"

#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define PEEK_CHUNK 4096

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

static int is_executable_file(const char *path){
    struct stat info;

    if(stat(path,&info)!=0) return 0;
    if(!S_ISREG(info.st_mode)) return 0;
    if(access(path,X_OK)!=0) return 0;
    return 1;
}

static char *make_candidate(const char *directory,const char *name){
    char *cwd;
    char *path;
    size_t len;

    if(directory[0]=='/'){
        len=strlen(directory)+strlen(name)+2;
        path=malloc(len);
        if(path==NULL) return NULL;
        snprintf(path,len,"%s/%s",directory,name);
        return path;
    }

    cwd=getcwd(NULL,0);
    if(cwd==NULL) return NULL;
    len=strlen(cwd)+strlen(directory)+strlen(name)+3;
    path=malloc(len);
    if(path!=NULL) snprintf(path,len,"%s/%s/%s",cwd,directory,name);
    free(cwd);
    return path;
}

static int search_current(const char *name){
    char *cwd=getcwd(NULL,0);
    char *path;
    int found=0;

    if(cwd==NULL) return 0;
    path=make_candidate(cwd,name);
    if(path!=NULL && is_executable_file(path)){
        printf("%s\n",path);
        found=1;
    }
    free(path);
    free(cwd);
    return found;
}

static int search_path(const char *name){
    const char *path_value=getenv("PATH");
    char *path_copy;
    char *directory;
    char *save=NULL;
    char *path;
    int found=0;

    if(path_value==NULL) return 0;
    path_copy=strdup(path_value);
    if(path_copy==NULL) return 0;

    directory=strtok_r(path_copy,":",&save);
    while(directory!=NULL){
        if(directory[0]=='\0') directory=".";
        path=make_candidate(directory,name);
        if(path!=NULL && is_executable_file(path)){
            printf("%s\n",path);
            found=1;
        }
        free(path);
        directory=strtok_r(NULL,":",&save);
    }

    free(path_copy);
    return found;
}

static void locate_one(const char *name){
    int found=search_current(name);

    if(search_path(name)!=0) found=1;
    if(found==0){
        fprintf(stderr,"locate: command not found (%s)\n",name);
    }
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int newline;
} PeekLine;

typedef struct {
    PeekLine *items;
    size_t count;
    size_t cap;
} PeekLines;

static void free_line(PeekLine *line){
    free(line->data);
    line->data=NULL;
    line->len=0;
    line->cap=0;
    line->newline=0;
}

static int add_line_char(PeekLine *line,char c){
    char *temp;
    size_t new_cap;

    if(line->len==line->cap){
        new_cap=line->cap==0 ? 64 : line->cap*2;
        temp=realloc(line->data,new_cap);
        if(temp==NULL) return -1;
        line->data=temp;
        line->cap=new_cap;
    }
    line->data[line->len]=c;
    line->len++;
    return 0;
}

static int add_line(PeekLines *lines,PeekLine *line,int newline){
    PeekLine *temp;
    size_t new_cap;

    if(lines->count==lines->cap){
        new_cap=lines->cap==0 ? 16 : lines->cap*2;
        temp=realloc(lines->items,new_cap*sizeof(PeekLine));
        if(temp==NULL) return -1;
        lines->items=temp;
        lines->cap=new_cap;
    }
    line->newline=newline;
    lines->items[lines->count]=*line;
    lines->count++;
    line->data=NULL;
    line->len=0;
    line->cap=0;
    line->newline=0;
    return 0;
}

static void free_lines(PeekLines *lines){
    for(size_t i=0;i<lines->count;i++) free_line(&lines->items[i]);
    free(lines->items);
    lines->items=NULL;
    lines->count=0;
    lines->cap=0;
}

static void print_line(PeekLine *line,int numbered,long long number,
                       int backwards){
    if(backwards){
        for(size_t i=0;i<line->len/2;i++){
            char temp=line->data[i];
            line->data[i]=line->data[line->len-1-i];
            line->data[line->len-1-i]=temp;
        }
    }
    if(numbered && line->len>0) printf("%lld ",number);
    if(line->len>0) fwrite(line->data,1,line->len,stdout);
    if(line->newline) putchar('\n');
}

static long long count_nonempty(int fd){
    char buffer[PEEK_CHUNK];
    ssize_t bytes;
    size_t len=0;
    long long count=0;

    if(lseek(fd,0,SEEK_SET)==(off_t)-1) return 0;
    while((bytes=read(fd,buffer,sizeof(buffer)))>0){
        for(ssize_t i=0;i<bytes;i++){
            if(buffer[i]=='\n'){
                if(len>0) count++;
                len=0;
            }else{
                len++;
            }
        }
    }
    if(len>0) count++;
    return count;
}

static void print_forward(int fd,int numbered,long long *line_number){
    char buffer[PEEK_CHUNK];
    ssize_t bytes;
    PeekLine line={0};

    while((bytes=read(fd,buffer,sizeof(buffer)))>0){
        for(ssize_t i=0;i<bytes;i++){
            if(buffer[i]=='\n'){
                long long number=0;
                if(numbered && line.len>0){
                    (*line_number)++;
                    number=*line_number;
                }
                line.newline=1;
                print_line(&line,numbered,number,0);
                line.len=0;
                line.newline=0;
            }else if(add_line_char(&line,buffer[i])!=0){
                free_line(&line);
                return;
            }
        }
    }

    if(line.len>0){
        long long number=0;
        if(numbered){
            (*line_number)++;
            number=*line_number;
        }
        print_line(&line,numbered,number,0);
    }
    free_line(&line);
}

static void print_reverse_regular(int fd,int numbered,long long *line_number){
    char buffer[PEEK_CHUNK];
    PeekLine line={0};
    off_t end=lseek(fd,0,SEEK_END);
    off_t position=end;
    long long remaining=count_nonempty(fd);
    long long number=*line_number+remaining;
    int first_newline=1;

    while(position>0){
        off_t start=position>PEEK_CHUNK ? position-PEEK_CHUNK : 0;
        ssize_t size;

        if(lseek(fd,start,SEEK_SET)==(off_t)-1) break;
        size=read(fd,buffer,(size_t)(position-start));
        if(size<=0) break;

        for(ssize_t i=size-1;i>=0;i--){
            char c=buffer[i];
            position=start+i;
            if(c=='\n'){
                if(first_newline && line.len==0){
                    first_newline=0;
                    line.newline=1;
                    continue;
                }
                first_newline=0;
                line.newline=1;
                if(numbered && line.len>0){
                    print_line(&line,1,number,1);
                    number--;
                }else{
                    print_line(&line,0,0,1);
                }
                line.len=0;
                line.newline=1;
            }else if(add_line_char(&line,c)!=0){
                free_line(&line);
                return;
            }
        }
        position=start;
    }

    if(line.len>0){
        if(numbered){
            print_line(&line,1,number,1);
            number--;
        }else{
            print_line(&line,0,0,1);
        }
    }
    *line_number+=remaining;
    free_line(&line);
}

static int read_all_lines(int fd,PeekLines *lines){
    char buffer[PEEK_CHUNK];
    ssize_t bytes;
    PeekLine line={0};

    while((bytes=read(fd,buffer,sizeof(buffer)))>0){
        for(ssize_t i=0;i<bytes;i++){
            if(buffer[i]=='\n'){
                if(add_line(lines,&line,1)!=0){
                    free_line(&line);
                    return -1;
                }
            }else if(add_line_char(&line,buffer[i])!=0){
                free_line(&line);
                return -1;
            }
        }
    }
    if(line.len>0 && add_line(lines,&line,0)!=0){
        free_line(&line);
        return -1;
    }
    free_line(&line);
    return 0;
}

static void print_reverse_lines(PeekLines *lines,int numbered,
                                long long *line_number){
    long long remaining=0;
    long long number;

    for(size_t i=0;i<lines->count;i++){
        if(lines->items[i].len>0) remaining++;
    }
    number=*line_number+remaining;
    for(size_t i=lines->count;i>0;i--){
        PeekLine *line=&lines->items[i-1];
        if(numbered && line->len>0){
            print_line(line,1,number,0);
            number--;
        }else{
            print_line(line,0,0,0);
        }
    }
    *line_number+=remaining;
}

static void peek_one(const char *name,int numbered,int backwards,
                     long long *line_number){
    int fd;
    int close_fd=0;
    struct stat info;

    if(strcmp(name,"-")==0){
        fd=STDIN_FILENO;
    }else{
        fd=open(name,O_RDONLY);
        if(fd<0){
            fputs("peek: no such file or directory\n",stderr);
            return;
        }
        close_fd=1;
    }

    if(fstat(fd,&info)!=0){
        fputs("peek: no such file or directory\n",stderr);
    }else if(S_ISDIR(info.st_mode)){
        fputs("peek: is a directory\n",stderr);
    }else if(backwards && S_ISREG(info.st_mode)){
        print_reverse_regular(fd,numbered,line_number);
    }else if(backwards){
        PeekLines lines={0};
        if(read_all_lines(fd,&lines)==0){
            print_reverse_lines(&lines,numbered,line_number);
        }
        free_lines(&lines);
    }else{
        print_forward(fd,numbered,line_number);
    }

    if(close_fd) close(fd);
}

int run_peek(ShellState *state,const TokenList *tokens){
    size_t i=1;
    int numbered=0;
    int backwards=0;
    long long line_number=0;

    (void)state;
    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
       strcmp(tokens->items[0].text,"peek")!=0) return 0;

    while(i<tokens->count && tokens->items[i].type==TOKEN_WORD &&
          tokens->items[i].text[0]=='-' &&
          tokens->items[i].text[1]!='\0' &&
          strcmp(tokens->items[i].text,"-")!=0){
        for(size_t j=1;tokens->items[i].text[j]!='\0';j++){
            if(tokens->items[i].text[j]=='n') numbered=1;
            else if(tokens->items[i].text[j]=='r') backwards=1;
            else{
                fputs("peek: invalid syntax\n",stderr);
                return 1;
            }
        }
        i++;
    }

    for(size_t j=i;j<tokens->count;j++){
        if(tokens->items[j].type!=TOKEN_WORD){
            fputs("peek: invalid syntax\n",stderr);
            return 1;
        }
    }

    if(i==tokens->count){
        peek_one("-",numbered,backwards,&line_number);
    }else{
        for(;i<tokens->count;i++){
            peek_one(tokens->items[i].text,numbered,backwards,&line_number);
        }
    }
    return 1;
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

int run_locate(ShellState *state, const TokenList *tokens){
    size_t i;

    (void)state;
    if(tokens->count==0 || tokens->items[0].type!=TOKEN_WORD ||
       strcmp(tokens->items[0].text,"locate")!=0) return 0;
    if(tokens->count==1){
        fputs("locate: invalid syntax\n",stderr);
        return 1;
    }

    for(i=1;i<tokens->count;i++){
        if(tokens->items[i].type!=TOKEN_WORD) break;
        locate_one(tokens->items[i].text);
    }
    return 1;
}
