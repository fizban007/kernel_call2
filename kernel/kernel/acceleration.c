#include <linux/acceleration.h>
#include <linux/syscalls.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define WINDOW 20
#define NOISE 10
/*https://www.linuxquestions.org/questions/programming-9/mtx_os-c-35-37-error-%91spin_lock_unlocked%92-undeclared-here-not-in-a-function-4175468757/*/
static DEFINE_SPINLOCK(acceleration_xyz_lock);
static DEFINE_SPINLOCK(event_lock);
/*http://tuxthink.blogspot.com/2011/04/wait-queues.html*/
DECLARE_WAIT_QUEUE_HEAD(process);

struct acceleration_xyz {
	struct dev_acceleration *xyz;
	struct acceleration_xyz *next;
};

struct acceleration_xyz *head = NULL;
struct acceleration_xyz *tail = NULL;
int xyz_len = 0;

struct event {
	struct acc_motion *baseline;
	int id;
	int condition; /* 1 if wake up 0 if put process to wait */
	wait_queue_head_t q;
	struct mutex process_lock;
	int wait_proc_count;
	
	struct event *next;
	
};

struct event *head_event = NULL;
struct event *tail_event = NULL;
int num_event = 0;

int set_acceleration(struct dev_acceleration __user *acceleration)
{
	struct dev_acceleration* k_acceleration;
	int res;

	k_acceleration = kmalloc(sizeof(struct dev_acceleration),GFP_KERNEL);
	if (k_acceleration == NULL)
		return -ENOMEM;
	printk("in syscall 249 set_acceleration");
	res = copy_from_user(k_acceleration, acceleration, sizeof(struct dev_acceleration));
	if (res != 0)
		return -EFAULT;

/*	printk("detected x-axis: %d\n", k_acceleration->x);
	printk("detected y-axis: %d\n", k_acceleration->y);
	printk("detected z-axis: %d\n", k_acceleration->z);*/
	kfree(k_acceleration);
	return 0;
}

int check_window(struct acceleration_xyz *head, struct event *ptr) {
	struct acceleration_xyz *sensor, *sensor_next;
	unsigned int x = 0, y = 0, z = 0;
	unsigned int delta_x, delta_y, delta_z;
	unsigned int frequency = 0;

	if (head == NULL || head->next == NULL)
		return 0;
	sensor = head;
	sensor_next = head->next;
	while(sensor_next != NULL) {
		delta_x = abs(sensor_next->xyz->x - sensor->xyz->x);
		delta_y = abs(sensor_next->xyz->y - sensor->xyz->y);
		delta_z = abs(sensor_next->xyz->z - sensor->xyz->z);
		x += delta_x;
		y += delta_y;
		z += delta_z;
		if (delta_x + delta_y + delta_z > NOISE) {
			frequency++;
		}
		sensor = sensor_next;
		sensor_next = sensor->next;
	}
	printk("frequency=%d",frequency);
	printk("x=%d y=%d z=%d\n",x,y,z);
	printk("%d %d %d %d\n",ptr->baseline->frq, ptr->baseline->dlt_x, ptr->baseline->dlt_y, 
		ptr->baseline->dlt_z);
	if (frequency >= ptr->baseline->frq && x >= ptr->baseline->dlt_x
		&& y >= ptr->baseline->dlt_y && z >= ptr->baseline->dlt_z)
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
	struct event *cur_event;
	int res;

	printk("in the syscall create\n");	
	k_acce = kmalloc(sizeof(struct acc_motion), GFP_KERNEL);
	if(k_acce == NULL)
		return -ENOMEM;
	res = copy_from_user(k_acce, acceleration, 
				sizeof(struct acc_motion));
	if (res != 0)
		return -EFAULT;
	if(k_acce->frq > WINDOW)
		k_acce->frq = WINDOW;
	cur_event = kmalloc(sizeof(struct event), GFP_KERNEL);
	if (cur_event == NULL) {
		kfree(k_acce);
		return -ENOMEM;
	}
	cur_event->next = NULL;
	cur_event->id = num_event;
	cur_event->baseline = k_acce;
	cur_event->condition = 0;
	cur_event->wait_proc_count = 0;
	init_waitqueue_head(&(cur_event->q));
	mutex_init(&(cur_event->process_lock));
	spin_lock(&event_lock);
	num_event++;
	if (head_event == NULL) {
		head_event = cur_event;
		tail_event = head_event;
	} else {
		tail_event->next = cur_event;
		tail_event = tail_event->next;
	}
	spin_unlock(&event_lock);
	printk("hehehehe");
	return 0;
}
struct event *find_event_struct(int event_id)
{
	struct event *cur = head_event;
	
	while(cur != NULL) {
		if (cur->id != event_id)
			cur = cur->next;
		else
			break;
	}
	return cur;
}
int accevt_wait(int event_id)
{
	struct event *cur;
	wait_queue_t wait;

	/* find event struct based on event_id*/
	spin_lock(&event_lock);
	cur = find_event_struct(event_id);
	if (cur == NULL) {
		spin_unlock(&event_lock);
		return -EINVAL;
	}
	printk("accevt_wait: the first lock\n");
	spin_unlock(&event_lock);
	/* count the number of processes that will be put into waitqueue */
	mutex_lock(&(cur->process_lock));
	cur->wait_proc_count += 1;
	mutex_unlock(&(cur->process_lock));
	printk("cur->wait_proc_count:%d\n",cur->wait_proc_count);
	/* put process into waitqueue q */
	init_wait(&wait);
	add_wait_queue(&(cur->q), &wait);
	while(!cur->condition) {
		prepare_to_wait(&(cur->q), &wait, TASK_INTERRUPTIBLE);
		schedule();
	}
	finish_wait(&(cur->q), &wait);
	printk("wake up\n");
	/* check if the event will be destroyed immediately when the 
	 * process is waiting for the event to happen
	*/
	spin_lock(&event_lock);
	if (cur->id == -1) {
		spin_unlock(&event_lock);
		return -EINVAL;
	}
	spin_unlock(&event_lock);
	
	/* if the event is not being destroyed, then the function shoud
	 * return 0 and the woken-up process should print sth and the 
	 * number of processes in the waitqueue q should be decreased by 1*/
	mutex_lock(&(cur->process_lock));
	cur->wait_proc_count -= 1;
	if (cur->wait_proc_count == 0)
		cur->condition = 0;
	mutex_unlock(&(cur->process_lock));
	return 0;
}
int accevt_signal(struct dev_acceleration __user *acceleration)
{
	int res;
	struct dev_acceleration *k_acce;
	struct acceleration_xyz *temp, *to_add;
	struct event *ptr;

	//printk("in the syscall 252\n");
	k_acce = kmalloc(sizeof(struct dev_acceleration), GFP_KERNEL);
	if (k_acce == NULL)
		return -ENOMEM;
	res = copy_from_user(k_acce, acceleration,
				sizeof(struct dev_acceleration));
	if (res != 0)
		return -EFAULT;
	to_add = kmalloc(sizeof(struct acceleration_xyz), GFP_KERNEL);
	if (to_add == NULL) {
		kfree(k_acce);
		return -ENOMEM;
	}
	to_add->xyz = k_acce;
	to_add->next = NULL;
	spin_lock(&acceleration_xyz_lock);
	if (head == NULL) {
		head = to_add;
		tail = head;
	} else {
		tail->next = to_add;
		tail = tail->next;
	}
	xyz_len++;
	/* delete the first acceleration_xyz result 
	 * and let xyz_len = WINDOW 
	*/
	if (xyz_len > WINDOW) {
		temp = head;
		head = head->next;
		temp->next = NULL;
		kfree(temp->xyz);
		kfree(temp);
		xyz_len = WINDOW;
	}
	/* test result */
/*	temp = head;
	while (temp != NULL) {
		printk("x=%d,y=%d,z=%d\n",temp->xyz->x,temp->xyz->y,temp->xyz->z);
		temp = temp->next;
	}*/

	/* check if some event happen when the event list is not empty 
	 * loop each event struct, call the check_window() function
	 * return 1 if a event should happen else return 0 
	 */
	spin_lock(&event_lock);
	if (head_event != NULL) {
		ptr = head_event;
		while(ptr != NULL) {
			printk("loop events\n");
			res = check_window(head, ptr);
			printk("res = %d\n", res);
			if ( res == 1) {
				/* invoke signal */
				printk("sen signal\n");
				ptr->condition = 1;
				wake_up(&(ptr->q));
			}
			ptr = ptr->next;
		}
	}
	printk("go out of the lock");
	spin_unlock(&event_lock);
	spin_unlock(&acceleration_xyz_lock);
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
