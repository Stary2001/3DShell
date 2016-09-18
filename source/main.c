#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>

#define MAX_CONN 8

int shell_cmd(int fd, u8 type);

int do_cmd(int client_fd)
{
	int r = 0;

	u8 cmd;
	r = recv(client_fd, &cmd, 1, 0);
	if(r == 0 || r == -1)
	{
		printf("recv error\n");
		return -1;
	}
	else
	{
		return shell_cmd(client_fd, cmd);
	}

	return 0;
}

/*void compact(struct pollfd *fds, int *nfds)
{
	int tail = -1;

	for(int i = 0; i < *nfds; i++)
	{
		if(fds[i].fd != -1)
		{
			if(i != 0 && tail != i-1)
			{
				tail++;
				fds[tail].fd = fds[i].fd;
				fds[i].fd = -1;
			}
			else
			{
				tail = i;
			}
		}
	}
}*/

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
  	serv_addr.sin_addr.s_addr = gethostid();
  	serv_addr.sin_port = htons(1337);

  	int one = 1;
    r = setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

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
							fds[nfds].fd = r;
							fds[nfds].events = POLLIN;
							fds[nfds].revents = 0;
							nfds++;

							printf("accepted connection from %s:%u\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
						}
					}
				}
				else
				{
					if(do_cmd(fds[i].fd))
					{
						printf("stopping...\n");
						running = 0;
					}
				}
			}
			else if((fds[i].revents & POLLERR) || (fds[i].revents & POLLNVAL))
			{
				printf("closing %i slot %i\n", fds[i].fd, i);
				close(fds[i].fd);
				fds[i].fd = -1;
				//compact(fds, &nfds);

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
