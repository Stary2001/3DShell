#include <3ds.h>
#include <scenic/proc.h>
#include <scenic/kernel/kproc.h>
#include <scenic/kernel/kmem.h>

#define checked_recv(fd, v, l) if((r=read(fd, v, l, 0)) == -1) { return -1; }
#define VER 0

int send_success(int fd, u8 type, u8 success)
{
	send(fd, &type, 1, 0);
	send(fd, &success, 1, 0);
}


int shell_cmd(int fd, u8 type)
{
	int r = 0;

	printf("got shell command %02x\n", type);
	switch(type)
	{
		case 0x00: // hello
		{
			u8 ver;
			checked_recv(fd, &ver, 1);
			if(ver > VER)
			{
				send_success(fd, 0, 1);
				return -1;
			}
			u8 len;
			checked_recv(fd, &len, 1);
			char buf[256];
			checked_recv(fd, buf, len);
			buf[len] = 0;
			printf("hello from %s\n", buf);

			send_success(fd, 0, 1);
		}
		break;

		case 0x01: // proc
			send_success(fd, 1, 0);
		break;

		case 0x02: // peek
			send_success(fd, 2, 0);
		break;

		case 0x03: // poke
			send_success(fd, 3, 0);
		break;
	}
	return 0;
}