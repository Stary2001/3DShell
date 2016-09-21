#include <3ds.h>
#include "shell.h"

#define is(s) 

u8 cksum(char *buf, size_t len)
{
	u8 sum = 0;
	for(int i = 0; i < len; i++)
	{
		sum += buf[i];
	}
	return sum;
}

void send_packet(int fd, char *p, size_t len)
{
	char chk_str[3];
	chk_str[2] = 0;
	// static supported list for now....
	
	u8 chk = cksum(p, len);
	sprintf(chk_str, "%02x", chk);

	send(fd, "$", 1, 0);
	send(fd, p, len, 0);
	send(fd, "#", 1, 0);
	send(fd, chk_str, 2, 0);
}

void send_supported(int fd)
{
	char *supp = "PacketSize=512";
	send_packet(fd, supp, strlen(supp));
}

void send_ok(int fd)
{
	char *ok = "OK";
	send_packet(fd, ok, strlen(ok));
}

int parse_pkt(int fd, char *pkt_buf, size_t pkt_len)
{
	pkt_buf[pkt_len - 1] = 0; // kill the #

	switch(pkt_buf[0])
	{
		case 'g':
		{
			char regs[38 * 8 + 1];
			memset(regs, '0', 38*8);
			regs[8*38] = 0;

			send_packet(fd, regs, strlen(regs));
		}
		break;

		case 'q':
		{
			if(strncmp(pkt_buf+1, "Supported", strlen("Supported")) == 0)
			{
				printf("qSupported\n");
				send_supported(fd);
			}
			else
			{
				return -1;
			}
		}
		break;

		case 'm':
		{
			char *p = strchr(pkt_buf, ',');
			if(p == NULL)
			{
				return -1;
			}
			else 
			{
				*p = 0;
				int addr = atoi(pkt_buf);
				*p = ',';
				p++;
				int sz = atoi(p);

				char buf[256+1]; 
				memset(buf, '0', 256);
				buf[256] = 0;
				send_packet(fd, buf, sz * 2);
			}
		}

		case 'v':
		{
			if(strncmp(pkt_buf+1, "MustReplyEmpty", strlen("MustReplyEmpty")) == 0)
			{
				send(fd, "$#00", 4, 0);
			}
		}
		break;

		case '?': // why'd it stop
		{
			char *s = "S01";
			send_packet(fd, s, strlen(s));
		}
		break;

		default:
			printf("unk: '%s'\n", pkt_buf);
			return -1;
		break;
	}

	return 0;
}

int gdb_do_packet(int fd, struct client_ctx *ctx)
{
	char pkt_buf[512];
	char checksum[2];
	memset(pkt_buf, 0, 512);

	int r = read_until(fd, pkt_buf, 512, "#", 1);
	int pkt_len = r;
	if(r == -1 || r == 0) { return r; }

	printf("read_until: %i\n", r);
	printf("'%s'\n", pkt_buf);

	r = recv(fd, checksum, 2, 0);

	// todo: actual checksum..
	send(fd, "+", 1, 0);
	if(parse_pkt(fd, pkt_buf, pkt_len) == -1)
	{
		send(fd, "$#00", 4, 0);
	}

	return r;
}