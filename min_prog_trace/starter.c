#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <sleep_seconds> <program_to_exec> [args_for_program]\n", argv[0]);
		return 1;
	}

	pid_t pid = getpid();
	printf("Process ID: %d\n", pid);

	int sleep_time = atoi(argv[1]);
	printf("Sleeping for %d seconds...\n", sleep_time);
	sleep(sleep_time);

	printf("Executing %s...\n", argv[2]);
	execvp(argv[2], &argv[2]);

	perror("execvp failed");
	return 1;
}
