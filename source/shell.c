#include <3ds.h>
#include <scenic/proc.h>
#include <scenic/kernel/kproc.h>
#include <scenic/kernel/kmem.h>

int shell_cmd(int fd, u8 type)
{
	printf("got shell command %02x\n", type);
	return 0;
}