#pragma once
#include <scenic/proc.h>

#define STOP_UNDEF 0
#define STOP_ATTACH 1
#define STOP_BREAK 2

#define MAX_PROC 16

typedef struct gdb_proc_ctx
{
	scenic_process *p;
	u32 pid;
	u32 tid;

	u16 stop_reason;
	u16 stop_status;
	int _curr_thread_idx;

} gdb_proc_ctx;

typedef struct proc_list_node
{
	gdb_proc_ctx *c;
	struct proc_list *next;
} proc_list_node;

typedef struct proc_list
{
	proc_list_node *head;
	proc_list_node *tail;
} proc_list;

typedef struct gdb_ctx
{
	shell_ctx shell;
	proc_list procs;

	gdb_proc_ctx *curr_proc;
	u8 ack;
} gdb_ctx;


int gdb_do_packet(int fd, struct client_ctx *ctx);
int gdb_shell_out(int fd, void *ctx, const char *s, int len);

void gdb_add_proc(gdb_ctx *ctx, gdb_proc_ctx *p_ctx, u32 pid);
void gdb_del_proc(gdb_ctx *ctx, u32 pid);
gdb_proc_ctx *gdb_get_proc(gdb_ctx *ctx, u32 pid);
scenic_thread *gdb_get_thread(gdb_ctx *ctx, u32 pid, u32 tid);