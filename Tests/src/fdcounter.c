#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char* argv[])
{
	int zz, fds;

	fds=0;

	for (zz=0; zz< FOPEN_MAX; zz++){
		if ((fcntl(zz, F_GETFD) != -1) || (errno != EBADF) ) fds++;
	}

	printf("%d file descriptors used.\n", fds);
}

int n, fds;
fds=0;
for (zz=0; zz< FOPEN_MAX; zz++){
	if ((fcntl(zz, F_GETFD) != -1) || (errno != EBADF) ) fds++;
}
printf("%d file descriptors OPEN.\n", fds);
