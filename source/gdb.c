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
	
	u8 chk = cksum(p, len);
	sprintf(chk_str, "%02x", chk);

	send(fd, "$", 1, 0);
	send(fd, p, len, 0);
	send(fd, "#", 1, 0);
	send(fd, chk_str, 2, 0);
}

void send_supported(int fd)
{
	char *supp = "PacketSize=512;qXfer:features:read+";
	send_packet(fd, supp, strlen(supp));
}

void send_ok(int fd)
{
	char *ok = "OK";
	send_packet(fd, ok, strlen(ok));
}

int cmp(char *buf, const char *buff)
{
	return strncmp(buf, buff, strlen(buff));
}

extern const char *target_xml;
extern const int target_xml_len;
extern const char *arm_core_xml;
extern const int arm_core_xml_len;
extern const char *arm_vfp_xml;
extern const int arm_vfp_xml_len;

int parse_pkt(int fd, char *pkt_buf, size_t pkt_len)
{
	pkt_buf[pkt_len - 1] = 0; // kill the #

	// Yeah. I do bad things here. Sshhh.

	switch(pkt_buf[0])
	{
		case 'g':
		{
			char regs[17 * 8 + 1]; // r0->r15, cpsr
			memset(regs, '0', 17*8);
			regs[8*17] = 0;

			send_packet(fd, regs, strlen(regs));
		}
		break;

		case 'q':
		{
			if(cmp(pkt_buf+1, "Supported") == 0)
			{
				printf("qSupported\n");
				send_supported(fd);
			}
			else if(cmp(pkt_buf+1, "Xfer") == 0)
			{
				pkt_buf[5] = 0; // "qXfer:"

				char *object = pkt_buf + 6;
				char *object_end = strchr(object, ':');
				if(object_end == NULL) return -1;
				*object_end = 0;

				if(cmp(object, "features") == 0)
				{
					char *op = object_end + 1;
					char *op_end = strchr(op, ':');
					if(op_end == NULL) return -1;
					*op_end = 0;

					char *annex = op_end + 1;
					char *annex_end = strchr(annex, ':');
					if(annex_end == NULL) return -1;
					*annex_end = 0;

					char *off_s = annex_end + 1;
					char *off_end = strchr(off_s, ',');
					if(off_end == NULL) return -1;
					*off_end = 0;
					char *len_s = off_end + 1;

					unsigned long off = strtoul(off_s, NULL, 16);
					unsigned long len = strtoul(len_s, NULL, 16);

					printf("qXfer %s %s file %s, %u/%u\n", object, op, annex, off, len);

					char *v = NULL;
					int target_len = 0;

					if(cmp(annex, "target.xml") == 0)
					{
						v = &target_xml;
						target_len = target_xml_len;
					}
					else if(cmp(annex, "arm-core.xml") == 0)
					{
						v = &arm_core_xml;
						target_len = arm_core_xml_len;
					}
					else if(cmp(annex, "arm-vfpv2.xml") == 0)
					{
						v = &arm_vfp_xml;
						target_len = arm_vfp_xml_len;
					}

					if(v == NULL)
					{
						return -1;	
					}
					
					v += off;
					target_len -= off;

					u8 chk;
					
					if(off + len > target_len) // would we overrun with the *entire* buffer?
					{
						send(fd, "$l", 2, 0); // end of data, dont truncate length
						chk = 'l';
					}
					else
					{
						send(fd, "$m", 2, 0); // still more data, only send buffer size
						chk = 'm';
						target_len = len;
					}

					chk += cksum(v, target_len);
					send(fd, v, target_len, 0);

					char chk_str[3];
					chk_str[2] = 0;
					sprintf(chk_str, "%02x", chk);
					send(fd, "#", 1, 0);
					send(fd, chk_str, 2, 0);

					return 0;
				}

				return -1;
			}
			else
			{
				return -1;
			}
		}
		break;

		case 'm':
		{
			printf("m: %s\n", pkt_buf);
			char *p = strchr(pkt_buf, ',');
			if(p == NULL)
			{
				return -1;
			}
			else 
			{
				*p = 0;
				unsigned long addr = strtoul(pkt_buf + 2, NULL, 16);
				*p = ',';
				p++;
				unsigned long sz = strtoul(p, NULL, 16);

				char buf[256+1]; 
				memset(buf, '0', 256);
				buf[256] = 0;
				send_packet(fd, buf, sz * 2);
			}
		}

		case 'v':
		{
			if(cmp(pkt_buf+1, "MustReplyEmpty") == 0)
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
			//printf("unk: '%s'\n", pkt_buf);
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

	r = recv(fd, checksum, 2, 0);

	// todo: actual checksum..
	send(fd, "+", 1, 0);
	if(parse_pkt(fd, pkt_buf, pkt_len) == -1)
	{
		send(fd, "$#00", 4, 0);
	}

	return r;
}