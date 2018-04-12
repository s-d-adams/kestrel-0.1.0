#include <ncurses.h>

int main (void)
{
	initscr ();
	int c = KEY_RESIZE;
	while (c == KEY_RESIZE)
	{
		mvprintw (0, 0, "%d   ", COLS);
		refresh ();
		c = getch ();
	}
	endwin ();

	return 0;
}
