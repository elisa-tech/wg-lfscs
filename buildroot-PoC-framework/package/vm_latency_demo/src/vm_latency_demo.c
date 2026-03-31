#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#define NUM_PAGES   4
#define NUM_SAMPLES 512
#define BAR_WIDTH   50

static inline uint64_t timespec_diff_ns(const struct timespec *start,
					const struct timespec *end) {
	uint64_t s = (uint64_t)(end->tv_sec - start->tv_sec);
	int64_t ns = end->tv_nsec - start->tv_nsec;
	return s * 1000000000ULL + (uint64_t)ns;
}

static size_t get_page_size(void) {
	long ps = sysconf(_SC_PAGESIZE);
	if (ps <= 0) {
		perror("sysconf(_SC_PAGESIZE)");
		exit(EXIT_FAILURE);
	}
	return (size_t)ps;
}

static void *allocate_region(size_t page_size, size_t num_pages) {
	size_t length = page_size * num_pages;
	void *p = mmap(NULL, length,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}
	return p;
}

static void build_offsets(size_t *offsets, size_t num_samples,
			  size_t page_size, size_t num_pages) {
	size_t region_size = page_size * num_pages;
	size_t step = region_size / num_samples;
	if (step == 0) {
		step = 1;
	}

	for (size_t i = 0; i < num_samples; ++i) {
		size_t off = (i * step) % region_size;
		offsets[i] = off;
	}
}

static void run_read_campaign(const volatile unsigned char *region,
			      const size_t *offsets,
			      uint64_t *latencies_ns,
			      uint64_t *usage,
			      size_t num_samples,
			      volatile unsigned char *dummy) {
	struct rusage r1, r2;
	for (size_t i = 0; i < num_samples; ++i) {
		struct timespec t1, t2;

	getrusage(RUSAGE_SELF, &r1);
		if (clock_gettime(CLOCK_MONOTONIC_RAW, &t1) != 0) {
			perror("clock_gettime(before read)");
			exit(EXIT_FAILURE);
		}

		*dummy ^= region[offsets[i]];

		if (clock_gettime(CLOCK_MONOTONIC_RAW, &t2) != 0) {
			perror("clock_gettime(after read)");
			exit(EXIT_FAILURE);
		}
	getrusage(RUSAGE_SELF, &r2);

		latencies_ns[i] = timespec_diff_ns(&t1, &t2);
	usage[i] = r2.ru_nivcsw - r1.ru_nivcsw;
	}
}

static void run_write_campaign(unsigned char *region,
			       const size_t *offsets,
			       uint64_t *latencies_ns,
			       uint64_t *usage,
			       size_t num_samples) {
	struct rusage r1, r2;
	for (size_t i = 0; i < num_samples; ++i) {
		struct timespec t1, t2;

	getrusage(RUSAGE_SELF, &r1);
		if (clock_gettime(CLOCK_MONOTONIC_RAW, &t1) != 0) {
			perror("clock_gettime(before write)");
			exit(EXIT_FAILURE);
		}

		region[offsets[i]] = (unsigned char)(i & 0xff);

		if (clock_gettime(CLOCK_MONOTONIC_RAW, &t2) != 0) {
			perror("clock_gettime(after write)");
			exit(EXIT_FAILURE);
		}
	getrusage(RUSAGE_SELF, &r2);

		latencies_ns[i] = timespec_diff_ns(&t1, &t2);
	usage[i] = r2.ru_nivcsw - r1.ru_nivcsw;
	}
}

static void print_samples(const char *title, const uint64_t *latencies_ns, const uint64_t *usage,
			  const size_t *offsets, size_t num_samples,
			  size_t page_size) {
	uint64_t min = latencies_ns[0];
	uint64_t max = latencies_ns[0];
	uint64_t sum = 0;

	for (size_t i = 0; i < num_samples; ++i) {
		if (latencies_ns[i] < min) min = latencies_ns[i];
		if (latencies_ns[i] > max) max = latencies_ns[i];
		sum += latencies_ns[i];
	}

	double avg = (double)sum / (double)num_samples;

	printf("\n=== %s ===\n", title);
	printf("samples: %zu, min: %" PRIu64 " ns, avg: %.1f ns, max: %" PRIu64 " ns\n",
		   num_samples, min, avg, max);

	for (size_t i = 0; i < num_samples; ++i) {
		size_t bar_len = 0;
		if (max > 0) {
			bar_len = (size_t)((latencies_ns[i] * BAR_WIDTH) / max);
		}
		if (bar_len == 0 && latencies_ns[i] > 0) {
			bar_len = 1;
		}

		size_t page_idx = offsets[i] / page_size;
		size_t page_off = offsets[i] % page_size;

		printf("%3zu | p%zu + %-4zu | %4zu | %8" PRIu64 " ns | ",
			   i, page_idx, page_off, usage[i], latencies_ns[i]);

		for (size_t j = 0; j < bar_len; ++j) {
			putchar('#');
		}
		putchar('\n');
	}
}

int main(int argc, char **argv) {
	size_t num_samples = NUM_SAMPLES;
	int order_read_first = 1;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
			num_samples = strtoull(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			char *opt = argv[++i];
			if (strcmp(opt, "rw") == 0) {
				order_read_first = 1;
			} else if (strcmp(opt, "wr") == 0) {
				order_read_first = 0;
			} else {
				fprintf(stderr, "Invalid order '%s' (use rw or wr)\n", opt);
				return EXIT_FAILURE;
			}
		} else {
			fprintf(stderr, "Usage: %s [-n samples] [-o rw|wr]\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	size_t page_size = get_page_size();
	size_t region_size = page_size * NUM_PAGES;

	printf("System page size : %zu bytes\n", page_size);
	printf("Mapped region	 : %zu pages (%zu bytes)\n", NUM_PAGES, region_size);
	printf("Samples per phase : %zu\n", num_samples);
	printf("Order			 : %s\n", order_read_first ? "read->write" : "write->read");

	unsigned char *region = allocate_region(page_size, NUM_PAGES);

	size_t *offsets = malloc(num_samples * sizeof(size_t));
	uint64_t *read_latencies = malloc(num_samples * sizeof(uint64_t));
	uint64_t *write_latencies = malloc(num_samples * sizeof(uint64_t));
	uint64_t *read_usage = malloc(num_samples * sizeof(uint64_t));
	uint64_t *write_usage = malloc(num_samples * sizeof(uint64_t));

	if (!offsets || !read_latencies || !write_latencies ||
		!read_usage || !write_usage) {
		perror("malloc");
		return EXIT_FAILURE;
	}

	volatile unsigned char dummy = 0;

	build_offsets(offsets, num_samples, page_size, NUM_PAGES);

	if (order_read_first) {
		run_read_campaign(region, offsets, read_latencies, read_usage, num_samples, &dummy);
		run_write_campaign(region, offsets, write_latencies, write_usage, num_samples);
	} else {
		run_write_campaign(region, offsets, write_latencies, write_usage, num_samples);
		run_read_campaign(region, offsets, read_latencies, read_usage, num_samples, &dummy);
	}

	print_samples("READ campaign", read_latencies, read_usage, offsets, num_samples, page_size);
	print_samples("WRITE campaign", write_latencies, write_usage, offsets, num_samples, page_size);

	if (munmap(region, region_size) != 0) {
		perror("munmap");
		return EXIT_FAILURE;
	}

	free(offsets);
	free(read_latencies);
	free(write_latencies);
	free(read_usage);
	free(write_usage);

	fprintf(stderr, "\nIgnore this value: %u\n", (unsigned)dummy);

	return EXIT_SUCCESS;
}
