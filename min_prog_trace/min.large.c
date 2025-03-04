#define NOP1   asm("nop");
#define NOP2      NOP1     NOP1     NOP1     NOP1
#define NOP4      NOP2     NOP2     NOP2     NOP2
#define NOP16     NOP4     NOP4     NOP4     NOP4
#define NOP64     NOP16    NOP16    NOP16    NOP16
#define NOP256    NOP64    NOP64    NOP64    NOP64
#define NOP1024   NOP256   NOP256   NOP256   NOP256
#define NOP4096   NOP1024  NOP1024  NOP1024  NOP1024
#define NOP16384  NOP4096  NOP4096  NOP4096  NOP4096
#define NOP65536  NOP16384 NOP16384 NOP16384 NOP16384


int _start() {
	int a,b,c;
	volatile int f=0;

	if (f) {
		NOP65536
	}
	a = b + c;
	while (1);
	return a;
}
