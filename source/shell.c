#include <3ds.h>
#include <stdarg.h>
#include <scenic/proc.h>
#include <scenic/kernel/kproc.h>
#include <scenic/kernel/kmem.h>
#include <scenic/debug.h>
#include <scenic/custom_svc.h>
#include "shell.h"
#include "sock_util.h"

#define ARG_MAX 32

int ps_command(int fd, struct shell_ctx *ctx, int argc, char **argv)
{
	u32 pids[64];
	u32 n_pids;

	Result r = fixed_svcGetProcessList(&n_pids, pids, 64);
	if(r < 0)
	{
		return -1;
	}

	shell_printf(fd, ctx, "got %i pids\n", n_pids);

	char name[9];
	name[8] = 0;

	for(int i = 0; i < n_pids; i++) // skip first pid. it's zero. duh.
	{
		scenic_kproc *p = kproc_find_by_id(pids[i]);
		shell_printf(fd, ctx, "%02x - ", pids[i]);
		kproc_get_name(p, name);
		shell_printf(fd, ctx, "%s\n", name);
		kproc_close(p);
	}

	return 0;
}

int exit_command(int fd, struct shell_ctx *ctx, int argc, char **argv)
{
	return -1;
}

#define NUM_CMD 2
struct shell_cmd cmd_list[] = {
	{"ps", ps_command},
	{"exit", exit_command},
};

char printf_buffer[1024];
int shell_printf(int fd, struct shell_ctx *ctx, const char *line, ...)
{
	va_list a;
	va_start(a, line);
	int l = vsnprintf(printf_buffer, 1024, line, a);
	if(l > 0)
	{
		ctx->out(fd, ctx, printf_buffer, l);
	}
	va_end(a);
}

int process_line(int fd, struct shell_ctx *ctx, const char *line)
{
	int argc = 0;
	char *argv[ARG_MAX];
	char *cmd = strtok(line, " ");
	char *a = strtok(NULL, " ");

	while(a != NULL)
	{
		shell_printf(fd, ctx, "arg %x\n", a);
		argv[argc++] = a;

		a = strtok(NULL, " ");
	}

	shell_printf(fd, ctx, "got command '%s' with %i args\n", cmd, argc);
	for(int i = 0; i < NUM_CMD; i++)
	{
		shell_printf(fd, ctx, "got %i %x\n", i, cmd_list[i].name);
		if(strcmp(cmd_list[i].name, cmd) == 0)
		{
			return cmd_list[i].func(fd, ctx, argc, argv);
		}
	}

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

int sock_shell_out(int fd, void *ctx, const char *s, int len)
{
	return send(fd, s, len, 0);
}