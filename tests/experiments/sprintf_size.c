#include <stdio.h>

int main (void)
{
	int size = snprintf (NULL, 0, "a");
//	int size = sprintf (NULL, "a");
	printf ("%d\n", size);
	return 0;
}
