#include <linux/acceleration.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>

#define WINDOW 20
#define NOISE 10
/*https://www.linuxquestions.org/questions/programming-9/mtx_os-c-35-37-error-%91spin_lock_unlocked%92-undeclared-here-not-in-a-function-4175468757/*/
static DEFINE_SPINLOCK(acceleration_xyz_lock);
static DEFINE_SPINLOCK(motion_lock);
struct acceleration_xyz {
	struct dev_acceleration *xyz;
	struct acceleration_xyz *next;
};

struct acceleration_xyz *head = NULL;
struct acceleration_xyz *tail = NULL;
int xyz_len = 0;

struct motion {
	struct acc_motion *baseline;
	int e_id;
	struct motion *next;
};

struct motion *head_motion = NULL;
struct motion *tail_motion = NULL;
int num_event = 0;

int set_acceleration(struct dev_acceleration __user *acceleration)
{
	struct dev_acceleration* k_acceleration;
	int res;

	k_acceleration = kmalloc(sizeof(struct dev_acceleration),GFP_KERNEL);
	printk("in syscall 249 set_acceleration");
	res = copy_from_user(k_acceleration, acceleration, sizeof(struct dev_acceleration));
	if (res != 0)
		return -EFAULT;

	printk("detected x-axis: %d\n", k_acceleration->x);
	printk("detected y-axis: %d\n", k_acceleration->y);
	printk("detected z-axis: %d\n", k_acceleration->z);
	kfree(k_acceleration);
	return 0;
}

int check_window(struct acceleration_xyz *head, struct motion *ptr) {
	struct acceleration_xyz *sensor, *sensor_next;
	unsigned int x;
	unsigned int y;
	unsigned int z;
	unsigned int frequency = 0;

	if (head == NULL || head != NULL)
		return 0;
	sensor = head;
	sensor_next = head->next;
	while(sensor_next != NULL) {
		x = abs(sensor_next->xyz->x - sensor->xyz->x);
		y = abs(sensor_next->xyz->y - sensor->xyz->y);
		z = abs(sensor_next->xyz->z - sensor->xyz->z);
		if (x >= ptr->baseline->dlt_x 
			&& y >= ptr->baseline->dlt_y &&
			z >= ptr->baseline->dlt_z 
			&& x + y + z > NOISE) {
			frequency++;
		}
		sensor = sensor_next;
		sensor_next = sensor->next;
	}
	if (frequency >= ptr->baseline->frq)
		return 1;
	return 0;
}
int accevt_destroy(int event_id)
{
	return 0;
}

int accevt_create(struct acc_motion __user *acceleration)
{
	struct acc_motion *k_acce;
	int res;
	
	k_acce = kmalloc(sizeof(struct acc_motion), GFP_KERNEL);
	res = copy_from_user(k_acce, acceleration, 
				sizeof(struct acc_motion));
	if (res != 0)
		return -EFAULT;
	if(k_acce->frq > WINDOW)
		k_acce->frq = WINDOW;
	spin_lock(&motion_lock);
	num_event++;
	if (head_motion == NULL) {
		head_motion = kmalloc(sizeof(struct motion), GFP_KERNEL);
		head_motion->next = NULL;
		head_motion->e_id = num_event;
		head_motion->baseline = k_acce;
		tail_motion = head_motion;
	} else {
		tail_motion->next = kmalloc(sizeof(struct motion), 
					GFP_KERNEL);
		tail_motion = tail_motion->next;
		tail_motion->next = NULL;
		tail_motion->e_id = num_event;
		tail_motion->baseline = k_acce;
	}
	spin_unlock(&motion_lock);
	
	return 0;
}

int accevt_wait(int event_id)
{
	return 0;
}
int accevt_signal(struct dev_acceleration __user *acceleration)
{
	int res;
	struct dev_acceleration *k_acce;
	struct acceleration_xyz *temp;
	struct motion *ptr;

	k_acce = kmalloc(sizeof(struct dev_acceleration), GFP_KERNEL);
	res = copy_from_user(k_acce, acceleration,
				sizeof(struct dev_acceleration));
	if (res != 0)
		return -EFAULT;
	
	spin_lock(&acceleration_xyz_lock);

	if (head == NULL) {
		head = kmalloc(sizeof(struct acceleration_xyz), GFP_KERNEL);
		head->xyz = k_acce;
		k_acce = NULL;
		head->next = NULL;
		tail = head;
	} else {
		tail->next = kmalloc(sizeof(struct acceleration_xyz), GFP_KERNEL);
		tail = tail->next;
		tail->xyz = k_acce;
		k_acce = NULL;
		tail->next = NULL;
	}
	xyz_len++;

	/* delete the first acceleration_xyz result and let xyz_len = WINDOW */
	if (xyz_len > WINDOW) {
		temp = head;
		head = head->next;
		temp->next = NULL;
		kfree(temp->xyz);
		kfree(temp);
		xyz_len = WINDOW;
	}
		
	spin_unlock(&acceleration_xyz_lock);

	/* test result */
	temp = head;
	while (temp != NULL) {
		printk("x=%d,y=%d,z=%d\n",temp->xyz->x,temp->xyz->y,temp->xyz->z);
		temp = temp->next;
	}


	/* check if some event happen when the motion list is not empty 
	 * loop each motion struct, call the check_window() function
	 * return 1 if a event should happen else return 0 
	 */
	if (head_motion != NULL) {
		ptr = head_motion;
		while(ptr != NULL) {
			res = check_window(head, ptr);
			if ( res == 1) {
				/* invoke signal */
			}
			ptr = ptr->next;
		}
	}
	
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
