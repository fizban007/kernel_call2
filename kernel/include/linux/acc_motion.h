#ifndef _ACC_MOTION_H
#define _ACC_MOTION_H

struct acc_motion {
	unsigned int dlt_x; /* delta x */
	unsigned int dlt_y; /* delta y */
	unsigned int dlt_z; /* delta z */

	/* number of samples that satisfies:
 	 * dlt_x + dlt_y + dlt_z > NOISE
 	 * frq  = min{frq, WINDOW} 
 	 */
 	unsigned int frq;
}

int accevt_create(struct acc_motion __user *acceleration);
