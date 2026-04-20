#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define DEFAULT_CHILDREN 4
#define DEFAULT_CHUNK_MB 64UL
#define MAX_CHILDREN 16
#define MAX_LINE 512
#define CHILD_NAME_LEN 32
#define PAGE_SZ 4096UL

enum child_cmd {
	CMD_TOUCH_NEXT = 'T',
	CMD_EXIT = 'Q',
};

typedef struct {
	pid_t pid;
	int from_child_fd;
	int to_child_fd;
	bool alive;
	bool alloc_done;
	bool touch_done;
	size_t reserved_bytes;
	size_t touched_bytes;
	size_t failed_at_bytes;
	int exit_status;
	char last_msg[160];
} child_state_t;

typedef struct {
	unsigned long mem_total_kb;
	unsigned long mem_available_kb;
	unsigned long swap_total_kb;
	unsigned long commit_limit_kb;
	unsigned long committed_as_kb;
	int overcommit_memory;
	unsigned long overcommit_ratio;
	unsigned long overcommit_kbytes;
} meminfo_t;

static volatile sig_atomic_t g_sigchld = 0;

static void sigchld_handler(int signo) {
	(void)signo;
	g_sigchld = 1;
}

static void die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

static void set_nonblock(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) die("fcntl(F_GETFL) failed: %s", strerror(errno));
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		die("fcntl(F_SETFL) failed: %s", strerror(errno));
	}
}

static unsigned long read_ulong_from_file(const char *path, unsigned long defval) {
	FILE *f = fopen(path, "r");
	if (!f) return defval;
	unsigned long v = defval;
	if (fscanf(f, "%lu", &v) != 1) v = defval;
	fclose(f);
	return v;
}

static int read_int_from_file(const char *path, int defval) {
	FILE *f = fopen(path, "r");
	if (!f) return defval;
	int v = defval;
	if (fscanf(f, "%d", &v) != 1) v = defval;
	fclose(f);
	return v;
}

static bool read_meminfo(meminfo_t *mi) {
	memset(mi, 0, sizeof(*mi));

	FILE *f = fopen("/proc/meminfo", "r");
	if (!f) return false;

	char key[64];
	unsigned long value;
	char unit[32];

	while (fscanf(f, "%63[^:]: %lu %31s\n", key, &value, unit) == 3) {
		if (strcmp(key, "MemTotal") == 0) mi->mem_total_kb = value;
		else if (strcmp(key, "MemAvailable") == 0) mi->mem_available_kb = value;
		else if (strcmp(key, "SwapTotal") == 0) mi->swap_total_kb = value;
		else if (strcmp(key, "CommitLimit") == 0) mi->commit_limit_kb = value;
		else if (strcmp(key, "Committed_AS") == 0) mi->committed_as_kb = value;
	}
	fclose(f);

	mi->overcommit_memory = read_int_from_file("/proc/sys/vm/overcommit_memory", -1);
	mi->overcommit_ratio = read_ulong_from_file("/proc/sys/vm/overcommit_ratio", 0);
	mi->overcommit_kbytes = read_ulong_from_file("/proc/sys/vm/overcommit_kbytes", 0);
	return true;
}

static const char *policy_name(int p) {
	switch (p) {
		case 0: return "heuristic";
		case 1: return "always";
		case 2: return "strict";
		default: return "unknown";
	}
}

static double bytes_to_gib(size_t b) {
	return (double)b / (1024.0 * 1024.0 * 1024.0);
}

static double kb_to_gib(unsigned long kb) {
	return (double)kb / (1024.0 * 1024.0);
}

static void safe_write_log(int fd, const char *fmt, ...) {
	char buf[MAX_LINE];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0) return;
	if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
	(void)write(fd, buf, (size_t)n);
}

static void touch_chunk(uint8_t *base, size_t len, uint8_t pattern) {
	for (size_t off = 0; off < len; off += PAGE_SZ) {
		base[off] = pattern;
	}
	if (len > 0) base[len - 1] = pattern;
}

static void child_main(int idx, int read_fd, int log_fd, size_t mem_total_bytes, size_t chunk_bytes, size_t reserve_goal_bytes) {
	char pname[CHILD_NAME_LEN];
	snprintf(pname, sizeof(pname), "ovc-child-%d", idx);
#ifdef PR_SET_NAME
	prctl(PR_SET_NAME, (unsigned long)pname, 0, 0, 0);
#endif

	size_t capacity = reserve_goal_bytes / chunk_bytes + 8;
	uint8_t **regions = calloc(capacity, sizeof(*regions));
	if (!regions) {
		safe_write_log(log_fd, "ALLOC_DONE child=%d status=calloc_failed errno=%d\n", idx, errno);
		_exit(2);
	}

	size_t reserved = 0;
	size_t region_count = 0;

	safe_write_log(log_fd,
				   "START child=%d pid=%ld target_bytes=%zu chunk_bytes=%zu\n",
				   idx, (long)getpid(), reserve_goal_bytes, chunk_bytes);

	while (reserved < reserve_goal_bytes) {
		void *p = mmap(NULL,
					   chunk_bytes,
					   PROT_READ | PROT_WRITE,
					   MAP_PRIVATE | MAP_ANONYMOUS,
					   -1,
					   0);
		if (p == MAP_FAILED) {
			safe_write_log(log_fd,
						   "ALLOC_FAIL child=%d reserved_bytes=%zu errno=%d err=%s\n",
						   idx, reserved, errno, strerror(errno));
			break;
		}

		if (region_count >= capacity) {
			size_t newcap = capacity * 2;
			uint8_t **tmp = realloc(regions, newcap * sizeof(*regions));
			if (!tmp) {
				safe_write_log(log_fd,
							   "ALLOC_FAIL child=%d reserved_bytes=%zu errno=%d err=realloc_failed\n",
							   idx, reserved, ENOMEM);
				break;
			}
			regions = tmp;
			capacity = newcap;
		}

		regions[region_count++] = (uint8_t *)p;
		reserved += chunk_bytes;
		safe_write_log(log_fd,
					   "ALLOC_OK child=%d reserved_bytes=%zu regions=%zu addr=%p\n",
					   idx, reserved, region_count, p);
	}

	safe_write_log(log_fd,
				   "ALLOC_DONE child=%d reserved_bytes=%zu regions=%zu\n",
				   idx, reserved, region_count);

	size_t touched_regions = 0;
	while (1) {
		char cmd;
		ssize_t n = read(read_fd, &cmd, 1);
		if (n == 0) break;
		if (n < 0) {
			if (errno == EINTR) continue;
			safe_write_log(log_fd, "READ_CMD_ERR child=%d errno=%d err=%s\n", idx, errno, strerror(errno));
			break;
		}

		if (cmd == CMD_EXIT) {
			safe_write_log(log_fd,
						   "EXIT child=%d touched_regions=%zu touched_bytes=%zu\n",
						   idx, touched_regions, touched_regions * chunk_bytes);
			break;
		}
		if (cmd != CMD_TOUCH_NEXT) continue;

		if (touched_regions >= region_count) {
			safe_write_log(log_fd,
						   "TOUCH_DONE child=%d touched_regions=%zu touched_bytes=%zu\n",
						   idx, touched_regions, touched_regions * chunk_bytes);
			continue;
		}

		uint8_t pattern = (uint8_t)(0xA0 + (idx & 0x0F));
		touch_chunk(regions[touched_regions], chunk_bytes, pattern);
		touched_regions++;
		safe_write_log(log_fd,
					   "TOUCH_OK child=%d touched_regions=%zu touched_bytes=%zu\n",
					   idx, touched_regions, touched_regions * chunk_bytes);
	}

	close(read_fd);
	close(log_fd);
	_exit(0);
}

static bool parse_field_size(const char *line, const char *field, size_t *out) {
	const char *p = strstr(line, field);
	if (!p) return false;
	p += strlen(field);
	unsigned long long v = 0;
	if (sscanf(p, "%llu", &v) != 1) return false;
	*out = (size_t)v;
	return true;
}

static int parse_field_int(const char *line, const char *field, int *out) {
	const char *p = strstr(line, field);
	if (!p) return 0;
	p += strlen(field);
	int v = 0;
	if (sscanf(p, "%d", &v) != 1) return 0;
	*out = v;
	return 1;
}

static void consume_child_line(child_state_t *cs, const char *line) {
	snprintf(cs->last_msg, sizeof(cs->last_msg), "%s", line);

	size_t val;
	int ival;

	if (strncmp(line, "ALLOC_OK", 8) == 0 || strncmp(line, "ALLOC_DONE", 10) == 0) {
		if (parse_field_size(line, "reserved_bytes=", &val)) cs->reserved_bytes = val;
		if (strncmp(line, "ALLOC_DONE", 10) == 0) cs->alloc_done = true;
	}
	if (strncmp(line, "ALLOC_FAIL", 10) == 0) {
		if (parse_field_size(line, "reserved_bytes=", &val)) cs->failed_at_bytes = val;
		cs->alloc_done = true;
	}
	if (strncmp(line, "TOUCH_OK", 8) == 0 || strncmp(line, "TOUCH_DONE", 10) == 0) {
		if (parse_field_size(line, "touched_bytes=", &val)) cs->touched_bytes = val;
		if (strncmp(line, "TOUCH_DONE", 10) == 0) cs->touch_done = true;
	}
	if (strncmp(line, "EXIT", 4) == 0) {
		cs->touch_done = true;
	}
	if (parse_field_int(line, "child=", &ival)) {
		(void)ival;
	}
}

static void render_screen(const child_state_t *children, int nchildren, size_t target_total_bytes, size_t reserve_goal_bytes_per_child) {
	meminfo_t mi;
	read_meminfo(&mi);

	size_t reserved_total = 0;
	size_t touched_total = 0;
	int alive = 0;

	printf("\033[H\033[J");
	printf("Linux overcommit PoC\n");
	printf("policy=%d (%s)  MemTotal=%.2f GiB  MemAvailable=%.2f GiB  Swap=%.2f GiB\n",
		   mi.overcommit_memory,
		   policy_name(mi.overcommit_memory),
		   kb_to_gib(mi.mem_total_kb),
		   kb_to_gib(mi.mem_available_kb),
		   kb_to_gib(mi.swap_total_kb));
	printf("CommitLimit=%.2f GiB  Committed_AS=%.2f GiB  overcommit_ratio=%lu  overcommit_kbytes=%lu\n",
		   kb_to_gib(mi.commit_limit_kb),
		   kb_to_gib(mi.committed_as_kb),
		   mi.overcommit_ratio,
		   mi.overcommit_kbytes);
	printf("Goal: each child reserves %.2f GiB, total reserve target %.2f GiB\n",
		   bytes_to_gib(reserve_goal_bytes_per_child), bytes_to_gib(target_total_bytes));
	printf("%-7s %-8s %-7s %-12s %-12s %-8s %s\n",
		   "child", "pid", "alive", "reserved", "touched", "status", "last event");
	printf("-------------------------------------------------------------------------------------------------------------\n");

	for (int i = 0; i < nchildren; i++) {
		const child_state_t *c = &children[i];
		reserved_total += c->reserved_bytes;
		touched_total += c->touched_bytes;
		if (c->alive) alive++;
		printf("%-7d %-8ld %-7s %-12.2f %-12.2f %-8s %s\n",
			   i,
			   (long)c->pid,
			   c->alive ? "yes" : "no",
			   bytes_to_gib(c->reserved_bytes),
			   bytes_to_gib(c->touched_bytes),
			   c->alloc_done ? (c->touch_done ? "done" : "alloc") : "allocing",
			   c->last_msg[0] ? c->last_msg : "-");
	}

	printf("\nReserved total: %.2f GiB / %.2f GiB	Touched total: %.2f GiB	Alive: %d/%d\n",
		   bytes_to_gib(reserved_total), bytes_to_gib(target_total_bytes), bytes_to_gib(touched_total), alive, nchildren);
	printf("\nRun strict mode with:   sudo sysctl vm.overcommit_memory=2\n");
	printf("Run permissive mode with: sudo sysctl vm.overcommit_memory=1   (or 0 heuristic)\n");
	printf("Restore previous value after the test. Prefer a VM/cgroup: touching memory can trigger the OOM killer.\n");
	fflush(stdout);
}

static void reap_children(child_state_t *children, int nchildren) {
	int status;
	pid_t pid;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		for (int i = 0; i < nchildren; i++) {
			if (children[i].pid == pid) {
				children[i].alive = false;
				children[i].exit_status = status;
				if (WIFSIGNALED(status)) {
					snprintf(children[i].last_msg, sizeof(children[i].last_msg),
							 "gone: signal=%d", WTERMSIG(status));
				} else if (WIFEXITED(status)) {
					snprintf(children[i].last_msg, sizeof(children[i].last_msg),
							 "gone: exit=%d", WEXITSTATUS(status));
				}
				if (children[i].from_child_fd >= 0) {
					close(children[i].from_child_fd);
					children[i].from_child_fd = -1;
				}
				if (children[i].to_child_fd >= 0) {
					close(children[i].to_child_fd);
					children[i].to_child_fd = -1;
				}
				break;
			}
		}
	}
	g_sigchld = 0;
}

int main(int argc, char **argv) {
	int nchildren = DEFAULT_CHILDREN;
	size_t chunk_bytes = DEFAULT_CHUNK_MB * 1024UL * 1024UL;

	if (argc > 1) nchildren = atoi(argv[1]);
	if (argc > 2) chunk_bytes = (size_t)strtoull(argv[2], NULL, 10) * 1024UL * 1024UL;
	if (nchildren < 4) nchildren = 4;
	if (nchildren > MAX_CHILDREN) nchildren = MAX_CHILDREN;
	if (chunk_bytes < PAGE_SZ) chunk_bytes = PAGE_SZ;

	meminfo_t mi;
	if (!read_meminfo(&mi)) die("failed to read /proc/meminfo");

	size_t mem_total_bytes = (size_t)mi.mem_total_kb * 1024UL;
	size_t reserve_goal_bytes_per_child = mem_total_bytes / 4;
	size_t target_total_bytes = reserve_goal_bytes_per_child * (size_t)nchildren;

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, NULL) < 0) die("sigaction failed: %s", strerror(errno));

	child_state_t children[MAX_CHILDREN];
	memset(children, 0, sizeof(children));
	for (int i = 0; i < nchildren; i++) {
		children[i].from_child_fd = -1;
		children[i].to_child_fd = -1;
	}

	for (int i = 0; i < nchildren; i++) {
		int p2c[2], c2p[2];
		if (pipe(p2c) < 0 || pipe(c2p) < 0) die("pipe failed: %s", strerror(errno));

		pid_t pid = fork();
		if (pid < 0) die("fork failed: %s", strerror(errno));

		if (pid == 0) {
			close(p2c[1]);
			close(c2p[0]);
			child_main(i, p2c[0], c2p[1], mem_total_bytes, chunk_bytes, reserve_goal_bytes_per_child);
		}

		close(p2c[0]);
		close(c2p[1]);
		set_nonblock(c2p[0]);

		children[i].pid = pid;
		children[i].from_child_fd = c2p[0];
		children[i].to_child_fd = p2c[1];
		children[i].alive = true;
		snprintf(children[i].last_msg, sizeof(children[i].last_msg), "spawned");
	}

	char linebuf[MAX_CHILDREN][MAX_LINE];
	size_t linepos[MAX_CHILDREN];
	memset(linepos, 0, sizeof(linepos));

	bool all_alloc_done = false;
	bool touch_phase = false;
	int next_touch_child = 0;
	time_t last_touch = 0;

	while (1) {
		fd_set rfds;
		FD_ZERO(&rfds);
		int maxfd = -1;

		for (int i = 0; i < nchildren; i++) {
			if (children[i].alive && children[i].from_child_fd >= 0) {
				FD_SET(children[i].from_child_fd, &rfds);
				if (children[i].from_child_fd > maxfd) maxfd = children[i].from_child_fd;
			}
		}

		struct timeval tv = {.tv_sec = 0, .tv_usec = 250000};
		int rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);
		if (rc < 0 && errno != EINTR) die("select failed: %s", strerror(errno));

		if (g_sigchld) reap_children(children, nchildren);

		for (int i = 0; i < nchildren; i++) {
			if (!children[i].alive || children[i].from_child_fd < 0) continue;
			if (rc > 0 && FD_ISSET(children[i].from_child_fd, &rfds)) {
				char buf[256];
				ssize_t n = read(children[i].from_child_fd, buf, sizeof(buf));
				if (n == 0) {
					close(children[i].from_child_fd);
					children[i].from_child_fd = -1;
					continue;
				}
				if (n < 0) {
					if (errno == EAGAIN || errno == EINTR) continue;
					close(children[i].from_child_fd);
					children[i].from_child_fd = -1;
					continue;
				}

				for (ssize_t j = 0; j < n; j++) {
					char ch = buf[j];
					if (ch == '\n') {
						linebuf[i][linepos[i]] = '\0';
						consume_child_line(&children[i], linebuf[i]);
						linepos[i] = 0;
					} else if (linepos[i] + 1 < sizeof(linebuf[i])) {
						linebuf[i][linepos[i]++] = ch;
					}
				}
			}
		}

		all_alloc_done = true;
		bool anyone_alive = false;
		bool any_touchable = false;
		for (int i = 0; i < nchildren; i++) {
			if (children[i].alive) anyone_alive = true;
			if (!children[i].alloc_done) all_alloc_done = false;
			if (children[i].alive && children[i].reserved_bytes > children[i].touched_bytes) any_touchable = true;
		}

		if (all_alloc_done) touch_phase = true;

		time_t now = time(NULL);
		if (touch_phase && any_touchable && now != last_touch) {
			for (int tries = 0; tries < nchildren; tries++) {
				int idx = (next_touch_child + tries) % nchildren;
				if (!children[idx].alive) continue;
				if (children[idx].reserved_bytes <= children[idx].touched_bytes) continue;
				char cmd = CMD_TOUCH_NEXT;
				ssize_t w = write(children[idx].to_child_fd, &cmd, 1);
				if (w == 1) {
					snprintf(children[idx].last_msg, sizeof(children[idx].last_msg), "parent: touch requested");
					next_touch_child = (idx + 1) % nchildren;
					last_touch = now;
				}
				break;
			}
		}

		render_screen(children, nchildren, target_total_bytes, reserve_goal_bytes_per_child);

		if (!anyone_alive) break;
		if (touch_phase && !any_touchable) {
			for (int i = 0; i < nchildren; i++) {
				if (children[i].alive && children[i].to_child_fd >= 0) {
					char cmd = CMD_EXIT;
					(void)write(children[i].to_child_fd, &cmd, 1);
				}
			}
		}
	}

	printf("\nAll children are gone. End of test.\n");
	return 0;
}
