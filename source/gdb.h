typedef struct gdb_ctx
{
	unsigned int tid;
	shell_ctx shell;
} gdb_ctx;

int gdb_do_packet(int fd, struct client_ctx *ctx);
int gdb_shell_out(int fd, void *ctx, const char *s);