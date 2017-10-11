#include <linux/dev_acceleration.h>
#include <linux/acc_motion.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/string.h>

int accevt_signal(struct dev_acceleration __user *acceleration)
{
	return 0;
}

SYSCALL_DEFINE1(accevt_signal, struct dev_acceleration __user*, acceleration)
{
	return accevt_signal(acceleration);
}
