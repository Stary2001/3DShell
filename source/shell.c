#include <3ds.h>
#include <scenic/proc.h>
#include <scenic/kernel/kproc.h>
#include <scenic/kernel/kmem.h>
#include "shell.h"
#include "sock_util.h"

#define checked_recv(fd, v, l) if((r=read(fd, v, l, 0)) == -1) { return -1; }
#define VER 0

int process_cmd(int fd, struct client_ctx *ctx)
{
	char line[256];
	int r = read_until(fd, line, 256, "\n", 1);
	if(r == 0 || r == -1)
	{
		return -1;
	}

	printf("found line %i long\n", r);

	return 0;
}