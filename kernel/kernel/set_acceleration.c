#include <linux/dev_acceleration.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/string.h>


int set_acceleration(struct dev_acceleration __user *acceleration)
{
	return 0;
}

SYSCALL_DEFINE1(set_acceleration, struct dev_acceleration __user *, acceleration)
{
	return set_acceleration(acceleration);
}
