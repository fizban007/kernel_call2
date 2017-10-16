#include <linux/acc_motion.h>
#include <linux/dev_acceleration.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/errno.h>

int accevt_wait(int event_id)
{
	return 0;
}

/*SYSCALL_DEFINE1(accevt_wait, int __user *, event_id)*/
SYSCALL_DEFINE1(accevt_wait, int, event_id)
{
	return accevt_wait(event_id);
}
