// Minimal PFN visualization demo using /proc/kpageflags.

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#define KPAGEFLAGS_PATH "/proc/kpageflags"
#define MEM_USE_PERCENT 75

enum {
	KPF_LOCKED		= 0,
	KPF_ERROR		= 1,
	KPF_REFERENCED		= 2,
	KPF_UPTODATE		= 3,
	KPF_DIRTY		= 4,
	KPF_LRU			= 5,
	KPF_ACTIVE		= 6,
	KPF_SLAB		= 7,
	KPF_WRITEBACK		= 8,
	KPF_RECLAIM		= 9,
	KPF_BUDDY		= 10,
	KPF_MMAP		= 11,
	KPF_ANON		= 12,
	KPF_SWAPCACHE		= 13,
	KPF_SWAPBACKED		= 14,
	KPF_COMPOUND_HEAD	= 15,
	KPF_COMPOUND_TAIL	= 16,
	KPF_HUGE		= 17,
	KPF_UNEVICTABLE		= 18,
	KPF_HWPOISON		= 19,
	KPF_NOPAGE		= 20,
	KPF_KSM			= 21,
	KPF_THP			= 22,
	KPF_OFFLINE		= 23,
	KPF_ZERO_PAGE		= 24,
	KPF_IDLE		= 25,
	KPF_PGTABLE		= 26
};

#define BIT_ULL(n) (1ULL << (n))
#define HAS_FLAG(v, bit) (((v) & BIT_ULL(bit)) != 0)

typedef enum {
	CLS_FREE = 0,
	CLS_SLAB,
	CLS_USER_ANON,
	CLS_USER_FILE,
	CLS_PGTABLE,
	CLS_HUGE,
	CLS_OTHER,
	CLS_OFFLINE,
	CLS_COUNT
} page_class_t;

static const char *class_name(page_class_t c) {
	switch (c) {
		case CLS_FREE:	  return "free";
		case CLS_SLAB:	  return "slab";
		case CLS_USER_ANON: return "user-anon";
		case CLS_USER_FILE: return "user-file";
		case CLS_PGTABLE:   return "pgtable";
		case CLS_HUGE:	  return "huge";
		case CLS_OTHER:	 return "other-kernel";
		case CLS_OFFLINE:   return "offline/oob";
		default:			return "unknown";
	}
}

static bool class_is_user(page_class_t c) {
	return c == CLS_USER_ANON || c == CLS_USER_FILE;
}

static bool class_is_strong_source(page_class_t c) {
	return c == CLS_SLAB;
}

static bool class_is_weak_source(page_class_t c) {
	return c == CLS_OTHER;
}

static const char *class_ansi_bg(page_class_t c) {
	switch (c) {
		case CLS_FREE:		return "\x1b[42m";   // green
		case CLS_SLAB:		return "\x1b[41m";   // red
		case CLS_USER_ANON:	return "\x1b[43m";   // yellow
		case CLS_USER_FILE:	return "\x1b[44m";   // blue
		case CLS_PGTABLE:	return "\x1b[45m";   // magenta
		case CLS_HUGE:		return "\x1b[46m";   // cyan
		case CLS_OTHER:		return "\x1b[47m";   // white/gray-ish
		case CLS_OFFLINE:	return "\x1b[100m";  // bright black
		default:		return "\x1b[0m";
	}
}

static const char *ANSI_RESET = "\x1b[0m";
static const char *ANSI_BOLD  = "\x1b[1m";

static void print_cell(page_class_t c, bool boundary) {
	const char *bg = class_ansi_bg(c);
	if (boundary) {
		fputs(ANSI_BOLD, stdout);
		fputs(bg, stdout);
		fputs("[]", stdout);
		fputs(ANSI_RESET, stdout);
	} else {
		fputs(bg, stdout);
		fputs("  ", stdout);
		fputs(ANSI_RESET, stdout);
	}
}

static void print_legend(void) {
	printf("Legend: ");
	for (int i = 0; i < CLS_COUNT; i++) {
		print_cell((page_class_t)i, false);
		printf(" %s", class_name((page_class_t)i));
		if (i != CLS_COUNT - 1)
			printf("  ");
	}
	printf("\n");
	printf("Boundary marker: ");
	fputs(ANSI_BOLD, stdout);
	fputs("[]", stdout);
	fputs(ANSI_RESET, stdout);
	printf(" marks kernel↔user adjacency\n");
}

static int terminal_columns(void) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
		return ws.ws_col;
	return 80;
}

static int read_kpageflags_entry(int fd, uint64_t pfn, uint64_t *flags_out) {
	off_t off = (off_t)(pfn * sizeof(uint64_t));
	uint64_t v = 0;
	ssize_t n = pread(fd, &v, sizeof(v), off);
	if (n == (ssize_t)sizeof(v)) {
		*flags_out = v;
		return 0;
	}
	if (n == 0)	  return 1;
	if (n < 0)	   return -errno;
	return -EIO;
}

static page_class_t classify_flags(uint64_t f) {
	if (HAS_FLAG(f, KPF_OFFLINE) || HAS_FLAG(f, KPF_NOPAGE))
		return CLS_OFFLINE;

	if (HAS_FLAG(f, KPF_BUDDY))
		return CLS_FREE;

	if (HAS_FLAG(f, KPF_SLAB))
		return CLS_SLAB;

	if (HAS_FLAG(f, KPF_PGTABLE))
		return CLS_PGTABLE;

	if (HAS_FLAG(f, KPF_HUGE) ||
		HAS_FLAG(f, KPF_THP) ||
		HAS_FLAG(f, KPF_COMPOUND_HEAD) ||
		HAS_FLAG(f, KPF_COMPOUND_TAIL))
		return CLS_HUGE;

	if (HAS_FLAG(f, KPF_ANON))
		return CLS_USER_ANON;

	if (HAS_FLAG(f, KPF_MMAP))
		return CLS_USER_FILE;

	return CLS_OTHER;
}

typedef struct {
	uint64_t start_pfn;
	uint64_t count;
	int cols;
	int percentage;
	bool legend_top;
	bool summary;
	bool stop_on_error;
	bool stop_on_eof;
} opts_t;

static void usage(const char *prog) {
	fprintf(stderr,
		"Usage: %s [options]\n"
		"\n"
		"Options:\n"
		"  --start-pfn N     Start PFN (decimal or 0x...)\n"
		"  --count N         Number of PFNs to render\n"
		"  --cols N          Cells per row (default: auto from terminal width)\n"
		"  --percentage N    occupy given percentage of memory before mapping\n"
		"  --legend-bottom   Put legend after raster\n"
		"  --no-summary      Do not print class/boundary summary\n"
		"  --stop-on-error   Stop on pread error\n"
		"  --stop-on-eof     Stop when PFN read reaches EOF\n"
		"  -h, --help        Show this help\n",
		prog);
}

static uint64_t parse_u64(const char *s) {
	errno = 0;
	char *end = NULL;
	unsigned long long v = strtoull(s, &end, 0);
	if (errno || end == s || *end != '\0') {
		fprintf(stderr, "Invalid integer: %s\n", s);
		exit(2);
	}
	return (uint64_t)v;
}

static void parse_args(int argc, char **argv, opts_t *o) {
	memset(o, 0, sizeof(*o));
	o->start_pfn = 0;
	o->count = 4096;
	o->cols = 0;
	o->legend_top = true;
	o->summary = true;
	o->stop_on_error = false;
	o->stop_on_eof = false;
	o->percentage = MEM_USE_PERCENT;

	static const struct option long_opts[] = {
		{"start-pfn",     required_argument, 0, 1},
		{"count",         required_argument, 0, 2},
		{"cols",          required_argument, 0, 3},
		{"use-ram",       required_argument, 0, 4},
		{"legend-bottom", no_argument,       0, 5},
		{"no-summary",    no_argument,       0, 6},
		{"stop-on-error", no_argument,       0, 7},
		{"stop-on-eof",   no_argument,       0, 8},
		{"help",          no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	int c;
	while ((c = getopt_long(argc, argv, "h", long_opts, NULL)) != -1) {
		switch (c) {
			case 1: o->start_pfn = parse_u64(optarg); break;
			case 2: o->count = parse_u64(optarg); break;
			case 3: o->cols = (int)parse_u64(optarg); break;
			case 4: o->percentage = (int)parse_u64(optarg); break;
			case 5: o->legend_top = false; break;
			case 6: o->summary = false; break;
			case 7: o->stop_on_error = true; break;
			case 8: o->stop_on_eof = true; break;
			case 'h':
			default:
				usage(argv[0]);
				exit(c == 'h' ? 0 : 2);
		}
	}
}

typedef struct {
	uint64_t class_counts[CLS_COUNT];
	uint64_t slab_to_user_count;
	uint64_t other_to_user_count;
	uint64_t first_boundary_pfn[16];
	page_class_t first_boundary_left[16];
	page_class_t first_boundary_right[16];
	size_t first_boundary_used;
} stats_t;

static void record_boundary(stats_t *st, uint64_t left_pfn, page_class_t a, page_class_t b) {
	if (st->first_boundary_used < 16) {
		size_t i = st->first_boundary_used++;
		st->first_boundary_pfn[i] = left_pfn;
		st->first_boundary_left[i] = a;
		st->first_boundary_right[i] = b;
	}
}

static int row_label_width(uint64_t start_pfn, uint64_t count) {
	uint64_t last = start_pfn;
	if (count > 0)
		last = start_pfn + count - 1;

	int w1 = snprintf(NULL, 0, "0x%08llx  ",
					  (unsigned long long)start_pfn);
	int w2 = snprintf(NULL, 0, "0x%08llx  ",
					  (unsigned long long)last);
	return (w1 > w2) ? w1 : w2;
}

static void render_range(int fd, const opts_t *o, stats_t *st) {
	uint64_t start = o->start_pfn;
	uint64_t end = start + o->count;
	if (end < start) {
		fprintf(stderr, "PFN range overflow\n");
		exit(2);
	}

	int cols = o->cols;
	int label_cols = row_label_width(start, o->count);

	if (cols <= 0) {
		int term_cols = terminal_columns();
		cols = (term_cols - label_cols) / 2;
		if (cols <= 0)
			cols = 1;
	}

	printf("PFN range: 0x%llx .. 0x%llx  (%llu PFNs), cols=%d\n",
		   (unsigned long long)start,
		   (unsigned long long)(end ? end - 1 : 0),
		   (unsigned long long)o->count,
		   cols);
	if (o->legend_top)
		print_legend();

	page_class_t prev = CLS_OFFLINE;
	bool have_prev = false;

	for (uint64_t pfn = start; pfn < end; pfn++) {
		uint64_t flags = 0;
		page_class_t cls;
		int rc = read_kpageflags_entry(fd, pfn, &flags);

		if (rc == 0) {
			cls = classify_flags(flags);
		} else if (rc == 1) {
			if (o->stop_on_eof)
				break;
			cls = CLS_OFFLINE;
		} else {
			if (o->stop_on_error) {
				fprintf(stderr, "Failed reading PFN 0x%llx: %s\n",
						(unsigned long long)pfn, strerror(-rc));
				exit(1);
			}
			cls = CLS_OFFLINE;
		}

		st->class_counts[cls]++;

		bool boundary = false;
		if (have_prev) {
			if (class_is_strong_source(prev) && class_is_user(cls)) {
				boundary = true;
				record_boundary(st, pfn - 1, prev, cls);
				st->slab_to_user_count++;
			} else if (class_is_weak_source(prev) && class_is_user(cls)) {
				st->other_to_user_count++;
			}
		}

		if (((pfn - start) % (uint64_t)cols) == 0) {
			if (pfn != start)
				putchar('\n');
			printf("0x%08llx  ", (unsigned long long)pfn);
		}

		print_cell(cls, boundary);

		prev = cls;
		have_prev = true;
	}

	putchar('\n');

	if (!o->legend_top)
		print_legend();
}

static void print_summary(const stats_t *st) {
	printf("\nSummary:\n");
	for (int i = 0; i < CLS_COUNT; i++) {
		printf("  %-12s  %10llu\n",
			   class_name((page_class_t)i),
			   (unsigned long long)st->class_counts[i]);
	}

	printf("  %-12s  %10llu\n", "slab->user",
		   (unsigned long long)st->slab_to_user_count);
	printf("  %-12s  %10llu\n", "other->user",
		   (unsigned long long)st->other_to_user_count);

	if (st->first_boundary_used) {
		printf("\nFirst interesting boundaries:\n");
		for (size_t i = 0; i < st->first_boundary_used; i++) {
			printf("  PFN 0x%llx -> 0x%llx : %s -> %s\n",
				   (unsigned long long)st->first_boundary_pfn[i],
				   (unsigned long long)(st->first_boundary_pfn[i] + 1),
				   class_name(st->first_boundary_left[i]),
				   class_name(st->first_boundary_right[i]));
		}
	}
}

long get_total_mem() {
	FILE *fp = fopen("/proc/meminfo", "r");
	if (!fp) {
		perror("Failed to open /proc/meminfo");
		return -1;
	}

	char label[32];
	long value;
	char unit[8];

	while (fscanf(fp, "%s %ld %s", label, &value, unit) != EOF) {
		if (strcmp(label, "MemTotal:") == 0) {
			fclose(fp);
			return value * 1024;
		}
	}

	fclose(fp);
	return -1;
}

int allocate_and_touch(double percentage) {
	if (percentage <= 0 || percentage > 100) {
		fprintf(stderr, "Error: Percentage must be between 1 and 100.\n");
		return -1;
	}

	long total_mem = get_total_mem();
	if (total_mem < 0) return -1;

	size_t bytes_to_alloc = (size_t)(total_mem * (percentage / 100.0));
	printf("Total System Memory: %ld MB\n", total_mem / (1024 * 1024));
	printf("Targeting (%0.2f%%): %zu MB\n", percentage, bytes_to_alloc / (1024 * 1024));

	void *addr = mmap(NULL, bytes_to_alloc, PROT_READ | PROT_WRITE, 
					  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (addr == MAP_FAILED) {
		perror("mmap failed");
		return -1;
	}

	if (mlock(addr, bytes_to_alloc) != 0) {
		perror("mlock failed (are you root?)");
	}

	printf("Touching memory... ");
	fflush(stdout);
	memset(addr, 0, bytes_to_alloc);
	printf("Done.\n");

	return 0;
}

int main(int argc, char **argv) {
	opts_t opt;
	parse_args(argc, argv, &opt);

	if (!allocate_and_touch(opt.percentage) == 0) {
	fprintf(stderr, "Can't allocate required %d%% of memory\n", opt.percentage);
	return 1;
	}

	int fd = open(KPAGEFLAGS_PATH, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(%s): %s\n", KPAGEFLAGS_PATH, strerror(errno));
		return 1;
	}

	stats_t st;
	memset(&st, 0, sizeof(st));

	render_range(fd, &opt, &st);

	if (opt.summary)
		print_summary(&st);

	close(fd);
	return 0;
}
