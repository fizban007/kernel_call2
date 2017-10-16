#include<stdio.h>
#define _GNU_SOURCE
#include<unistd.h>
#include<sys/syscall.h>
#include<sys/types.h>
struct acc_motion {
	unsigned int dlt_x;
	unsigned int dlt_y;
	unsigned int dlt_z;
	unsigned int frq;
};

int main(int argc, char **argv)
{
	printf("line 15");
	struct acc_motion vertical, horizontal, both;
	struct acc_motion events[3];
	int res, i;

	vertical.dlt_x = 0;
	vertical.dlt_y = 0;
	vertical.dlt_z = 100;
	vertical.frq = 6;
	horizontal.dlt_x = 100;
	horizontal.dlt_y = 100;
	horizontal.dlt_z = 0;
	horizontal.frq = 10;
	both.dlt_x = 100;
	both.dlt_y = 100;
	both.dlt_z = 100;
	both.frq = 10;

	events[0] = vertical;
	events[1] = horizontal;
	events[2] = both;
	for (i = 0; i < 3; i++) {
		printf("i=%d\n",i);
		res = syscall(250, &events[i]);
		if (res != 0)
			printf("Error: %s", strerror(errno));
	}
	i = 1;
	while(1) {
		res = syscall(251, 0);
		if (res != 0) {
			printf("Error: %s", strerror(errno));
			break;
		}
		printf("%d: %d detected a vertical shake\n", i, getpid());
		i++;
	}

	return 0;


}
