#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include "shell.h"

#define MAX_CONN 8

void compact(struct pollfd *fds, struct client_ctx **ctx, int *nfds)
{
	int new_fds[MAX_CONN];
	struct client_ctx *new_ctx[MAX_CONN];

	int n = 0;

	for(int i = 0; i < *nfds; i++)
	{
		if(fds[i].fd != -1)
		{
			new_fds[n++] = fds[i].fd;
			new_ctx[n] = ctx[i];
		}
	}

	for(int i = 0; i < n; i++)
	{
		fds[i].fd = new_fds[i];
		ctx[i] = new_ctx[i];
	}
	*nfds = n;
}

int running = 1;
void sock_thread(void *arg)
{
	printf("hello\n");
	acInit();

	int soc_sz = 0x100000;
	void *soc_buff = (u32*)memalign(0x1000, soc_sz);
  	if(soc_buff == NULL)
  	{
  		acExit();
  		svcExitThread();
  	}

	socInit(soc_buff, soc_sz);

	struct sockaddr_in serv_addr;
  	int r = 0;

  	int serv_fd = socket(AF_INET, SOCK_STREAM, 0);
  	serv_addr.sin_family = AF_INET;
  	serv_addr.sin_addr.s_addr = (in_addr_t)gethostid();
  	serv_addr.sin_port = htons(1337);

  	int one = 1;
    r = setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
    int bufSize = 1024 * 32;
    setsockopt(serv_fd, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

  	r = bind(serv_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  	if(r == -1)
  	{
  		close(serv_fd);
  		acExit();
  		socExit();

  		printf("bind failed!\n");
  		free(soc_buff);
  		svcExitThread();
  	}

  	r = listen(serv_fd, 5);
  	if(r == -1)
  	{
  		close(serv_fd);
  		acExit();
  		socExit();
  		free(soc_buff);
  		printf("listen failed!\n");
  		svcExitThread();
  	}

  	struct pollfd *fds = malloc(sizeof(struct pollfd) * MAX_CONN);
  	struct client_ctx **ctx = malloc(4 * MAX_CONN);

  	memset(fds, 0, sizeof(struct pollfd) * MAX_CONN);

  	int i;
  	for(i = 0; i < MAX_CONN; i++)
  	{
  		fds[i].fd = -1;
  	}

  	int nfds = 1;

  	fds[0].fd = serv_fd;
  	fds[0].events = POLLIN;

	while(running)
	{
		printf("polling %i fds..\n", nfds);

		r = poll(fds, nfds, -1);

		for(i = 0; i < nfds; i++)
		{
			if(fds[i].fd < 0) continue;

			if(fds[i].revents & POLLIN)
			{
				if(fds[i].fd == serv_fd)
				{
					struct sockaddr_in addr;
					socklen_t addrlen = sizeof(addr);
					r = accept(serv_fd, (struct sockaddr*)&addr, &addrlen);
					if(r != -1)
					{
						const char *sorry = "max conns reached\n";
						if(nfds == MAX_CONN)
						{
							send(r, sorry, strlen(sorry), 0);
							close(r);
						}
						else
						{
							ctx[nfds] = malloc(sizeof(struct client_ctx));
							memset(ctx[nfds], 0, sizeof(struct client_ctx));

							fds[nfds].fd = r;
							fds[nfds].events = POLLIN;
							fds[nfds].revents = 0;
							nfds++;

							printf("accepted connection from %s:%u\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
							printf("len: %i\n", ctx[nfds-1]->scratch_len);
						}
					}
				}
				else
				{
					r = process_cmd(fds[i].fd, ctx[i]);
					if(r == 1)
					{
						printf("stopping...\n");
						running = 0;
					}
					else if(r == -1)
					{
						printf("closing %i slot %i\n", fds[i].fd, i);
						free(ctx[i]);
						ctx[i] = NULL;

						close(fds[i].fd);
						fds[i].fd = -1;
						compact(fds, ctx, &nfds);
					}
				}
			}
			else if(fds[i].revents & POLLERR || fds[i].revents & POLLHUP)
			{
				printf("closing %i slot %i\n", fds[i].fd, i);
				
				free(ctx[i]);
				ctx[i] = NULL;
				close(fds[i].fd);
				fds[i].fd = -1;
				compact(fds, ctx, &nfds);

				if(i == 0) break;
			}
		}
	}

	printf("done\n");
	for(int i = 0; i < nfds; i++)
	{
		if(fds[i].fd != -1)
		{
			close(fds[i].fd);
		}
	}

	free(fds);

	socExit();
	free(soc_buff);

	acExit();
	svcExitThread();
}

int main(int argc, char **argv)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	printf("hello\n");

	Thread sock = threadCreate(sock_thread, NULL, 0x4000, 0x30, 0, true);

	while(aptMainLoop())
	{
		hidScanInput();
		u32 k = hidKeysDown();
		if(k & KEY_START) break;
		svcSleepThread(1000000); // Yield for a bit.
	}

	running = false;

	gfxExit();

	return 0;
}
