#include <3ds.h>
#include "sock_util.h"

#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

#define MAX_CONN 8
#define PEEK_SIZE 512
char peek_buffer[512];

int read_until(int fd, char *buf, size_t buf_len, char *sig, size_t sig_len)
{
	int r = recv(fd, peek_buffer, 512, MSG_PEEK);
	if(r == 0 || r == -1)
	{
		return r;
	}

	char *ptr = (char*) memmem(peek_buffer, PEEK_SIZE, sig, sig_len);
	if(ptr == NULL)
	{
		return -1; // Line too long!
	}
	size_t len = ptr - peek_buffer;
	len += sig_len;

	if(len > buf_len)
	{
		return -1;
	}
	memset(peek_buffer, 0, len);

	r = recv(fd, buf, len, 0);
	return r;
}

struct server_ctx *server_bind(u32 host, u16 port)
{
	struct sockaddr_in serv_addr;
	struct server_ctx *s = malloc(sizeof(struct server_ctx));

  	s->serv_fd = socket(AF_INET, SOCK_STREAM, 0);
  	serv_addr.sin_family = AF_INET;
  	serv_addr.sin_addr.s_addr = host;
  	serv_addr.sin_port = htons(port);

  	int one = 1;
    int r = setsockopt(s->serv_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  	r = bind(s->serv_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  	if(r == -1)
  	{
  		close(s->serv_fd);
  		free(s);
  		return NULL;
  	}

  	r = listen(s->serv_fd, 5);
  	if(r == -1)
  	{
  		free(s);
  		return NULL;
  	}

  	s->nfds = 1;
  	s->fds = malloc(sizeof(struct pollfd) * MAX_CONN);
  	s->client_ctxs = malloc(4 * MAX_CONN);
  	memset(s->fds, 0, sizeof(struct pollfd) * MAX_CONN);

  	int i;
  	for(i = 0; i < MAX_CONN; i++)
  	{
  		s->fds[i].fd = -1;
  	}

	s->client_ctxs[0] = NULL;
	s->fds[0].fd = s->serv_fd;
	s->fds[0].events = POLLIN;

  	return s;
}

int server_poll(struct server_ctx *serv, int timeout, poll_cback accept_cb, poll_cback data_cb)
{
	int r = poll(serv->fds, serv->nfds, timeout);

	for(int i = 0; i < serv->nfds; i++)
	{
		if(serv->fds[i].revents & POLLIN)
		{
			if(serv->fds[i].fd < 0) continue;

			if(serv->fds[i].fd == serv->serv_fd)
			{
				struct sockaddr_in addr;
				socklen_t addrlen = sizeof(addr);
				r = accept(serv->serv_fd, (struct sockaddr*)&addr, &addrlen);
				if(r != -1)
				{
					const char *sorry = "max conns reached\n";
					if(serv->nfds == MAX_CONN)
					{
						send(r, sorry, strlen(sorry), 0);
						close(r);
					}
					else
					{
						serv->client_ctxs[serv->nfds] = malloc(sizeof(struct client_ctx));
						memset(serv->client_ctxs[serv->nfds], 0, sizeof(struct client_ctx));

						serv->fds[serv->nfds].fd = r;
						serv->fds[serv->nfds].events = POLLIN;
						serv->fds[serv->nfds].revents = 0;
						serv->nfds++;

						printf("accepted connection from %s:%u\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

						if(accept_cb(serv->fds[serv->nfds-1].fd, serv->client_ctxs[serv->nfds-1]) == -1)
						{
							server_close(serv, serv->nfds-1);
							break;
						}
					}
				}
			}
			else
			{
				if(data_cb(serv->fds[i].fd, serv->client_ctxs[i]) == -1)
				{
					server_close(serv, i);
					break;
				}
			}
		}
		else if(serv->fds[i].revents & POLLERR || serv->fds[i].revents & POLLHUP)
		{
			server_close(serv, i);
			if(i == 0) return -1;
			break;
		}
	}

	return 0;
}

void compact(struct pollfd *fds, struct client_ctx **ctx, int *nfds)
{
	int new_fds[MAX_CONN];
	struct client_ctx *new_ctx[MAX_CONN];

	int n = 0;

	for(int i = 0; i < *nfds; i++)
	{
		if(fds[i].fd != -1)
		{
			new_fds[n] = fds[i].fd;
			new_ctx[n] = ctx[i];
			n++;
		}
	}

	for(int i = 0; i < n; i++)
	{
		fds[i].fd = new_fds[i];
		ctx[i] = new_ctx[i];
	}
	*nfds = n;
}

int server_close(struct server_ctx *serv, int i)
{
	printf("closing %i slot %i\n", serv->fds[i].fd, i);

	if(serv->client_ctxs[i] != NULL)
	{
		free(serv->client_ctxs[i]);
		serv->client_ctxs[i] = NULL;
	}

	close(serv->fds[i].fd);
	serv->fds[i].fd = -1;
	compact(serv->fds, serv->client_ctxs, &serv->nfds);
}

void server_destroy(struct server_ctx *serv)
{
	for(int i = 0; i < serv->nfds; i++)
	{
		if(serv->fds[i].fd != -1)
		{
			close(serv->fds[i].fd);
		}
		
		if(serv->client_ctxs[i] != NULL)
		{
			free(serv->client_ctxs[i]);
		}
	}

	free(serv->fds);
	free(serv->client_ctxs);
	free(serv);
}