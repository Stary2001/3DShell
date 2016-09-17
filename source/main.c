#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <scenic/proc.h>
#include <scenic/kernel/kproc.h>
#include <scenic/kernel/kmem.h>

int spin_pls[3];
void spin_loop(void *p)
{
	int i = (int)p;

	while(spin_pls[i])
	{
		svcSleepThread(0); // yield
	}
}

int main(int argc, char **argv)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	debug_enable();

	scenic_kproc *p = kproc_find(0x10);
	if(p)
	{
		printf("current kproc: %08x %08x\n", p, p->ptr);
		scenic_kthread *t = kproc_get_list_head(p);
		if(t)
		{
			printf("got t, %08x\n", t);

			while(t = kthread_next(t))
			{
				printf("got t, %08x\n", t);
			}
		}
	}

	/*printf("enabled debug\n");
	scenic_process *p = proc_open(0x10, FLAG_DEBUG);
	printf("proc %08x has handle %08x and debug %08x\n", p, p->handle, p->debug);

	debug_freeze(p);
	printf("HID frozen! try it out!\n");

	int counter = 3000000;*/

	while(aptMainLoop())
	{
		hidScanInput();
		u32 k = hidKeysDown();
		if(k & KEY_START) break;
		/*if(counter-- == 0)
		{
			debug_thaw(p);
			printf("HID thawed!\n");
		}*/
	}

	//proc_close(p);
	gfxExit();

	return 0;
}
