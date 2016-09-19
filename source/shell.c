#include <3ds.h>
#include <scenic/proc.h>
#include <scenic/kernel/kproc.h>
#include <scenic/kernel/kmem.h>
#include "shell.h"

#define checked_recv(fd, v, l) if((r=read(fd, v, l, 0)) == -1) { return -1; }
#define VER 0

int process_cmd(int fd, struct client_ctx *ctx)
{
	int r = 0;

	printf("trying to recv %i bytes\n", SCRATCH_LENGTH - ctx->scratch_len); 
	r = recv(fd, ctx->scratch + ctx->scratch_len, SCRATCH_LENGTH - ctx->scratch_len, 0);
	if(r == 0)
	{
		return -1;
	}
	if(r == -1)
	{
		return -1;
	}

	printf("got %i bytes\n", r);

	ctx->scratch_len += r;

	char *line_end = NULL;
	bool found = false;
	uint32_t len = 0;
	char *last_line_end = NULL;

	while(true)
	{
		last_line_end = line_end;
		line_end = (char*)memmem(ctx->scratch + ctx->scratch_off, SCRATCH_LENGTH - ctx->scratch_off, "\n", 1);
		if(line_end == NULL)
		{
			if(found)
			{
				len = ctx->scratch_len - (uint32_t)(last_line_end+1 - ctx->scratch);
				memmove(ctx->scratch, last_line_end + 1, len);
				memset(ctx->scratch + len, 0, SCRATCH_LENGTH - len);
				ctx->scratch_off = 0;
				ctx->scratch_len = len;
			}
			else
			{
				// oh no. 
				printf("more bytes are available\n");
				return -1; // todo: fix by growing buffer / doing memmem again.
			}
			break;
		}

		found = true;
		len = (uint32_t)(line_end - ctx->scratch) - ctx->scratch_off;

		*(ctx->scratch + ctx->scratch_off + len) = 0;
		printf("found line %s %i long\n", ctx->scratch + ctx->scratch_off, len);
		*(ctx->scratch + ctx->scratch_off + len) = '\n';

		len += 1;
		ctx->scratch_off += len;
	}

	return 0;
}