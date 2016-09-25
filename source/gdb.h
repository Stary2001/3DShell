typedef struct gdb_ctx
{
	unsigned int tid;
} gdb_ctx;

int gdb_do_packet(int fd, struct client_ctx *ctx);