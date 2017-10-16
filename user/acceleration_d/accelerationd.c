/*
 * Columbia University
 * COMS W4118 Fall 2017
 * Homework 3
 *
 * Adapted from Fall 2016 solution
 *
 */
#define DEBUG_MODE

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h> /* <-- This is a good place to look! */

#include "accelerationd.h" /* defined dev_acceleration and TIME_INVERVAL*/

static int effective_linaccel_sensor = -1;

/* helper functions which you should use */
static int open_sensors(struct sensors_module_t **hw_module,
			struct sensors_poll_device_t **poll_device);
static void enumerate_sensors(const struct sensors_module_t *sensors);
static struct dev_acceleration *poll_sensor_data_emulator(void);
static int poll_sensor_data(struct sensors_poll_device_t *sensors_device);

void daemon_mode(void)
{
	/* ref: http://www.thegeekstuff.com/2012/02/c-daemon-process/ */
	/* ref: http://www.netzmafia.de/skripten/unix/linux-daemon-howto.html */

	/* 1. fork a child
	 * 2. terminate the parent -> child becomes orphan and taken over by init
	 * 3. change file mode mask
	 * 4. call setsid() to run child in new session and have a new group
	 * 5. change the working dir of the daemon to root
	 * 6. close stdin, stdout and stderr file descriptors
	 * let main logic of daemon process run 
	 */

	pid_t pid, sid;
	/* 1. fork a child */
	pid = fork();
	if(pid < 0) { /* fork failed */
		printf("fork failed in daemon_mode.\n");
		exit(EXIT_FAILURE);
	} else if (pid > 0) { 
		/* 2. Terminate parent process. */
		exit(EXIT_SUCCESS);
	}

/* child process continues here to turn into daemon */
	
	/* 3. change the file mode mask */
	umask(0);
	/* 4. create a new SID for the child process */
	sid = setsid();
	if(sid < 0){
		printf("Error: setsid failed.");
		exit(EXIT_FAILURE);
	}
	/* 5. change the working dir of the daemon to root */
	if((chdir("/")) < 0) {
		printf("Error: chdir failed.");
		exit(EXIT_FAILURE);
	}
	/* 6. close stdin stdout stderr file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	return;
}

int main(int argc, char **argv)
{
	struct sensors_module_t *sensors_module = NULL;
	struct sensors_poll_device_t *sensors_device = NULL;
	int errsv = 0; /* Error that we pull up from system call*/

	if (argv[1] && strcmp(argv[1], "-e") == 0)
		goto emulation;
	/*
	 * TODO: Implement your code to make this process a daemon in
	 * daemon_mode function
	 */
	/*
	daemon_mode();
	*/
	printf("Opening sensors...\n");
	if (open_sensors(&sensors_module,
			 &sensors_device) < 0) {
		printf("open_sensors failed\n");
		return EXIT_FAILURE;
	}

	/* find the sensor ID for accelerometer 
	 * and save in "effective_linaccel_sensor" */
	enumerate_sensors(sensors_module);

	printf("turn me into a daemon!\n");

	while (1) {
emulation:
		errsv = poll_sensor_data(sensors_device);
		if(errsv != 0) {
			printf("Error: poll_sensor_data failed.\n");
			break;
		}
		/* TODO: Define time interval and call usleep */

		int errsl = 0;
		errsl = usleep(TIME_INTERVAL);
		if(errsl != 0) {
			printf("Error: failed to sleep.\n");
			break;
		}
		printf("slept for: %d ms\n", TIME_INTERVAL);
		break; /* to test 249 only */
	}

	return EXIT_SUCCESS;
}


static int poll_sensor_data(struct sensors_poll_device_t *sensors_device)
{
	struct dev_acceleration *cur_acceleration;
	int err = 0; /* Return value of this function*/

	if (effective_linaccel_sensor < 0) {
		/* emulation */
		cur_acceleration = poll_sensor_data_emulator();
		/*
		 * TODO: You have the acceleration here - 
		 * scale it and send it to your kernel
		 */
		
		printf("line 147 reached.\n");
		printf("cur_acceleration->x: %d\n", cur_acceleration->x);
		printf("cur_acceleration->y: %d\n", cur_acceleration->y);
		printf("cur_acceleration->z: %d\n", cur_acceleration->z);
	
		/* err = syscall(252, cur_acceleration);*/
		err = syscall(249, cur_acceleration);
		if(err){
			/*printf("Error: accevt_signal failed.\n");*/
			printf("Error: set_acceleration failed.\n");
		}

	} else {
		/* buffer is an array of 128 sensors_event_t structs */
		sensors_event_t buffer[128];
		ssize_t buf_size = sizeof(buffer)/sizeof(buffer[0]);
		ssize_t count = sensors_device->poll(sensors_device,
						buffer,
						buf_size);
		cur_acceleration = (struct dev_acceleration *)
					malloc(sizeof(struct dev_acceleration));
		if(!cur_acceleration) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			exit(1);
		}
		/*
		 * TODO: You have the acceleration here - scale it and
		 * send it to kernel
		 */

		/* ??? should we cap count by buf_size??? */

		/* find the sensors_event_t of type SENSOR_TYPE_ACCELEROMETER */
		/* sensors_event_t->type == SENSOR_TYPE_ACCELEROMETER */
		/* sensors_event_t->acceleration -> x / y / z */

		while(count > 0 && buffer[count-1].sensor != effective_linaccel_sensor) {
			count--;
		}

		if(count < 0) { /* Failed to find event with accelerometer handler # */
			err = 1;
			printf("Failed to locate sensor using handler in buffer\n");
		} else { /* found accelerometer's event*/
			 /* scale the data by 100, convert to int, and send to kernel */
			cur_acceleration->x = (int) ((buffer[count-1].acceleration.x)*100);
			cur_acceleration->y = (int) ((buffer[count-1].acceleration.y)*100);
			cur_acceleration->z = (int) ((buffer[count-1].acceleration.z)*100);

			#ifdef DEBUG_MODE
			printf("cur_x is %d", cur_acceleration->x);
			printf("cur_y is %d", cur_acceleration->y);
			printf("cur_z is %d", cur_acceleration->z);
			#endif
						
			err = syscall(252, cur_acceleration);
			if(err)
				printf("Error: accevt_signal failed\n");
		}

	}
	//}
	return err;
}



/*  DO NOT MODIFY BELOW THIS LINE  */
/*---------------------------------*/



static struct dev_acceleration *poll_sensor_data_emulator(void)
{
	int cur_x, cur_y, cur_z;
	struct dev_acceleration *ad;

	ad = (struct dev_acceleration *)
			malloc(sizeof(struct dev_acceleration));

	if (!ad) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		exit(1);
	}

	FILE *fp = fopen("/data/misc/acceleration", "r");

	if (!fp) {
		free(ad);
		return 0;
	}
	fscanf(fp, "%d, %d, %d", &cur_x, &cur_y, &cur_z);
	fclose(fp);
	ad->x = cur_x;
	ad->y = cur_y;
	ad->z = cur_z;

	return ad;
}




static int open_sensors(struct sensors_module_t **mSensorModule,
			struct sensors_poll_device_t **mSensorDevice)
{
	int err = hw_get_module(SENSORS_HARDWARE_MODULE_ID,
				(hw_module_t const **)mSensorModule);

	if (err) {
		printf("couldn't load %s module (%s)",
		       SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorModule)
		return -1;

	err = sensors_open(&((*mSensorModule)->common), mSensorDevice);

	if (err) {
		printf("couldn't open device for module %s (%s)",
		       SENSORS_HARDWARE_MODULE_ID, strerror(-err));
	}

	if (!*mSensorDevice)
		return -1;

	const struct sensor_t *list;
	ssize_t count =
		(*mSensorModule)->get_sensors_list(*mSensorModule, &list);
	size_t i;

	for (i = 0 ; i < (size_t)count ; i++)
		(*mSensorDevice)->activate(*mSensorDevice, list[i].handle, 1);
	return 0;
}

static void enumerate_sensors(const struct sensors_module_t *sensors)
{
	int nr, s;
	const struct sensor_t *slist = NULL;

	if (!sensors)
		printf("going to fail\n");

	nr = sensors->get_sensors_list((struct sensors_module_t *)sensors,
				       &slist);
	if (nr < 1 || slist == NULL) {
		printf("no sensors!\n");
		return;
	}

	for (s = 0; s < nr; s++) {
		printf("%s (%s) v%d\n\tHandle:%d, type:%d, max:%0.2f, "
			"resolution:%0.2f \n", slist[s].name, slist[s].vendor,
		       slist[s].version, slist[s].handle, slist[s].type,
		       slist[s].maxRange, slist[s].resolution);

		if (slist[s].type == SENSOR_TYPE_ACCELEROMETER)
			effective_linaccel_sensor = slist[s].handle; /*the sensor ID*/

	}
}
