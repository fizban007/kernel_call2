#include <linux/acc_motion.h>
#include <linux/dev_acceleration.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/list.h>

int accevt_destroy(int event_id)
{
	return 0;
}


/*SYSCALL_DEFINE1(accevt_wait, int __user *, event_id)*/
SYSCALL_DEFINE1(accevt_destroy, int, event_id)
{
	return accevt_destroy(event_id);
}

