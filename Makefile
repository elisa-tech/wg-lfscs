
min: min.c
	gcc --static -nostartfiles -o min min.c

min.large: min.large.c
	gcc --static -nostartfiles -O0 -o min.large min.large.c

ftrace_it: ftrace_it.c
	gcc ftrace_it.c -o ftrace_it
