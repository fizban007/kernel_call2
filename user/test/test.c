#include<stdio.h>
#include<stdlib.h>
#define _GNU_SOURCE
#include<unistd.h>
#include<sys/syscall.h>
#include<sys/types.h>
#include<string.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/wait.h>
#include <errno.h>
#include<pthread.h>
struct acc_motion {
	unsigned int dlt_x;
	unsigned int dlt_y;
	unsigned int dlt_z;
	unsigned int frq;
};
int run_time(struct timeval start)
{
	struct timeval end;
	int res;
	res = gettimeofday(&end, 0);
	if (res) {
		printf("Error: %s\n",strerror(res));
		exit(1);
	}
	return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000;
}
int main(int argc, char **argv)
{
	printf("line 15");
	struct acc_motion vertical, horizontal, both;
	struct acc_motion events[3];
	struct timeval start;
	char state[3][15];
	int res, i;

	vertical.dlt_x = 0;
	vertical.dlt_y = 0;
	vertical.dlt_z = 100;
	vertical.frq = 4;
	horizontal.dlt_x = 100;
	horizontal.dlt_y = 100;
	horizontal.dlt_z = 0;
	horizontal.frq = 4;
	both.dlt_x = 100;
	both.dlt_y = 100;
	both.dlt_z = 100;
	both.frq = 4;

	events[0] = vertical;
	events[1] = horizontal;
	events[2] = both;
	strcpy(state[0], "vertical");
	strcpy(state[1], "horizontal");
	strcpy(state[2], "both");
	for (i = 0; i < 3; i++) {
		res = syscall(250, &events[i]);
		if (res != 0)
			printf("Error: %s", strerror(errno));
	}
	for (i = 0; i < 10; i++) {
		pid_t pid = fork();
		if (pid < 0) {
			printf("Error: %s",strerror(errno));
			exit(1);
		} else if (pid > 0) {
			printf("i=%d\n",i);
			int count = 1;
			while(1) {
				res = syscall(251,i%3);
				if (res != 0) {
					printf("Error: %s\n", 
						strerror(errno));
					exit(1);
				}
				printf("%d: %d a %s shake\n", count, getpid(),state[i%3]);
				count++;
				if (count >= 10) {
					res = syscall(253, i%3);
					if (res != 0)
						exit(1);
					else
						exit(0);
				}
			}
		}
	}
	while(1) {
		if (run_time(start) > 60) {
			res = syscall(253, 0);
			if (res != 0) {
				printf("Error: %s", strerror(errno));
			}
			res = syscall(253, 1);
			if (res != 0) {
				printf("Error: %s", strerror(errno));
			}
			res = syscall(253, 2);
			if (res != 0) {
				printf("Error: %s", strerror(errno));
			}
			break;
		}
	}
	while (wait(NULL) >0);

	return 0;


}
