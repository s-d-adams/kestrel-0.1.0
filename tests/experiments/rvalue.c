#include <stdio.h>

int foo (void)
{
	return 42;
}

void p (int const *i)
{
	printf ("%d\n", *i);
}

int main (void)
{
	p (&foo());
}	
