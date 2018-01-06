#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#define MAX_CPUS 8


struct wrr_info {
	int num_cpus;
	int nr_running[MAX_CPUS];
	int total_weight[MAX_CPUS];
};

int main(int argc, char **argv)
{
	int pid, i, j, tmp = 1;
	struct wrr_info info;

	if (atoi(argv[1]) == 0) {
		syscall(245, atoi(argv[2]));
		printf("Weight set to %d\n", atoi(argv[2]));
		setuid(20000);
		for (i = 0; i < 10; i++) {
			pid = fork();
			if (pid == 0) {
				for (j = 0; j < 1000000000; j++)
					tmp = tmp + 2;
				printf("result: %d\n", tmp);
				return 0;
			}
		}
	} else if (atoi(argv[1]) == 1) {
		setuid(20000);
		for (i = 0; i < 10; i++) {
			pid = fork();
			if (pid == 0) {
				for (j = 0; j < 1000000000; j++)
					tmp = tmp + 2;
				return 0;
			}
		}
		for (i = 0; i < 10; i++) {
			if (syscall(244, &info) < 0)
				return 1;
			printf("cpus:%d\n", info.num_cpus);
			for (j = 0; j < info.num_cpus; j++)
				printf("nr:%d, weight:%d\n",
				info.nr_running[j], info.total_weight[j]);
			sleep(1);
		}
		for (i = 0; i < 10; i++) {
			pid = fork();
			if (pid == 0) {
				for (j = 0; j < 1000000000; j++)
					tmp = tmp + 2;
				return 0;
			}
		}
		for (i = 0; i < 10; i++) {
			if (syscall(244, &info) < 0)
				return 1;
			printf("cpus:%d\n", info.num_cpus);
			for (j = 0; j < info.num_cpus; j++)
				printf("nr:%d, weight:%d\n",
				info.nr_running[j], info.total_weight[j]);
			sleep(1);
		}
		wait(NULL);
	}
	return 0;
}
