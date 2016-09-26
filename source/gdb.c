#include <3ds.h>
#include <arpa/inet.h> // just for htons..
#include "shell.h"
#include "gdb.h"

int encode(char *dst, char *src, int len) // length of source buffer
{
	const char *chars = "0123456789abcdef";

	int i = 0;
	while(len--)
	{
		*dst++ = chars[(src[i] & 0xf0) >> 4];
		*dst++ = chars[src[i++] & 0xf];
	}
	return i;
}

int decode(char *dst, char *src, int len) // length of dst buffer
{
	char a[3];
	a[2] = 0;

	int i = 0;
	while(len--)
	{
		memcpy(a, src, 2);
		src += 2;
		dst[i++] = (char)strtoul(a, NULL, 16);
	}
	return i;
}

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

void send_packet_prefix(int fd, char *p, size_t len, const char *prefix)
{
	char chk_str[3];
	chk_str[2] = 0;
	
	u8 chk = cksum(p, len);
	chk += cksum(prefix, strlen(prefix));
	sprintf(chk_str, "%02x", chk);

	send(fd, "$", 1, 0);
	send(fd, prefix, strlen(prefix), 0);
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

int gdb_shell_out(int fd, void *ctx, const char *s)
{
	char encode_buff[512];
	int len = strlen(s);
	if(len > 256) { len = 256; }
	encode(encode_buff, s, len);

	send_packet_prefix(fd, encode_buff, strlen(encode_buff), "O");
}

extern const char *target_xml;
extern const int target_xml_len;
extern const char *arm_core_xml;
extern const int arm_core_xml_len;
extern const char *arm_vfp_xml;
extern const int arm_vfp_xml_len;

int do_features_xfer(struct gdb_ctx *ctx, int fd, const char *annex, const char *rest)
{
	char *off_s = rest;
	char *off_end = strchr(off_s, ',');
	if(off_end == NULL) return -1;
	*off_end = 0;
	char *len_s = off_end + 1;

	unsigned long off = strtoul(off_s, NULL, 16);
	unsigned long len = strtoul(len_s, NULL, 16);

	printf("qXfer features read file %s, %u/%u\n", annex, off, len);

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

	if(target_len <= off)
	{
		send_packet(fd, "l", 1);	
		return 0;
	}

	v += off;
	target_len -= off;
	printf("sending %i bytes\n", target_len);

	u8 chk;

	if(off + len > target_len) // would we overrun our buffer sending len bytes?
	{
		send_packet_prefix(fd, v, target_len, "l"); // send what's left of the file.
	}
	else
	{
		send_packet_prefix(fd, v, len, "m"); // send a gdb-buffer sized chunk.
	}

	return 0;
}

int do_query(struct gdb_ctx *ctx, int fd, char *pkt_buf, size_t pkt_len)
{
	if(pkt_buf[1] == 'C') // qC = 'get current thread id'
	{
		char buf[16];
		snprintf(buf, 16, "QC %02X", htons(ctx->tid));
		send_packet(fd, buf, strlen(buf));
	}
	else if(cmp(pkt_buf+1, "Attached") == 0)
	{
		send_packet(fd, "1", 1);
	}
	else if(cmp(pkt_buf+1, "Supported") == 0)
	{
		send_supported(fd);
	}
	else if(cmp(pkt_buf+1, "Xfer") == 0)
	{
		char *object = pkt_buf + 6;
		char *object_end = strchr(object, ':');
		if(object_end == NULL) return -1;
		*object_end = 0;

		char *op = object_end + 1;
		char *op_end = strchr(op, ':');
		if(op_end == NULL) return -1;
		*op_end = 0;

		char *annex = op_end + 1;
		char *annex_end = strchr(annex, ':');
		if(annex_end == NULL) return -1;
		*annex_end = 0;

		if(cmp(op, "read") == 0)
		{
			if(cmp(object, "features") == 0)
			{
				return do_features_xfer(ctx, fd, annex, annex_end + 1);
			}
		}

		return -1;
	}
	else if(cmp(pkt_buf+1, "fThreadInfo") == 0)
	{
		return -1; // stubbed
	}
	else if(cmp(pkt_buf+1, "Rcmd") == 0)
	{
		printf("got Rcmd\n");

		char line[256];
		memset(line, 0, 256);

		pkt_len -= strlen("qRcmd,");
		pkt_buf += strlen("qRcmd,");
		printf("hex %s %i long\n", pkt_buf, pkt_len);

		decode(line, pkt_buf, pkt_len/2);
		int r = process_line(fd, &ctx->shell, line);
		send_packet(fd, "OK", 2);
		return r;
	}
	else
	{
		return -1; // catch-all
	}

	return 0;
}

int parse_pkt(struct gdb_ctx *ctx, int fd, char *pkt_buf, size_t pkt_len)
{
	pkt_buf[pkt_len - 1] = 0; // kill the #
	pkt_len--;

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
			return do_query(ctx, fd, pkt_buf, pkt_len);
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

		case 'H':
			if(pkt_buf[1] == 'g')
			{
				unsigned long tid = strtoul(pkt_buf + 2, NULL, 16);
				if(tid == 0)
				{
					tid = 1; // first thread. shh.
				}
				ctx->tid = tid;
				send_ok(fd);
			}
			else if(pkt_buf[1] == 'c')
			{
				return -1; // intentionally refuse this packet, it's bad
			}
		break;

		default:
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

	if(parse_pkt((struct gdb_ctx*)ctx->data, fd, pkt_buf, pkt_len) == -1)
	{
		printf("refused pkt: '%s'\n", pkt_buf);
		send(fd, "$#00", 4, 0);
	}

	return r;
}