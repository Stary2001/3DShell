#pragma once
#include "client.h"

typedef struct server_ctx
{
	int serv_fd;
	struct pollfd *fds;
	struct client_ctx **client_ctxs;
	int nfds;
} server_ctx;

int read_until(int fd, char *buf, size_t buf_len, char *sig, size_t sig_len);

struct server_ctx *server_bind(u32 host, u16 port);
typedef int (*poll_cback)(int fd, struct client_ctx *ctx);
int server_poll(struct server_ctx *serv, int timeout, poll_cback accept_cb, poll_cback data_cb);
int server_close(struct server_ctx *serv, int i);
void server_destroy(struct server_ctx *serv);