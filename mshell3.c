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

void execute(char buf[], int wskMain);

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
                exit(EXEC_FAILURE);
            }
        }
    }
}

void execute(char buf[], int wskMain)
{
    line * ln;
    command *c;
    int k;
    ln = parseline(buf+wskMain);
    c = pickfirstcommand(ln);
    if(c!=NULL)
    {
        k = fork();
        if (k == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if(k)
        {
            waitpid(k, NULL, 0);
        }
        else
        {
            if(execvp(c->argv[0], c->argv) < 0);
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
