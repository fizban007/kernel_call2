#include <linux/acceleration.h>
#include <linux/syscalls.h>
#include <linux/errno.h>

int set_acceleration(struct dev_acceleration __user *acceleration)
{
	printk("in syscall 249 set_acceleration");
	struct dev_acceleration* k_acceleration;
	int size = sizeof(acceleration);
	if(copy_from_user(k_acceleration, acceleration, size)
		return -EFAULT;

	printk("detected x-axis: %d", k_acceleration->x);
	printk("detected y-axis: %d", k_acceleration->y);
	printk("detected z-axis: %d", k_acceleration->z);

	return 0;
}
int accevt_destroy(int event_id)
{
	return 0;
}

int accevt_create(struct acc_motion __user *acceleration)
{
	return 0;
}
int accevt_wait(int event_id)
{
	return 0;
}
int accevt_signal(struct dev_acceleration __user *acceleration)
{
	return 0;
}
SYSCALL_DEFINE1(accevt_destroy, int, event_id)
{
	return accevt_destroy(event_id);
}
SYSCALL_DEFINE1(accevt_create, struct acc_motion __user *, acceleration)
{
	return accevt_create(acceleration);
}
SYSCALL_DEFINE1(accevt_wait, int, event_id)
{
	return accevt_wait(event_id);
}
SYSCALL_DEFINE1(accevt_signal, struct dev_acceleration __user *, acceleration)
{
	return accevt_signal(acceleration);
}
SYSCALL_DEFINE1(set_acceleration, struct dev_acceleration __user *, acceleration)
{
	return set_acceleration(acceleration);
}
