/* signal handling stub for setjmp / longjmp */

int sigstub(int mask)
{
	return (mask);
}

int sigsetmask(int mask) __attribute__ ((weak, alias ("sigstub")));
int sigblock(int mask) __attribute__ ((weak, alias ("sigstub")));
