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
	bool condition; /* true if wake up false if put process to wait */
	bool destroy; /* true if accevt_destroy(event_id) is called */
	wait_queue_head_t q;
	rwlock_t process_lock;
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

int accevt_destroy(int event_id)
{
	struct event *delete=NULL, *prev=NULL;
	spin_lock(&event_lock);
	delete = find_event_struct(event_id);
	if (delete == NULL) {
		spin_unlock(&event_lock);
		return -EINVAL;
	}
	if (delete->destroy == true) {
		/* some process has already called accevt_destroy() for this event
		 * hence for this process we just return -EINVAL to show nonexistence*/
		spin_unlock(&event_lock);
		return -EINVAL;
	} else {
		write_lock(&(cur->process_lock));
		cur->destroy = true;
		write_unlock(&(cur->process_lock));
		if (cur->id != head->id) {
			prev = head;
			while (prev->next != cur)
				prev = prev->next;
		}
	}
	spin_unlock(&event_lock);
	write_lock(&(cur->process_lock));
	if (cur->wait_proc_count != 0) {
		cur->condition = true;
		write_unlock(&(cur->process_lock));
		wake_up(cur->q);
	}
	read_lock(&(cur->process_lock));
	while(cur->wait_proc_count != 0) {
		read_unlock(&(cur->process_lock));
		read_lock(&(cur->process_lock));
	}
	spin_lock(&event_lock);
	if (prev == NULL) {
		head = head->next;
		cur->next = NULL;
		kfree(cur->baseline);
		kfree(cur);
	} else {
		pre->next = cur->next;
		cur->next = NULL;
		kfree(cur->baseline);
		kfree(cur);	
	}	
	spin_unlock(&event_lock);
	return 0;
}

int accevt_create(struct acc_motion __user *acceleration)
{
	struct acc_motion *new_baseline;
	struct event *new_event;
	int res;

	printk("in the syscall accevt_create\n");
	
	new_baseline = kmalloc(sizeof(struct acc_motion), GFP_KERNEL);
	if(new_baseline == NULL)
		/* kmalloc error */
		return -ENOMEM;
	res = copy_from_user(new_baseline, acceleration, 
				sizeof(struct acc_motion));
	if (res != 0)
		return -EFAULT;
	new_baseline->frq = WINDOW <= new_baseline->frq ? WINDOW - 1 : new_baseline->frq;
	new_event = kmalloc(sizeof(struct event), GFP_KERNEL);
	if (new_event == NULL) {
		/* kmalloc error */
		kfree(new_baseline);
		return -ENOMEM;
	}
	new_event->next = NULL;
	new_event->id = num_event;
	new_event->baseline = new_baseline;
	new_event->condition = false;
	new_event->destroy = false;
	new_event->wait_proc_count = 0;
	init_waitqueue_head(&(new_event->q));
	rwlock_init(&(new_event->process_lock));

	/* put the new_event to the linked list "events" and let num_event plus 1 */
	spin_lock(&event_lock);
	num_event++;
	if (head_event == NULL) {
		head_event = new_event;
		tail_event = head_event;
	} else {
		tail_event->next = new_event;
		tail_event = tail_event->next;
	}
	spin_unlock(&event_lock);
	printk("finish create a new event \n");
	return 0;
}

int accevt_wait(int event_id)
{
	struct event *cur;
	wait_queue_t wait;

	/* find event struct based on event_id 
	 * and check if the process should be pushed to waitqueue*/
	spin_lock(&event_lock);
	cur = find_event_struct(event_id);
	if (cur == NULL || cur->destroy == true) {
		/* if the event doesn't exist or is ready to be destroyed, then the process
		 * shouldn't go to the wait queue and just return -EINVAL */
		spin_unlock(&event_lock);
		return -EINVAL;
	}
	printk("accevt_wait: finish the first lock\n");
	spin_unlock(&(cur->process_lock));

	/* count the number of processes that will be put into waitqueue */
	write_lock(&(cur->process_lock));
	cur->wait_proc_count += 1;
	printk("cur->wait_proc_count:%d\n",cur->wait_proc_count);
	write_unlock(&(cur->process_lock));
	/* put process into waitqueue q */
	/* all the wait functions contains locks, we don't need to lock before using these
	 * functions */
	init_wait(&wait);
	add_wait_queue(&(cur->q), &wait);
	while(!cur->condition) {
		prepare_to_wait(&(cur->q), &wait, TASK_INTERRUPTIBLE);
		schedule();
	}
	finish_wait(&(cur->q), &wait);
	printk("wake up\n");
	/* check if the event will be destroyed immediately when the 
	 * process is waiting for the event to happen. Before being destroyed, the event id
	 * will be changed to -1 in the destroy function, hence we can use the id to check.
	*/
	write_lock(&(cur->process_lock));
	cur->wait_proc_count -= 1;
	if (cur->destroy) {
		write_unlock(&(cur->process_lock));
		return -EINVAL;
	}	
	/* if the event is triggered by accevt_destroy(), then the function shoud
	 * return 0 and the woken-up process should print sth and the 
	 * number of processes in the waitqueue q should be decreased by 1*/
	if (cur->wait_proc_count == 0)
		cur->condition = false;
	write_unlock(&(cur->process_lock));
	return 0;
}
int accevt_signal(struct dev_acceleration __user *acceleration)
{
	int res;
	struct dev_acceleration *new_xyz;
	struct acceleration_xyz *delete, *to_add;
	struct event *ptr;

	//printk("in the syscall 252\n");
	new_xyz = kmalloc(sizeof(struct dev_acceleration), GFP_KERNEL);
	if (new_xyz == NULL) /* kmalloc error */
		return -ENOMEM;
	res = copy_from_user(new_xyz, acceleration,
				sizeof(struct dev_acceleration));
	if (res != 0) /* copy error */
		return -EFAULT;
	to_add = kmalloc(sizeof(struct acceleration_xyz), GFP_KERNEL);
	if (to_add == NULL) { /* kmalloc error again */
		kfree(new_xyz);
		return -ENOMEM;
	}
	to_add->xyz = new_xyz;
	to_add->next = NULL;

	/* add the to_add variable to linked list */
	spin_lock(&acceleration_xyz_lock);
	if (head == NULL) {
		head = to_add;
		tail = head;
	} else {
		tail->next = to_add;
		tail = tail->next;
	}
	xyz_len++;
	/* delete the first acceleration_xyz 
	 * and let xyz_len = WINDOW 
	*/
	if (xyz_len > WINDOW) {
		delete = head;
		head = head->next;
		delete->next = NULL;
		kfree(delete->xyz);
		kfree(delete);
		xyz_len = WINDOW;
	}
	spin_unlock(&acceleration_xyz_lock);
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
	spin_lock(&acceleration_xyz_lock);
	spin_lock(&event_lock);
	if (head_event != NULL) {/* check if the event linked list is empty */
		ptr = head_event;
		while(ptr != NULL) {
			res = check_window(head, ptr);
			printk("res = %d\n", res);
			if ( res == 1) {
				/* invoke signal */
				printk("sen signal\n");
				write_lock(&(ptr->process_lock));
				ptr->condition = true;
				write_unlock(&(ptr->process_lock));
				wake_up(&(ptr->q));
			}
			ptr = ptr->next;
		}
	}
	printk("signal: go out of the lock\n");
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
