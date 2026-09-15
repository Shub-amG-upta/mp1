#ifndef D1D2_H
#define D1D2_H

#include "shell.h"
#include "lexer.h"
#include <sys/types.h>

int run(const char *line,ShellState *state);
void buff();

int getjobcount(void);
int getjobinfo(int index,pid_t *pid,char *name,int *live);

int getshellterminal(void);
void giveterminal(pid_t pgid);

int is_background_child(void);
void set_background_child(int value);
int add_stopped_job(pid_t pgid,const TokenList *tokens);
int has_stopped_jobs(void);
void print_stopped(int number);
int take_interrupt(void);

void shutdown_jobs(void);

int runcomm(ShellState *state,const TokenList *tokens);
int is_builtin(const char *name);
void reset_child_mask(void);

void start_job_timer(long seconds);
void stop_job_timer(void);
int job_timer_fired(void);

int find_job(int number,pid_t *pgid);
void set_job_stopped(int index,int value);
void finish_job(int index);
const char *job_command_text(int index);

#endif