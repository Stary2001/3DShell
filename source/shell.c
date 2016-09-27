#include <3ds.h>
#include <stdarg.h>
#include <scenic/proc.h>
#include <scenic/kernel/kproc.h>
#include <scenic/kernel/kmem.h>
#include "shell.h"
#include "sock_util.h"

int shell_printf(int fd, struct shell_ctx *ctx, const char *line, ...)
{
	char printf_buffer[1024];
	va_list a;
	va_start(a, line);
	vsnprintf(printf_buffer, 1024, line, a);
	ctx->out(fd, ctx, printf_buffer);
	va_end(a);
}

int process_line(int fd, struct shell_ctx *ctx, const char *line)
{
	shell_printf(fd, ctx, "got line '%s'\n", line);
	return 0;
}

int process_cmd(int fd, struct client_ctx *ctx)
{
	char line[256];
	int r = read_until(fd, line, 256, "\n", 1);
	if(r == 0 || r == -1)
	{
		return -1;
	}

	line[r-1] = 0; // remove \n
	return process_line(fd, (struct shell_ctx*)ctx->data, line);
}

int sock_shell_out(int fd, void *ctx, const char *s)
{
	return send(fd, s, strlen(s), 0);
}