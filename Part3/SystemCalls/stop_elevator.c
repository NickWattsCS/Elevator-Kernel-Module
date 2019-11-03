#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

#include "systemcalls.h"

int (*STUB_stop_elevator)(void) = NULL;
EXPORT_SYMBOL(STUB_stop_elevator);

SYSCALL_DEFINE0(stop_elevator)
{
	printk(KERN_NOTICE "Inside SYSCALL_DEFINE1 block. %s\n", __FUNCTION__);

	if (STUB_stop_elevator != NULL)
	{
		return STUB_stop_elevator();
	}
	else
	{
		return -ENOSYS;
	}
}
