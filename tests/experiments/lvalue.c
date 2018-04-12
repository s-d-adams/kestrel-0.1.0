#include <stdio.h>

int main (void)
{
	int i = 1;
	({i;}) = 2;
	printf ("%d\n", i);
}
