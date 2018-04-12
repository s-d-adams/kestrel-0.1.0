#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main (void)
{
	char c;
	char * p = malloc (4);
	p = (char*) (((uintmax_t) p) | 0x1000000000000000);
	printf ("stack: %lx\nheap: %lx\n",(uintmax_t) &c,(uintmax_t) p);
	return 0;
}
