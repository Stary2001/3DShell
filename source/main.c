#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include "sock_util.h"
#include "client.h"
#include "shell.h"
#include "gdb.h"

int running = 1;

int accept_cback(int fd, struct client_ctx *ctx)
{
	u8 type;
	int i = recv(fd, &type, 1, MSG_PEEK);
	printf("char: %c\n", type);
	//svcSleepThread(4000000000ULL);

	if(type == '+')
	{
		ctx->type = TYPE_GDB;
		ctx->data = malloc(sizeof(struct gdb_ctx));
		gdb_ctx *c = (gdb_ctx*)ctx->data;
		c->shell.out = gdb_shell_out;
		c->ack = 1;
		c->pid = -1;
		c->tid = -1;
		c->_curr_thread_idx = 0;
	}
	else
	{
		ctx->type = TYPE_SHELL;
		ctx->data = malloc(sizeof(struct shell_ctx));
		shell_ctx *c = (shell_ctx*)ctx->data;
		c->out = sock_shell_out;
	}
}

int data_cback(int fd, struct client_ctx *ctx)
{
	if(ctx->type == TYPE_SHELL)
	{
		int r = process_cmd(fd, ctx);
		if(r == 1)
		{
			printf("stopping...\n");
			running = 0;
		}
		else if(r == -1)
		{
			return -1;
		}
	}
	else if(ctx->type == TYPE_GDB)
	{
		u8 packet_type = 0;
		int r = recv(fd, &packet_type, 1, 0);
		if(r == -1 || r == 0)
		{
			return -1;
		}

		switch(packet_type)
		{
			case '$':
				{	
					r = gdb_do_packet(fd, ctx);
					if(r == -1 || r == 0)
					{
						return -1;
					}
				}
			break;

			case '+': // successful
			break;

			case '-':
				printf("gdb doesn't like this\n");
				return -1;
			break;

			default:
				printf("unhandled gdb '%02x' (%c)\n", packet_type, packet_type);
			break;
		}
	}
}

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
  	int r = 0;

  	struct server_ctx *serv = server_bind(gethostid(), 1111);
  	printf("got server %08x\n", serv);
  	if(serv == NULL)
  	{
  		goto done;
  	}

	while(running)
	{
		r = server_poll(serv, 100, accept_cback, data_cback);
		if(r == -1) break;
	}

	server_destroy(serv);

	done:
	socExit();
	free(soc_buff);

	acExit();
	threadExit(0);
}

int main(int argc, char **argv)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	printf("hello\n");

	debug_enable();

	Thread sock = threadCreate(sock_thread, NULL, 0x4000, 0x30, 0, false);

	while(aptMainLoop())
	{
		hidScanInput();
		u32 k = hidKeysDown();
		if(k & KEY_START) break;
		svcSleepThread(1000000); // Yield for a bit.
	}

	running = false;
	threadJoin(sock, U64_MAX);
	threadFree(sock);

	gfxExit();

	return 0;
}
