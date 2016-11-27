#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "builtins.h"
#include <dirent.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

int echo(char *argv[]);
int undefined(char *argv[]);
int lexit(char *argv[]);
int cd(char *argv[]);
int lls(char *argv[]);
int lkill(char *argv[]);

builtin_pair builtins_table[]={
	{"exit", &lexit},
	{"lecho",	&echo},
	{"lcd",	&cd},
	{"lkill",	&lkill},
	{"lls",	&lls},
	{NULL,NULL}
};

int
echo( char * argv[])
{
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int
lexit(char * argv[])
{
	if(argv[1]!=NULL)
	   return BUILTIN_ERROR;
  exit(0);
}

int
cd(char *argv[])
{
    char *value;
		if(argv[1]==NULL)
    {
      value = getenv("HOME");
			if(value==NULL)
			  return BUILTIN_ERROR;
    }
		else
		{
	      if (argv[2]!=NULL) return BUILTIN_ERROR;
        value = argv[1];
    }

    if(chdir(value) < 0)
	  {
			return BUILTIN_ERROR;
		}
    return 0;
}

int
lkill(char *argv[])
{
	  if(argv[1]==NULL)
		  return BUILTIN_ERROR;
	  if(argv[3]!=NULL)
		  return BUILTIN_ERROR;
	  if(argv[2]==NULL)
		{
			pid_t pid = atoi(argv[1]);
			if(pid==0)
			  return BUILTIN_ERROR;
			else
		  {
				if(argv[2]=="0")
					return BUILTIN_ERROR;
        else
				{
					if(kill(pid, SIGTERM)<0)
					{
						return BUILTIN_ERROR;
					}
				}
			}
		}
		else
		{
			int x = strlen(argv[1]);
			char signal[x];
			strcpy(signal, argv[1]+1);
			pid_t pid = atoi(argv[2]);
			int sig = atoi(signal);
			if(sig==0)
				return BUILTIN_ERROR;
      else
		  {
				if(pid==0)
					return BUILTIN_ERROR;
				else
			  {
					if(argv[2]=="0")
					  return BUILTIN_ERROR;
	        else
					{
						if(kill(pid, sig)<0)
						{
							return BUILTIN_ERROR;
						}
					}
				}
			}
		}
		return 0;
}

int
lls(char *argv[])
{
  DIR *dir;
  struct dirent *dp;
  if(argv[1]!=NULL)
	  return BUILTIN_ERROR;
	dir = opendir(".");
	if(dir==NULL) return BUILTIN_ERROR;
	while((dp=readdir(dir)) != NULL)
	{
     if(dp->d_name[0]=='.')
		   continue;
		 printf("%s\n", dp->d_name);
	}
	if(closedir(dir) < 0)
	  return BUILTIN_ERROR;
	fflush(stdout);
	return 0;
}

int
undefined(char * argv[])
{
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}
