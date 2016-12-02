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

void checkPipelines(char buf[], int wskMain);
void controlDescriptors(command *c, int fd[2], int fd2[2]);
void checkErrnoRedirs(int err, command *c, int iter);
void checkErrnoCommand(int err, command *c);
void redirections(command *c);
void execCommand(command *c);
int builtin_command(command *c);
volatile int counter;
void handler(int sigNb);

int
main(int argc, char *argv[])
{
    line * ln;
    command *c;
    int n, k, status, i, wskMain, j, m, wskBuf, testLen;
    char buf[MAX_LINE_LENGTH+5];
    int foreground[2048]; // procesy w foregroundzie
    struct stat p;
    status = fstat(0, &p);
    wskMain = 0;
    wskBuf = 0;
    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    testLen = 0;
    while(1)
    {
        if(S_ISCHR(p.st_mode))
            write(1, PROMPT_STR, 2);
        n = read(0, buf+wskMain, MAX_LINE_LENGTH-wskMain);
        buf[n+wskMain] = 0;
        int pom;
        if(testLen == 1)
        {
          pom = wskBuf;
          while(pom<n+wskMain)
          {
            if(buf[pom]=='\n')
            {
              testLen=0;
              break;
            }
            pom++;
          }
          if(testLen==0)
          {
            wskBuf = pom;
          }
          else
          {
            wskBuf = 0;
            wskMain = 0;
            continue;
          }
        }
        if(n==0)
        {
            for(i=wskBuf; i<n+wskMain; i++)
            {
                if(buf[i]=='\n')
                {
                    buf[i] = 0;
                    checkPipelines(buf, wskBuf);
                    wskBuf = i+1;
                }
            }
            checkPipelines(buf, wskBuf);
            break;
        }
        else
        {
            for(i=wskBuf; i<n+wskMain; i++)
            {
                if(buf[i]=='\n')
                {
                    buf[i] = 0;
                    checkPipelines(buf, wskBuf);
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
                testLen = 1;
                wskMain = 0;
                wskBuf = 0;
                fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
            }
        }
    }
}

void handler(int sigNb)
{
    pid_t child;
    do
    {
        child = waitpid(-1, NULL, WNOHANG);
        if (child > 0) counter--;
    }
    while(child > 0);
}
void checkPipelines(char buf[], int wskMain)
{
    int frk;
    int iterPipe=0;
    int fd[2] = {-1, -1}, fd2[2] = {-1, -1};
    line * ln;
    ln = parseline(buf+wskMain);
    /*if(ln.flags==LINBACKGROUND)
    {

    }
    else
    {

    }*/
    while((ln->pipelines[iterPipe])!=NULL)
    {
        counter = 0;
        sigset_t mask;
        sigemptyset (&mask);
        sigaddset (&mask, SIGCHLD);
        sigprocmask (SIG_BLOCK, &mask, NULL);

        if(ln->pipelines[iterPipe][0]==NULL)
        {
            iterPipe++;
            continue;
        }
        if(ln->pipelines[iterPipe][1]==NULL)
        {
            int builtin = builtin_command(ln->pipelines[iterPipe][0]);
            if(!builtin)
            {
                frk = fork();
                if(!frk)
                {
                    execCommand(ln->pipelines[iterPipe][0]);
                    //exit(1);
                }
                else
                    counter++;

                sigemptyset (&mask);
                while (counter>0)
                {
                    sigsuspend (&mask);
                }
            }
            ++iterPipe;
            continue;
        }
        if(pipe(fd2)<0)
        {
            exit(EXEC_FAILURE);
        }
        int iPipe=0;
        command* c;
        for(c = ln->pipelines[iterPipe][iPipe]; c!=NULL; ++iPipe, c = ln->pipelines[iterPipe][iPipe])
        {
            frk = fork();
            if(!frk)
            {
                controlDescriptors(c, fd, fd2);
                //exit(1);
            }
            else
                counter++;

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
        if(fd[0] != -1)
            close(fd[0]);
        if(fd2[1] != -1)
            close(fd2[1]);
        if(fd[1] != -1)
            close(fd[1]);
        if(fd2[0] != -1)
            close(fd2[0]);

        sigemptyset (&mask);
        while (counter>0)
            sigsuspend (&mask);
        //sigprocmask (SIG_UNBLOCK, &mask, NULL);
        iterPipe++;
    }
}
void controlDescriptors(command *c, int* fd, int* fd2)
{
    if(fd[1] != -1)
    {
        close(fd[1]);
        fd[1] = -1;
    }
    if(fd2[0] != -1)
    {
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
    //fprintf(stdout, "%s %d %d %d %d \n", c->argv[0], fd2[0], fd2[1], fd[0], fd[1]);
    execCommand(c);
}
int builtin_command(command *c)
{
    int temp = 0, result = 0;
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
    if(c == NULL || c->argv == NULL || c->argv[0] == NULL)
      exit(1);
    int k, i;
    if(c!=NULL)
    {
        redirections(c);
        if(execvp(c->argv[0], c->argv) < 0)
        {
            int err = errno;
            checkErrnoCommand(err, c);
        }
        exit(1);
    }
    else
    {
        fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
    }
}
void redirections(command *c)
{
    int iter=0, err;
    while(c->redirs[iter]!=NULL)
    {
        int fdRedir;
        if(IS_RIN(c->redirs[iter]->flags))
        {
            close(0);
            if(fdRedir = open(c->redirs[iter]->filename, O_RDONLY)<0)
            {
                err = errno;
                checkErrnoRedirs(err, c, iter);
            }
            if(fdRedir>0)
            {
                exit(EXEC_FAILURE);
            }
        }
        if(IS_ROUT(c->redirs[iter]->flags) || IS_RAPPEND(c->redirs[iter]->flags))
        {
            close(1);
            if(IS_RAPPEND(c->redirs[iter]->flags))
            {
                if(fdRedir = open(c->redirs[iter]->filename, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0)
                {
                    err = errno;
                    checkErrnoRedirs(err, c, iter);
                }
            }
            else
            {
                if(fdRedir = open(c->redirs[iter]->filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0)
                {
                    err = errno;
                    checkErrnoRedirs(err, c, iter);
                }
            }
            if(fdRedir>1)
            {
                exit(EXEC_FAILURE);
            }
        }

        iter++;
    }
}
void checkErrnoRedirs(int err, command *c, int iter)
{
    if(err==ENOENT)
        fprintf(stderr, "%s: no such file or directory\n", c->redirs[iter]->filename);
    else if(err==EACCES)
        fprintf(stderr, "%s: permission denied\n", c->redirs[iter]->filename);

    exit(EXEC_FAILURE);
}
void checkErrnoCommand(int err, command *c)
{
    if(err==ENOENT)
        fprintf(stderr, "%s: no such file or directory\n", c->argv[0]);
    else if(err==EACCES)
        fprintf(stderr, "%s: permission denied\n", c->argv[0]);
    else
        fprintf(stderr, "%s: exec error\n", c->argv[0]);

    exit(EXEC_FAILURE);
}
