#pragma once
#include <scenic/proc.h>

#define STOP_UNDEF 0
#define STOP_ATTACH 1
#define STOP_BREAK 2

typedef struct gdb_ctx
{
	unsigned int tid;
	shell_ctx shell;

	scenic_process *proc;
	u32 pid;

	u16 stop_reason;
	u16 stop_status;
	u8 ack;

	int _curr_thread_idx;
} gdb_ctx;

int gdb_do_packet(int fd, struct client_ctx *ctx);
int gdb_shell_out(int fd, void *ctx, const char *s);