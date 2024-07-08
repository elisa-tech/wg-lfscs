
min: min.c
	gcc --static -nostartfiles -o min min.c

ftrace_it: ftrace_it.c
	gcc ftrace_it.c -o ftrace_it
