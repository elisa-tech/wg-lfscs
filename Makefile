CC = $(CROSS_COMPILE)gcc
CFLAGS = --static -nostartfiles

all: min ftrace_it min.large

min: min.c
	$(CC) $(CFLAGS) -o min min.c

min.large: min.large.c
	$(CC) $(CFLAGS) -O0 -o min.large min.large.c

ftrace_it: ftrace_it.c
	$(CC) ftrace_it.c -o ftrace_it
