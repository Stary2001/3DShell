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

	if(type == '+')
	{
		ctx->type = TYPE_GDB;
		ctx->data = malloc(sizeof(struct gdb_ctx));
		gdb_ctx *c = (gdb_ctx*)ctx->data;
		c->shell.out = gdb_shell_out;
		c->ack = 1;
		c->curr_proc = NULL;
		c->procs.head = c->procs.tail = NULL;

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

void wait_for_wifi()
{
	u32 wifi = 0;
	Result res = 0;

	while(!wifi && (res == 0 || res == 0xE0A09D2E))
	{
		res = ACU_GetWifiStatus(&wifi);
		if(res != 0)
		{
			wifi = 0;
		}
		svcSleepThread(10000000ULL);
	}
}

void sock_thread(void *arg)
{
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

  	wait_for_wifi();

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
	// debugging... this should crash
	int a = *(int*)0xdead;

	fsInit();
	sdmcInit();
	FILE *logfile = fopen("log.txt", "w");
	fprintf(logfile, "hi mom\n");
	fflush(logfile);
	fclose(logfile);

	/*kproc_init();
	debug_enable();
	Thread sock = threadCreate(sock_thread, NULL, 0x1000, 0x30, 1, false);
	threadJoin(sock, U64_MAX);
	threadFree(sock);*/
	fsExit();
	sdmcExit();

	svcExitProcess(0);

	return 0;
}
