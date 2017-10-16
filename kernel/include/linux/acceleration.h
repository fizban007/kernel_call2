#ifndef _ACCELERATION_H_
#define _AACELERATION_H_

struct acc_motion
{
	unsigned int dlt_x;
	unsigned int dlt_y;
	unsigned int dlt_z;
	unsigned int frq;
};

struct dev_acceleration
{
	int x;
	int y;
	int z;
};

/*int set_acceleration(struct dev_acceleration __user *acceleration);
int accevt_destroy(int event_id);
int accevt_create(struct acc_motion __user *acceleration);
int accevt_wait(int event_id);
int accevt_signal(struct dev_acceleration __user *acceleration);
*/
#endif
