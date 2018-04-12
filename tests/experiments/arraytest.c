#include <stdlib.h>
#include <stdio.h>

int main (void)
{
	int * c = ({
		int *c =  (int *) malloc (sizeof (int) * 4);
		struct ar {int i[4];};
		*((struct ar*)c) = (struct ar) {1, 2, 3 , 4};
		c;
	});

	struct ar a;

	printf ("%d %d %d %d\n", c[0], c[1], c[2], c[3]);

	return 0;
}
