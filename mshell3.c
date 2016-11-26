#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "config.h"
#include "siparse.h"
#include "utils.h"
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "builtins.h"
#include <signal.h>

void execute(char buf[], int wskMain);
void comand(command *c, int fd[2], int fd2[2]);
void execCommand(command *c);
int builtin_command(command *c);

volatile int counter;
void handler(int sigNb)
{
  pid_t child;
  do{
    child = waitpid(-1, NULL, WNOHANG);
    if (child > 0) counter--;
  } while(child > 0);
}

int
main(int argc, char *argv[])
{
    line * ln;
    command *c;
    int n, k, status, i, wskMain, j, m, wskBuf;
    char buf[MAX_LINE_LENGTH+5];
    struct stat p;
    status = fstat(0, &p);
    wskMain = 0;
    wskBuf = 0;
    while(1)
    {
        if(S_ISCHR(p.st_mode))
            write(1, PROMPT_STR, 2);
        n = read(0, buf+wskMain, MAX_LINE_LENGTH-wskMain);
        buf[n+wskMain] = 0;
        if(n==0)
        {
            for(i=wskBuf; i<n+wskMain; i++)
            {
                if(buf[i]=='\n')
                {
                    buf[i] = 0;
                    execute(buf, wskBuf);
                    wskBuf = i+1;
                }
            }
            execute(buf, wskBuf);
            break;
        }
        else
        {
            for(i=wskBuf; i<n+wskMain; i++)
            {
                if(buf[i]=='\n')
                {
                    buf[i] = 0;
                    execute(buf, wskBuf);
                    wskBuf = i+1;
                }
            }
            if(wskBuf!=0 && i==MAX_LINE_LENGTH)
            {
                for(i=wskBuf, j=0; i<MAX_LINE_LENGTH; i++, j++)
                    buf[j] = buf[i];
                wskBuf = 0;
                wskMain = j;
            }
            else if(i < MAX_LINE_LENGTH)
            {
                wskMain = i;
            }
            else if(wskBuf==0 && i==MAX_LINE_LENGTH)
            {
                wskMain = 0;
                wskBuf = 0;
                printf("%s\n", SYNTAX_ERROR_STR);
            }
        }
    }
}

void execute(char buf[], int wskMain)
{
    int frk;
    int iterPipe=0;
    int fd[2] = {-1, -1}, fd2[2] = {-1, -1};
    line * ln;
    ln = parseline(buf+wskMain);
    volatile int counter;
    while((ln->pipelines[iterPipe])!=NULL)
    {
        if(ln->pipelines[iterPipe][0]==NULL)
        {
          iterPipe++;
          continue;
        }
        if(ln->pipelines[iterPipe][1]==NULL)
        {
          int builtin = builtin_command(ln->pipelines[iterPipe][0]);
          if(builtin)
          {
            ++iterPipe;
            continue;
          }
          else
          {
            execCommand(ln->pipelines[iterPipe][0]);
            ++iterPipe;
            continue;
          }
        }

        if(pipe(fd2)<0)
        {
          exit(EXEC_FAILURE);
        }

        int iPipe=0; command* c;
        int counter = 0;
        for(c = ln->pipelines[iterPipe][iPipe]; c!=NULL; ++iPipe, c = ln->pipelines[iterPipe][iPipe])
        {
            // tu robie forka
            frk = fork();
            if(!frk)
            {
              command(ln->pipelines[iterPipe][iPipe]);
            }
            else
            {
              counter++;
            }
            //comand(c, fd, fd2);
            if(fd[0] != -1)
              close(fd[0]);
            if(fd2[1] != -1)
              close(fd2[1]);
            fd[0] = fd2[1] = -1;
            if(ln->pipelines[iterPipe][iPipe+2]==NULL)
            {
              fd[0] = fd2[0];
              fd[1] = fd2[1];
              if(fd2[1] != -1)
                close(fd2[1]);
              fd2[0] = fd2[1] = -1;
              fd[1] = -1;
            }
            else
            {
              fd[0] = fd2[0];
              fd[1] = fd2[1];
              pipe(fd2);
            }
        }
        sigset_t mask;
        struct sigaction sa;
        sa.sa_handler = handler;
        sa.flags = 0;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGCHLD, &sa, NULL);

        sigemptyset (&mask);
        sigaddset (&mask, SIGCHLD);
        sigprocmask (SIG_BLOCK, &mask, NULL);
        while (counter>0)
          sigsuspend (&mask);
        sigprocmask (SIG_UNBLOCK, &mask, NULL);

        iterPipe++;
    }
}

void command(command *c)
{
  if(fd[1] != -1)
  {
    close(fd[1]);
    fd[1] = -1;
  }
  if(fd2[0] != -1){
    close(fd2[0]);
    fd2[0] = -1;
  }
  if(fd[0] != -1)
  {
    dup2(fd[0], 0);
    close(fd[0]);
    fd[0] = -1;
  }
  if(fd2[1] != -1)
  {
    dup2(fd2[1], 1);
    close(fd2[1]);
    fd2[1] = -1;
  }
  //exit(1);
  execCommand(c);
}

int builtin_command(command *c)
{
  int temp = 0;
  int result = 0;
  while(builtins_table[temp].name!=NULL)
  {
      if(c->argv[0]==NULL) break;
      if(strcmp(c->argv[0], builtins_table[temp].name)==0)
      {
          result = 1;
          int val = builtins_table[temp].fun(c->argv);
          if(val==BUILTIN_ERROR)
          {
              fprintf(stderr,"Builtin %s error.\n", c->argv[0]);
          }
          break;
      }
      temp++;
  }
  return result; // c byl builtinsem
}

void execCommand(command *c)
{
  int k, i;
  if(c!=NULL)
  {
      k = fork();
      if (k == -1)
      {
          exit(EXIT_FAILURE);
      }
      if(k)
      {
          waitpid(k, NULL, 0);
      }
      else
      {
          int iter=0;
          while(c->redirs[iter]!=NULL)
          {
              if(IS_RIN(c->redirs[iter]->flags))
              {
                  int fd;
                  close(0);
                  if(fd = open(c->redirs[iter]->filename, O_RDONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)<0)
                  {
                      if(errno==ENOENT)
                      {
                          fprintf(stderr, "(%s): no such file or directory\n", c->redirs[iter]->filename);
                          exit(EXEC_FAILURE);
                      }
                      else if(errno==EACCES)
                      {
                          fprintf(stderr, "(%s):  permission denied\n", c->redirs[iter]->filename);
                          exit(EXEC_FAILURE);
                      }
                      else
                      {
                          exit(EXEC_FAILURE);
                      }
                  }
                  if(fd>0)
                  {
                      exit(EXEC_FAILURE);
                  }
              }
              if(IS_ROUT(c->redirs[iter]->flags) || IS_RAPPEND(c->redirs[iter]->flags))
              {
                  int fd;
                  close(1);
                  if(IS_RAPPEND(c->redirs[iter]->flags))
                  {
                      if(fd = open(c->redirs[iter]->filename, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0)
                      {
                          if(errno==ENOENT)
                          {
                              fprintf(stderr, "(%s): no such file or directory\n", c->redirs[iter]->filename);
                              exit(EXEC_FAILURE);
                          }
                          else if(errno==EACCES)
                          {
                              fprintf(stderr, "(%s):  permission denied\n", c->redirs[iter]->filename);
                              exit(EXEC_FAILURE);
                          }
                          else
                          {
                              exit(EXEC_FAILURE);
                          }
                      }
                  }
                  else
                  {
                      if(fd = open(c->redirs[iter]->filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0)
                      {
                          if(errno==ENOENT)
                          {
                              fprintf(stderr, "(%s): no such file or directory\n", c->redirs[iter]->filename);
                              exit(EXEC_FAILURE);
                          }
                          else if(errno==EACCES)
                          {
                              fprintf(stderr, "(%s):  permission denied\n", c->redirs[iter]->filename);
                              exit(EXEC_FAILURE);
                          }
                          else
                          {
                              exit(EXEC_FAILURE);
                          }
                      }
                  }
                  if(fd>1)
                  {
                      exit(EXEC_FAILURE);
                  }
              }
              iter++;
          }
          if(execvp(c->argv[0], c->argv) < 0)
          {
              int err = errno;
              if(errno==ENOENT)
              {
                  fprintf(stderr, "[%s]: no such file or directory\n", c->argv[0]);
                  exit(EXEC_FAILURE);
              }
              else if(errno==EACCES)
              {
                  fprintf(stderr, "[%s]: permission denied\n", c->argv[0]);
                  exit(EXEC_FAILURE);
              }
              else
              {
                  fprintf(stderr, "[%s]: exec error\n", c->argv[0]);
                  exit(EXEC_FAILURE);
              }
          }
          exit(1);
      }
  }
  else
  {
      printf("%s\n", SYNTAX_ERROR_STR);
  }
}
