#include <stdlib.h>
#include <stdio.h>

struct coord
{
	int x, y;
};

int main (void)
{
	struct coord * c = ({
		struct coord *c =  (struct coord *) malloc (sizeof (struct coord));
		*c = (struct coord) {.x = 1, .y = 2};
		c;
	});

	printf ("x: %d, y: %d\n", c->x, c->y);

	return 0;
}
