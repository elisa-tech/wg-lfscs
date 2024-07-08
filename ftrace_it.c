#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

volatile int *semaphore;

void setup_ftrace(pid_t pid) {
	char buffer[256];

	int fd = open("/sys/kernel/tracing/current_tracer", O_WRONLY);
	if (fd < 0) {
		perror("open current_tracer");
		exit(EXIT_FAILURE);
	}
	if (write(fd, "function\n", 9) < 0) {
		perror("write current_tracer");
		close(fd);
		exit(EXIT_FAILURE);
	}
	close(fd);
	fd = open("/sys/kernel/tracing/set_ftrace_pid", O_WRONLY);
	if (fd < 0) {
		perror("open set_ftrace_pid");
		exit(EXIT_FAILURE);
	}
	snprintf(buffer, sizeof(buffer), "%d\n", pid);
	if (write(fd, buffer, strlen(buffer)) < 0) {
		perror("write set_ftrace_pid");
		close(fd);
		exit(EXIT_FAILURE);
	}
	close(fd);
}

void disable_ftrace() {
	int fd = open("/sys/kernel/tracing/current_tracer", O_WRONLY);
	if (fd < 0) {
		perror("open current_tracer");
		exit(EXIT_FAILURE);
	}
	if (write(fd, "nop\n", 4) < 0) {
		perror("write current_tracer");
		close(fd);
		exit(EXIT_FAILURE);
	}
	close(fd);
}

int main(int argc, char *argv[]) {
	char tmp_buff[10];

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <program_to_trace>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	semaphore = mmap(NULL, sizeof(*semaphore), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (semaphore == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	*semaphore = 0;

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid == 0) { // Child process
		while (*semaphore == 0) {
		}

		if (execve(argv[1], &argv[1], NULL) < 0) {
			perror("execve");
			exit(EXIT_FAILURE);
		}
	} else {
		setup_ftrace(pid);
		printf("all set in, press enter to start\n");
		read(0, tmp_buff, 1);

		*semaphore = 1;

		waitpid(pid, NULL, 0);

		disable_ftrace();

		printf("Tracing completed.\n");

		munmap((void *)semaphore, sizeof(*semaphore));
	}

	return 0;
}
