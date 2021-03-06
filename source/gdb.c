#include <3ds.h>
#include "shell.h"
#include "gdb.h"
#include <scenic/debug.h>
#include <scenic/proc.h>

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
	char *supp = "PacketSize=400;qXfer:features:read+;multiprocess+;QStartNoAckMode+";
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

char encode_buff[512];
int gdb_shell_out(int fd, void *ctx, const char *s, int len)
{
	if(len > 256) { len = 256; }

	encode(encode_buff, s, len);
	send_packet_prefix(fd, encode_buff, len * 2, "O");
}

void gdb_add_proc(gdb_ctx *ctx, gdb_proc_ctx *p_ctx, u32 pid)
{
	proc_list_node *n = malloc(sizeof(proc_list_node));
	n->c = p_ctx;
	n->next = NULL;

	if(ctx->procs.head == NULL) // need a new head
	{
		ctx->procs.head = n;
		ctx->procs.tail = n;
	}
	else
	{
		ctx->procs.tail->next = n;
		ctx->procs.tail = n;
	}
}

void gdb_del_proc(gdb_ctx *ctx, u32 pid)
{
	if(ctx->procs.head == NULL) return;
	gdb_proc_ctx *p = ctx->procs.head->c;

	if(p->pid == pid)
	{
		ctx->procs.head = ctx->procs.head->next;
		if(ctx->procs.head == NULL)
		{
			ctx->procs.tail = NULL;
		}
	}
	else
	{
		proc_list_node *c = ctx->procs.head;
		proc_list_node *old = NULL;

		while(c != NULL && c->c->pid != pid)
		{
			old = c;
			c = c->next;
		}

		if(c != NULL && old != NULL)
		{
			p = c->c;
			old->next = c->next;
		}

		if(ctx->procs.tail == c)
		{
			ctx->procs.tail = old;
		}
	}

	if(p != NULL)
	{
		proc_close(p->p);
		free(p);
	}
}

gdb_proc_ctx *gdb_get_proc(gdb_ctx *ctx, u32 pid)
{
	if(ctx->procs.head == NULL) return NULL;
	proc_list_node *c = ctx->procs.head;
	while(c != NULL && c->c->pid != pid)
	{
		c = c->next;
	}
	if(c == NULL) return NULL;
	return c->c;
}

scenic_thread *gdb_get_thread(gdb_ctx *ctx, u32 pid, u32 tid)
{
	gdb_proc_ctx *p = gdb_get_proc(ctx, pid);
	for(int i = 0; i < p->p->num_threads; i++)
	{
		if(p->p->threads[i].tid == tid)
		{
			return &p->p->threads[i];
		}
	}

	return NULL;
}

extern const char *target_xml;
extern const int target_xml_len;
extern const char *arm_core_xml;
extern const int arm_core_xml_len;
extern const char *arm_vfp_xml;
extern const int arm_vfp_xml_len;

int do_features_xfer(struct gdb_ctx *ctx, int fd, const char *annex, const char *rest)
{
	if(ctx->curr_proc == NULL)
	{
		send_packet(fd, "E01", strlen("E01"));
		return 0;
	}

	char *off_s = rest;
	char *off_end = strchr(off_s, ',');
	if(off_end == NULL) return -1;
	*off_end = 0;
	char *len_s = off_end + 1;

	unsigned long off = strtoul(off_s, NULL, 16);
	unsigned long len = strtoul(len_s, NULL, 16);

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
		gdb_proc_ctx *p = ctx->curr_proc;
		snprintf(buf, 16, "QCp%x.%x", p->pid, p->tid);
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
	else if(cmp(pkt_buf+2, "ThreadInfo") == 0)
	{
		if(ctx->curr_proc == NULL) // Not attached, error.
		{
			send_packet_prefix(fd, "01", 2, "E");
			return 0;
		}

		scenic_process *p = ctx->curr_proc->p;

		if(pkt_buf[1] == 'f') // q*f*threadinfo
		{
			ctx->curr_proc->_curr_thread_idx = 0;
		}
		else
		{
			if(ctx->curr_proc->_curr_thread_idx == p->num_threads)
			{
				send_packet(fd, "l", 1);
				return 0;
			}
		}

		char buff[256];
		buff[0] = 'm';
		int off = 1;

		if(proc_get_all_threads(p) < 0)
		{
			printf("proc_get_all_threads failed!\n");
			send_packet_prefix(fd, "01", 2, "E");
			return 0;
		}

		printf("getting threads... %i -> %i\n", ctx->curr_proc->_curr_thread_idx, p->num_threads);

		int i;
		for(i = ctx->curr_proc->_curr_thread_idx; i < p->num_threads; i++)
		{
			printf("tid %08x\n", p->threads[i].tid);
			int len = snprintf(buff + off, 256 - off, "p%x.%x,", p->pid, p->threads[i].tid);
			if(off + len > 256)
			{
				ctx->curr_proc->_curr_thread_idx = i;
				break;
			}
			off += len;
		}
		off--; // Remove the final comma!

		ctx->curr_proc->_curr_thread_idx = i;
		send_packet(fd, buff, off);

		return 0;
	}
	else if(cmp(pkt_buf+1, "Rcmd") == 0)
	{
		printf("got Rcmd\n");

		char line[256];
		memset(line, 0, 256);

		pkt_len -= strlen("qRcmd,");
		pkt_buf += strlen("qRcmd,");
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

int do_query_write(struct gdb_ctx *ctx, int fd, char *pkt_buf, size_t pkt_len)
{
	if(cmp(pkt_buf+1, "StartNoAckMode") == 0)
	{
		ctx->ack = 0;
		send_ok(fd);
	}
	else
	{
		return -1;
	}

	return 0;
}

int do_v_pkt(struct gdb_ctx *ctx, int fd, char *pkt_buf, size_t pkt_len)
{
	if(cmp(pkt_buf+1, "MustReplyEmpty") == 0)
	{
		send(fd, "$#00", 4, 0);
	}
	else if(cmp(pkt_buf+1, "Attach") == 0)
	{
		char *pid_s = strchr(pkt_buf, ';');
		if(pid_s != NULL)
		{
			pid_s++;

			unsigned long pid = strtoul(pid_s, NULL, 16);

			scenic_process *p = proc_open(pid, FLAG_DEBUG);
			debug_freeze(p);
			proc_get_all_threads(p);
			gdb_proc_ctx *p_ctx = malloc(sizeof(gdb_proc_ctx));
			p_ctx->pid = pid;
			p_ctx->tid = p->threads[0].tid;
			p_ctx->p = p;

			p_ctx->stop_reason = STOP_ATTACH;
			p_ctx->stop_status = 0;
			send_stop(fd, p_ctx);

			gdb_add_proc(ctx, p_ctx, pid);
			ctx->curr_proc = p_ctx;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}

	return 0;
}

int send_stop(int fd, struct gdb_proc_ctx *p_ctx)
{
	const char *prefix;
	char buf[3];

	if(p_ctx != NULL)
	{
		switch(p_ctx->stop_reason)
		{
			case STOP_ATTACH:
			case STOP_BREAK:
				prefix = "S";
			break;

			default:
				prefix = "W";
			break;
		}
		sprintf(buf, "%02x", p_ctx->stop_status);
	}
	else
	{
		prefix = "W";
		sprintf(buf, "01");
	}

	send_packet_prefix(fd, buf, 2, prefix);
}

int parse_pkt(struct gdb_ctx *ctx, int fd, char *pkt_buf, size_t pkt_len)
{
	pkt_buf[pkt_len - 1] = 0; // kill the #
	pkt_len--;

	// Yeah. I do bad things here. Sshhh.

	switch(pkt_buf[0])
	{
		case 'D':
		{
			char *pid_s = strchr(pkt_buf, ';');
			unsigned long pid = 0;
			if(pid_s != NULL)
			{
				pid = strtoul(pid_s, NULL, 16);
				if(pid == ctx->curr_proc->pid)
				{
					ctx->curr_proc = NULL;
				}
				gdb_del_proc(ctx, pid);
			}
			else
			{
				send_packet_prefix(fd, "01", 2, "E");
			}

			send_ok(fd);
		}
		break;

		case 'g':
		{
			scenic_debug_thread_ctx t_ctx;
			scenic_thread *t = proc_get_thread(ctx->curr_proc->p, ctx->curr_proc->tid);
			
			int r = debug_get_thread_ctx(t, &t_ctx);
			if(r < 0)
			{
				send_packet_prefix(fd, "01", 2, "E");
				return 0;
			}

			char regs[17 * 8 + 1]; // r0->r15, cpsr
			memset(regs, '0', 17*8);
			regs[8*17] = 0;

			encode(regs, &t_ctx, 17*4);

			send_packet(fd, regs, strlen(regs));

			return 0;
		}
		break;

		case 'G':
		{
			pkt_buf++;
			pkt_len--;

			scenic_thread *t = proc_get_thread(ctx->curr_proc->p, ctx->curr_proc->tid);

			scenic_debug_thread_ctx t_ctx;
			int r = debug_get_thread_ctx(t, &t_ctx);
			if(r < 0)
			{
				send_packet_prefix(fd, "01", 2, "E");
				return 0;
			}

			decode(&t_ctx, pkt_buf, pkt_len/2);
			r = debug_set_thread_ctx(t, &t_ctx);
			if(r < 0)
			{
				send_packet_prefix(fd, "01", 2, "E");
				return 0;
			}

			send_ok(fd);

			return 0;
		}
		break;

		case 'q':
		{
			return do_query(ctx, fd, pkt_buf, pkt_len);
		}
		break;

		case 'Q':
			return do_query_write(ctx, fd, pkt_buf, pkt_len);
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
				unsigned long addr = strtoul(pkt_buf + 1, NULL, 16);

				if(addr < 0x1000) // Just don't service requests near NULL.
				{
					send_packet(fd, "E01", strlen("E01"));
					return 0;
				}

				*p = ',';
				p++;
				unsigned long sz = strtoul(p, NULL, 16);

				char *tmp_buf = malloc(sz);
				char *out_buf = malloc(sz * 2);
				memset(tmp_buf, 0, sz);
				memset(out_buf, 0, sz);

				scenic_process *self = proc_open((u32)-1, FLAG_NONE); // self

				//printf("reading %x -> %x\n", addr, addr+sz - 1);

				if(dma_copy(self, tmp_buf, ctx->curr_proc->p, addr, sz) < 0)
				{
					printf("dma copy failed!!\n");
					send_packet(fd, "E01", strlen("E01"));
					return 0;
				}

				encode(out_buf, tmp_buf, sz);
				send_packet(fd, out_buf, sz * 2);
				free(tmp_buf);
				free(out_buf);
				proc_close(self);
				return 0;
			}
		}
		break;

		case 'T': // is thread alive?
		{
			char *pid_str = pkt_buf + 1; // skip 'T'
			char *pid_end = strchr(pid_str, '.');
			if(!pid_end) { send_packet(fd, "E01", strlen("E01")); return 0; }
			char *tid_str = pid_end + 1;

			unsigned long pid = strtoul(pid_str, NULL, 16);
			unsigned long tid = strtoul(tid_str, NULL, 16);

			if(gdb_get_thread(ctx, pid, tid) != NULL)
			{
				send_ok(fd);
				return 0;
			}

			send_packet_prefix(fd, "01", 2, "E");
			return 0;
		}
		break;

		case 'v':
		{
			return do_v_pkt(ctx, fd, pkt_buf, pkt_len);
		}
		break;

		case '?': // why'd it stop
		{
			send_stop(fd, ctx->curr_proc);
		}
		break;

		case 'H':
			if(pkt_buf[1] == 'g')
			{
				if(ctx->curr_proc == NULL)
				{
					send_packet(fd, "E01", strlen("E01"));
				}
				else
				{
					char *pid_str = pkt_buf + 3; // skip 'Hgp'
					char *pid_end = strchr(pid_str, '.');
					if(!pid_end) { send_packet(fd, "E01", strlen("E01")); return 0; }
					char *tid_str = pid_end + 1;


					unsigned long pid = strtoul(pid_str, NULL, 16); // we dont even use this..
					unsigned long tid = strtoul(tid_str, NULL, 16);

					if(tid == 0)
					{
						if(ctx->curr_proc->p->num_threads != 0)
						{
							tid = ctx->curr_proc->p->threads[0].tid;
						}
						else
						{
							tid = -1;
						}
					}

					printf("switched to %i\n", tid);

					ctx->curr_proc->tid = tid;
					send_ok(fd);
				}
			}
			else if(pkt_buf[1] == 'c')
			{
				return -1; // intentionally refuse this packet, it's bad
			}
		break;

		case 'Z':
		case 'z':
		{
			bool enable = pkt_buf[0] == 'Z';
			char type = pkt_buf[1];
			pkt_buf+=3; // skip Zn,

			if(type == '0')
			{
				char *addr_s = pkt_buf;
				char *addr_end = strchr(addr_s, ',');
				*addr_end = 0;
				const char *kind = addr_end+1;

				unsigned long addr = strtoul(addr_s, NULL, 16);
				printf("bkpt at %08x\n", addr);
				int32_t n = 0;
				if(enable)
				{
					if(debug_add_breakpoint(ctx->curr_proc, addr) < 0)
					{
						return -1;
					}
					else
					{
						send_ok(fd);
					}
				}
				else
				{
					debug_remove_breakpoint(ctx->curr_proc, addr);
					send_ok(fd);
				}
			}
			else
			{
				printf(":(\n");
				return -1;
			}
		}
		break;

		case '!':
			send_ok(fd);
		break;

		default:
			return -1;
		break;
	}

	return 0;
}

int gdb_do_packet(int fd, struct client_ctx *ctx)
{
	char pkt_buf[1024];
	char checksum[2];
	memset(pkt_buf, 0, 1024);

	int r = read_until(fd, pkt_buf, 1024, "#", 1);
	int pkt_len = r;
	if(r == -1 || r == 0) { return r; }

	r = recv(fd, checksum, 2, 0);

	// todo: actual checksum..
	gdb_ctx *c = (gdb_ctx*)ctx->data;
	if(c->ack) send(fd, "+", 1, 0);

	if(parse_pkt(c, fd, pkt_buf, pkt_len) == -1)
	{
		printf("refused pkt: '%s'\n", pkt_buf);
		send(fd, "$#00", 4, 0);
	}

	return r;
}