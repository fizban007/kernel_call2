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
static DEFINE_SPINLOCK(set_acceleration_lock);


struct acceleration_xyz {
	struct dev_acceleration *xyz;
	struct acceleration_xyz *next;
};

struct event {
	struct acc_motion *baseline;
	int id;
	bool condition; /* true if wake up false if put process to wait */
	bool destroy; /* true if accevt_destroy(event_id) is called */
	wait_queue_head_t q;
//	rwlock_t wake_up_num_lock;
//	rwlock_t destroy_lock;
//	rwlock_t wait_proc_count_lock;
	struct mutex queue_lock;
	int wait_proc_count;
	int wake_up_num;
	struct event *next;
	
};
struct acceleration_xyz *head = NULL;
struct acceleration_xyz *tail = NULL;
int xyz_len = 0;

struct event *head_event = NULL;
struct event *tail_event = NULL;
int num_event = 0;

struct dev_acceleration dev_acc;

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

	spin_lock(&set_acceleration_lock);
	dev_acc.x = k_acceleration->x;
	dev_acc.y = k_acceleration->y;
	dev_acc.z = k_acceleration->z;
	spin_unlock(&set_acceleration_lock);
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
	if (delete == NULL || delete->destroy == true) {
		spin_unlock(&event_lock);
		return -EINVAL;
	} else {

		if (delete->id != head_event->id) {
			prev = head_event;
			while (prev->next != delete)
				prev = prev->next;
			prev->next = delete->next;
		} else
			head_event = delete->next;
		delete->next = NULL;
	}
	spin_unlock(&event_lock);
	mutex_lock(&(delete->queue_lock));
	if (delete->wait_proc_count != 0) {
		delete->destroy = true;
		delete->condition = true;
		delete->wake_up_num = delete->wait_proc_count;
		wake_up(&(delete->q));
	
		while(delete->wait_proc_count != 0) {
			mutex_unlock(&(delete->queue_lock));
			mutex_lock(&(delete->queue_lock));
		}
	}
	mutex_unlock(&(delete->queue_lock));
	
	kfree(delete->baseline);
	kfree(delete);	
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
	new_event->wake_up_num = 0;
	init_waitqueue_head(&(new_event->q));
//	rwlock_init(&(new_event->destroy_lock));
//	rwlock_init(&(new_event->condition_lock));
//	rwlock_init(&(new_event->wait_proc_count_lock));
	mutex_init(&(new_event->queue_lock));
	//rwlock_init(&(new_event->wake_up_num_lock));
	/* put the new_event to the linked list "events" 
	 * let num_event plus 1 */
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

	printk("event_id: %d\n", event_id);
	init_wait(&wait);
	/* find event struct based on event_id 
	 * and check if the process should be pushed to waitqueue*/
	spin_lock(&event_lock);
	cur = find_event_struct(event_id);
	if (cur == NULL) {
		spin_unlock(&event_lock);
		return -EINVAL;
	}
	printk("accevt_wait: finish the first lock\n");
	spin_unlock(&event_lock);

	/* count the number of processes that will be put into waitqueue */
	mutex_lock(&(cur->queue_lock));
	printk("cur->wait_proc_count:%d\n",cur->wait_proc_count);
	/* put process into waitqueue q */
	while (cur->wake_up_num != 0) {
		mutex_unlock(&(cur->queue_lock));
		mutex_lock(&(cur->queue_lock));
	}
	if(cur->destroy == true) {
		mutex_unlock(&(cur->queue_lock));
		return -EINVAL;
	}
	cur->wait_proc_count += 1;

	add_wait_queue(&(cur->q), &wait);
	while(!cur->condition) {
		prepare_to_wait(&(cur->q), &wait, TASK_INTERRUPTIBLE);
		mutex_unlock(&(cur->queue_lock));
		schedule();
		mutex_lock(&(cur->queue_lock));
		cur->wake_up_num--;
		cur->wait_proc_count--;
	}
	finish_wait(&(cur->q), &wait);
	printk("wake up\n");
	if (cur->destroy == true)
		return -EINVAL;
	if (cur->wait_proc_count == 0)
		cur->condition = false;

	mutex_unlock(&(cur->queue_lock));
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
	if (new_xyz == NULL)
		return -ENOMEM;
	res = copy_from_user(new_xyz, acceleration,
				sizeof(struct dev_acceleration));
	if (res != 0)
		return -EFAULT;
	to_add = kmalloc(sizeof(struct acceleration_xyz), GFP_KERNEL);
	if (to_add == NULL) {
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
	spin_lock(&event_lock);
	if (head_event != NULL) {/* check if the event linked list is empty */
		ptr = head_event;
		while(ptr != NULL) {
			res = check_window(head, ptr);
			if ( res == 1) {
				/* invoke signal */
				printk("send signal res = %d\n", res);
				mutex_lock(&(ptr->queue_lock));
				if (ptr->wait_proc_count != 0) {
					ptr->condition = true;
					ptr->wake_up_num = 
						ptr->wait_proc_count;
					wake_up(&(ptr->q));
				}
				mutex_unlock(&(ptr->queue_lock));
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
