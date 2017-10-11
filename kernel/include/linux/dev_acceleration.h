#ifndef _DEV_ACCELERATION_H
#define _DEV_ACCELERATION_H

struct dev_acceleration {
	/* acceleration along each axis */
	int x;
	int y;
	int z;
}

int set_acceleration(struct dev_acceleration __user *acceleration);
int accevt_signal(struct dev_acceleration __user *acceleration);

#endif
