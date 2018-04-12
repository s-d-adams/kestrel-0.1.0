#include <stdio.h>
#include <stdlib.h>

void print_and_free (char const * const s)
{
	printf ("%s\n", s);
	free ((char *) s);
}

void free_void (void const * const v)
{
	free ((char *) v);
}

int main (void)
{
	char * s = (char *) malloc (5);
	s[0] = 'a';
	s[1] = 'b';
	s[2] = 'c';
	s[3] = 0;
	free_void (s);
	return 0;
}
	
