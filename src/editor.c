/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>
#include <time.h>

#include "display.h"
#include "type.h"
#include "context.h"
#include "eval.h"

static
char * filename = NULL;

static
char default_file_content[] = "";

static
char * build_command = NULL;

static
char * last_find_str = NULL;

static
char * meta_override_str = NULL;

int maxy, maxx;
size_t line_offset = 0;
char * file_content = NULL;
size_t file_content_size = 0;
struct token_list * token_list = NULL;
struct syntax_tree * syntax_tree = NULL;
struct line_list * line_list = NULL;
struct line_list_cursor_model line_cursor;
struct syntax_tree_cursor_model syntax_cursor;
MEVENT mouse;
int dont_delete_empty_name = 0;

static NCURSES_COLOR_T const bg_color_light = COLOR_WHITE;
static NCURSES_COLOR_T const fg_color_light = COLOR_BLACK;
static NCURSES_COLOR_T const bg_color_dark = COLOR_BLACK;
static NCURSES_COLOR_T const fg_color_dark = COLOR_WHITE;
static NCURSES_COLOR_T bg_color = COLOR_WHITE;
static NCURSES_COLOR_T fg_color = COLOR_BLACK;

int rl_catch_signals;
int rl_catch_sigwinch;
int rl_change_environment;

char * rl_line_buffer;
int rl_point;

void
rl_callback_read_char (void);

typedef void rl_vcpfunc_t (char *);

void
rl_callback_handler_install (char const * prompt, rl_vcpfunc_t * lhandler);

void
rl_callback_handler_remove (void);

int
(* rl_getc_function) (FILE * not_used);

int
(* rl_input_available_hook) (void);

void
(* rl_redisplay_function) (void);

struct readline_data
{
	char const * prompt;
	int input;
	int input_ready;
	char * output;
	int output_ready;
};

static
struct readline_data
rl_data;

static
int
rl_input_ready (void)
{
	return rl_data.input_ready;
}

static
int
rl_getc (FILE * not_used)
{
	(void) not_used;
	rl_data.input_ready = 0;
	return rl_data.input;
}

static
void
rl_putc (int const c)
{
	rl_data.input = c;
	rl_data.input_ready = 1;
	rl_callback_read_char ();
}

static
void
rl_redisplay (void)
{
	attron (A_REVERSE);
	move (maxy + 1, 0);
	int i;
	for (i = 0; i <= maxx; ++ i)
	{
		addch (' ');
	}
	mvprintw (maxy + 1, 0, "%s", rl_data.prompt);
	int y, x;
	getyx (stdscr, y, x);
	(void) y;
	size_t const len = strlen (rl_line_buffer);
	size_t const remaining_x = maxx - x;
	size_t const offset = len > remaining_x ? len - remaining_x : 0;
	printw ("%s", rl_line_buffer + offset);
	attroff (A_REVERSE);
}

static
void
rl_finished (char * const output)
{
	rl_data.output = output;
	rl_data.output_ready = 1;
}

static
void
init_rl (void)
{
	rl_data.input_ready = 0;
	rl_data.output = NULL;

	rl_catch_signals = 0;
	rl_catch_sigwinch = 0;
	rl_change_environment = 0;

	rl_getc_function = rl_getc;
	rl_input_available_hook = rl_input_ready;
	rl_redisplay_function = rl_redisplay;
}

static
void
resize (void);

static
void
init_colours (void)
{
	if (has_colors () == TRUE)
	{
		start_color ();
		if (can_change_color () == TRUE)
		{
			init_color (COLOR_WHITE, 1000, 1000, 1000);
		}
		init_pair (1, fg_color,    bg_color); // Default.
		init_pair (2, COLOR_BLUE,  bg_color); // Keywords.
		init_pair (3, COLOR_RED,   bg_color); // Strings.
		init_pair (4, COLOR_GREEN, bg_color); // Type names.
		attron (COLOR_PAIR (1));
		bkgd (COLOR_PAIR (1));
	}
}

void
redisplay (void);

static
void
set_colours_light (void)
{
	bg_color = bg_color_light;
	fg_color = fg_color_light;
	init_colours ();
	redisplay ();
}

static
void
set_colours_dark (void)
{
	bg_color = bg_color_dark;
	fg_color = fg_color_dark;
	init_colours ();
	redisplay ();
}

static
void
toggle_light_dark (void)
{
	// Safeguard for photosensitive epilepsy.
	static time_t last_toggled;
	static int first_toggle = 1;
	if (first_toggle)
	{
		time (& last_toggled);
		first_toggle = 0;
	}
	else
	{
		time_t now;
		time (& now);
		double const d = difftime (now, last_toggled);
		if (d < 1.0)
		{
			return;
		}
		else
		{
			last_toggled = now;
		}
	}

	if (bg_color == bg_color_dark)
	{
		set_colours_light ();
	}
	else
	{
		set_colours_dark ();
	}
}

static
void
init_ncurses (void)
{
	init_rl ();
	initscr ();
	raw ();
	noecho ();
	keypad (stdscr, TRUE);
	init_colours ();
	mousemask (BUTTON1_PRESSED, NULL);
	resize ();
}

void
write_ncurses (void * const data, char const * const content, size_t const size)
{
	(void) data;

	if (size == 1 && (* content == '\f'))
	{
		int i;
		addch (' ');
		for (i = 0; i <= maxx - 2; ++ i)
		{
			addch (ACS_HLINE);
		}
		return;
	}

	size_t i = 0;
	for (; i < size; ++ i)
	{
		addch (content [i]);
	}
}

void
write_stderr (void * const data, char const * const content, size_t const size)
{
	(void) data;
	dprintf (STDERR_FILENO, "%.*s", (int) size, content);
}

// Returns non-zero if true, returns 2 if it is the last in the expression.
int
is_part_of_current_expression (struct syntax_tree * const node)
{
	struct syntax_tree const * const parent = syntax_cursor.symbol->token.type == token_type_open ? syntax_cursor.symbol : syntax_cursor.symbol->parent_tree;

	if (node->parent_tree == parent)
	{
		if (node->token.type == token_type_close)
		{
			return 2;
		}
	}

	struct syntax_tree * np = node;
	while (np)
	{
		if (np == parent)
		{
			return 1;
		}
		np = np->parent_tree;
	}

	if (node == syntax_cursor.symbol)
	{
		return 2;
	}

	return 0;
}

static
int
is_keyword (char const * const bytes, size_t const length)
{
	static
	char const * const keywords [] =
	{
		"addr",
		"array",
		"assign",
		"cond",
		"dec",
		"def",
		"deref",
		"do",
		"doEach",
		"field",
		"inc",
		"lam",
		"let",
		"map",
		"mutable",
		"ptr",
		"struct",
		"use",
		NULL
	};

	size_t i = 0;
	for (; keywords[i]; ++ i)
	{
		char const * const s = keywords[i];
		if (strlen (s) == length)
		{
			if (! strncmp (s, bytes, length))
			{
				return 1;
			}
		}
	}
	return 0;
}

void
write_highlighted (void * const data, char const * const output, size_t const length, struct syntax_tree * const node)
{
	int highlight = 0;
	int keyword_highlight = 0;
	int primative_highlight = 0;
	if (node)
	{
		highlight = is_part_of_current_expression (node);
		if (highlight)
		{
			attron (A_BOLD);
		}
	}
	if (node && node->token.type == token_type_string)
	{
		attron (COLOR_PAIR (3));
	}
	else
	{
		keyword_highlight = is_keyword (output, length);
		if (keyword_highlight)
		{
			attron (COLOR_PAIR (2));
		}
		else
		{
			primative_highlight = node && node->token.type == token_type_name && is_type_primative (node);
			if (primative_highlight)
			{
				attron (COLOR_PAIR (4));
			}
		}
	}

	write_ncurses (data, output, length);

	if ((node && node->token.type == token_type_string) || keyword_highlight || primative_highlight)
	{
		attron (COLOR_PAIR (1));
	}

	if ((highlight == 2) || (node && node->token.type == token_type_break))
	{
		attroff (A_BOLD);
	}
}

void
write_meta (struct line_list_cursor_model const user_line_cursor, struct syntax_tree_cursor_model const user_syntax_cursor)
{
	move (maxy + 1, 0);
	attron (A_REVERSE);
	int i;
	for (i = 0; i <= maxx; ++ i)
	{
		addch (' ');
	}
	move (maxy + 1, 0);
	if (meta_override_str)
	{
		printw ("%s", meta_override_str);
		free (meta_override_str);
		meta_override_str = NULL;
	}
	else
	{
		printw ("Line: %d Column: %d", user_line_cursor.y, user_line_cursor.x);
		if (filename)
		{
			printw (" File: ");
			int y, x;
			getyx (stdscr, y, x);
			(void) y;
			size_t const len = strlen (filename);
			size_t const remaining_x = maxx - x;
			size_t const offset = len > remaining_x ? len - remaining_x : 0;
			printw ("%s", filename + offset);
		}
		if (0)
		{
			printw (" Index: %d Symbol: ", user_syntax_cursor.symbol->token.index + user_syntax_cursor.offset);
			write_ncurses (NULL, user_syntax_cursor.symbol->token.bytes, user_syntax_cursor.symbol->token.size);
		}
	}
	attroff (A_REVERSE);
}

void
move_cursor (struct line_list_cursor_model const user_line_cursor)
{
	move (user_line_cursor.y, user_line_cursor.x);
}

void
clear_line (struct line_list_cursor_model const line_cursor)
{
	move (line_cursor.y, 0);
	clrtoeol ();
}

void
rebuild_line_list (void)
{
	assert (syntax_tree);
	if (line_list)
	{
		del_line_list (line_list);
	}
	line_list = split_top_level_expressions (syntax_tree);
}

void
redisplay (void)
{
	erase ();
	move (0, 0);
	size_t const line_count = maxy;
	display_line_list (line_list, & line_offset, & line_count, NULL, write_highlighted, NULL);
	attroff (A_BOLD);
}

void
resize (void)
{
	getmaxyx (stdscr, maxy, maxx);
	maxy -= 2;
	maxx -= 1;

	if (line_list)
	{
		fit_line_list_to_columns (line_list, maxx);
		redisplay ();
		line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
		if (0)
		{
//			addch ('\n');
			display_syntax_tree_debug (syntax_tree, 0, write_stderr, NULL);
			fsync (STDERR_FILENO);
		}
	}
}

void
clear_file (void)
{
	if (file_content)
	{
		del_line_list (line_list);
		line_list = NULL;
		del_syntax_tree (syntax_tree);
		syntax_tree = NULL;
		del_token_list (token_list);
		token_list = NULL;
		free (file_content);
		file_content = NULL;
	}

	if (filename)
	{
		free (filename);
	}
	filename = NULL;

	file_content = default_file_content;
	file_content_size = 0;

	token_list = lex (file_content, file_content_size);
	assert (token_list);

	syntax_tree = parse (token_list);
	assert (syntax_tree);

	rebuild_line_list ();
	line_cursor.y = 0;
	line_cursor.x = 0;
	line_offset = 0;
}

void
rebuild_file_model (void)
{
	token_list = lex (file_content, file_content_size);
	assert (token_list);

	syntax_tree = parse (token_list);
	assert (syntax_tree);

	rebuild_line_list ();
	line_cursor.y = 0;
	line_cursor.x = 0;
	line_offset = 0;
}

void
open_file (char const * const fn)
{
	struct stat st;
	if (stat (fn, & st))
	{
		goto fail;
	}

	clear_file ();

	file_content = malloc (st.st_size);
	file_content_size = st.st_size;
	int const fd = open (fn, O_RDONLY);
	if (fd == -1)
	{
		free (file_content);
		goto fail;
	}
	assert (st.st_size == read (fd, file_content, st.st_size));
	close (fd);

	rebuild_file_model ();

	filename = strdup (fn);

	line_cursor.y = 0;
	line_cursor.x = 0;
	line_offset = 0;
	syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);

	return;
fail:
	attron (A_REVERSE);
	move (maxy + 1, 0);
	int i;
	for (i = 0; i <= maxx; ++ i)
	{
		addch (' ');
	}
	mvprintw (maxy + 1, 0, "%s: %s", fn, strerror (errno));
	getch ();
	attroff (A_REVERSE);
	return;
}

char *
rl_get_string (char const * const prompt, char const * input)
{
	rl_data.prompt = prompt;
	rl_data.output = NULL;
	rl_data.input_ready = 0;
	rl_data.output_ready = 0;
	rl_callback_handler_install (prompt, rl_finished);
	if (input)
	{
		while (* input)
		{
			rl_putc (* input);
			++ input;
		}
	}
	do
	{
		while (! rl_data.output_ready)
		{
			int const c = getch ();
			if (c == KEY_BACKSPACE)
			{
				rl_putc ('\b');
			}
			else if (c == KEY_RESIZE)
			{
				resize ();
				rl_redisplay ();
			}
			else
			{
				rl_putc (c);
			}
		}
	}
	while (rl_data.output == NULL);
	return rl_data.output;
}

void
prompt_open_file (void)
{
	char * filename = rl_get_string ("Open File: ", NULL);

	char * fp = filename + (strlen (filename) - 1);
	while (isspace (* fp)) -- fp;
	fp [1] = '\0';
	fp = filename;
	while (isspace (* fp)) ++ fp;
	open_file (fp);
	free (filename);

	resize ();
}

void
counting_write_handler (void * data, char const * const output, size_t const size)
{
	* (size_t *) data += size;
}

void
memcpy_write_handler (void * data, char const * const output, size_t const size)
{
	char ** const bufptr = data;
	memcpy (* bufptr, output, size);
	* bufptr += size;
}

void
prompt_write_file (void)
{
	char * fn = rl_get_string ("Save File: ", filename);

	char * fp = fn + (strlen (fn) - 1);
	while (isspace (* fp)) -- fp;
	fp [1] = '\0';
	fp = fn;
	while (isspace (* fp)) ++ fp;

	size_t size = 0;
	display_syntax_tree_minimal (syntax_tree, counting_write_handler, & size);

	char * const file_content_minimised = (char *) malloc (size * sizeof (char));
	char * fcptr = file_content_minimised;
	display_syntax_tree_minimal (syntax_tree, memcpy_write_handler, & fcptr);

	int const fd = open (fp, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (fd == -1)
	{
		free (file_content_minimised);
		goto fail;
	}
	assert (size == write (fd, file_content_minimised, size));
	close (fd);

	if (filename)
	{
		free (filename);
	}
	filename = strdup (fn);

	free (file_content_minimised);
	free (fn);

	resize ();

	return;
fail:
	attron (A_REVERSE);
	move (maxy + 1, 0);
	int i;
	for (i = 0; i <= maxx; ++ i)
	{
		addch (' ');
	}
	mvprintw (maxy + 1, 0, "%s: %s", fn, strerror (errno));
	getch ();
	attroff (A_REVERSE);
	return;
}

void
prompt_run_command (void)
{
	char * command = rl_get_string ("$ ", NULL);
	char * cp = command + (strlen (command) - 1);
	while (isspace (* cp)) -- cp;
	cp [1] = '\0';
	cp = command;
	while (isspace (* cp)) ++ cp;

	def_prog_mode ();
	endwin ();
	system (command);
	printf ("\nPress any key to continue.\n");
	getchar ();
	reset_prog_mode ();
	refresh ();

	free (command);
}

static
void
editor_jump_to_symbol (struct syntax_tree * const symbol)
{
	syntax_cursor.symbol = symbol;
	syntax_cursor.offset = symbol->token.size;
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	if (line_cursor.y > maxy)
	{
		line_offset += (line_cursor.y - maxy);
		line_cursor.y = maxy;
	}
	resize ();
}

static
void
editor_find_str_not_found (char const * const find_str)
{
	asprintf (& meta_override_str, "Find: '%s' not found.", find_str);
}

static
struct syntax_tree *
editor_find (struct syntax_tree * const start, char const * const find_str)
{
	struct syntax_tree * seek = start;
	while (seek && t_strcmp (seek->token, find_str))
	{
		seek = find_next_symbol (seek);
	}

	if (seek && ! t_strcmp (seek->token, find_str))
	{
		return seek;
	}

	return NULL;
}

static
void
prompt_find (void)
{
	char * find_str = rl_get_string ("Find: ", last_find_str);

	struct syntax_tree * const find_result = editor_find (syntax_cursor.symbol, find_str);

	if (find_result)
	{
		editor_jump_to_symbol (find_result);
	}
	else
	{
		editor_find_str_not_found (find_str);
	}

	if (last_find_str)
	{
		free (last_find_str);
	}
	last_find_str = find_str;

	redisplay ();
}

static
void
editor_find_next (void)
{
	if (last_find_str == NULL)
	{
		prompt_find ();
		return;
	}

	struct syntax_tree * next = find_next_symbol (syntax_cursor.symbol);
	if (! next)
	{
		next = syntax_tree;
	}

	struct syntax_tree * find_result = editor_find (next, last_find_str);

	if (! find_result)
	{
		if (next != syntax_tree)
		{
			find_result = editor_find (syntax_tree, last_find_str);
		}
	}

	if (find_result)
	{
		editor_jump_to_symbol (find_result);
	}
	else
	{
		editor_find_str_not_found (last_find_str);
	}

	redisplay ();
}

void
prompt_build_command (void)
{
	if (build_command)
	{
		free (build_command);
		build_command = NULL;
	}
	build_command = rl_get_string ("Enter Build Command: ", NULL);
}

void
run_build_command (void)
{
	assert (build_command);
	def_prog_mode ();
	endwin ();
	system (build_command);
	printf ("\nPress any key to continue.\n");
	getchar ();
	reset_prog_mode ();
	refresh ();
}

void
build_this_file (void)
{
	def_prog_mode ();
	endwin ();

	struct conf conf = default_conf ();
	struct context ctx;
	context_init (& ctx, conf);

	struct string_list * const begin_m = new_string_list (NULL, 0, 0), * end_m;
	end_m = begin_m;

	if (ctx.conf.boundsChecking)
	{
		end_m = sl_copy_append (end_m, "#include \"assert.h\"\n");
	}
	end_m = sl_copy_append (end_m,
	    "#include \"stdio.h\""
	    "\n#include \"stdlib.h\""
	    "\n#include \"string.h\"\n");

	struct syntax_tree * it = syntax_tree->next;
	for (; it; it = it->next)
	{
		end_m = sl_splice (end_m, eval (& ctx, it));
		if (ctx.has_error)
		{
			printf ("%s\n", ctx.error_report.message);
			break;
		}
	}

	if (! ctx.has_error)
	{
		struct string_list * it = begin_m;
		for (; it; it = it->next)
		{
			printf ("%.*s", (int) it->size, it->string);
			if (it->size == 1 && * it->string == ';')
			{
				printf ("\n");
			}
		}
	}
	del_string_list (begin_m);
	printf ("\nPress any key to continue.\n");
	getchar ();
	reset_prog_mode ();
	refresh ();
}

void
new_file (void)
{
	clear_file ();
	resize ();
}

// Returns the last node moved, or the starting point if none were moved.
struct syntax_tree *
move_expression_list_to_new_parent (struct syntax_tree * const to_move, struct syntax_tree * const new_parent, struct syntax_tree * const starting_point, int const stop_after_close)
{
	struct syntax_tree * loop_prev = starting_point;
	struct syntax_tree * it = to_move;
	while (it)
	{
		if (it->token.type == token_type_break)
		{
			break;
		}
		struct syntax_tree * next = it->next;
		st_take (it);
		it->parent_tree = new_parent;
		st_add (loop_prev, it);
		loop_prev = it;
		it = next;
		if (stop_after_close && (loop_prev->token.type == token_type_close))
		{
			break;
		}
	}
	return loop_prev;
}

void
remove_open (void)
{
	assert (syntax_cursor.symbol->token.type == token_type_open);
	assert (syntax_cursor.offset == 1);

	struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
	struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);

	struct syntax_tree * new_parent = syntax_cursor.symbol->parent_tree;
	struct syntax_tree * const sub = syntax_cursor.symbol->sub_tree;
	assert (sub);
	struct syntax_tree * to_move = sub->next;
	struct syntax_tree * starting_point = syntax_cursor.symbol;

	do
	{
		struct syntax_tree * const last_moved = move_expression_list_to_new_parent (to_move, new_parent, starting_point, 0);
		to_move = last_moved->next;
		starting_point = new_parent;
		new_parent = new_parent ? new_parent->parent_tree : NULL;
	}
	while (starting_point);

	st_take (syntax_cursor.symbol);
	del_syntax_tree (syntax_cursor.symbol);
	syntax_cursor.symbol = next ? next : (prev ? prev                             : syntax_tree);
	syntax_cursor.offset = next ? 0    : (prev ? syntax_cursor.symbol->token.size : 0);
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
remove_close (void)
{
	assert (syntax_cursor.symbol->token.type == token_type_close);
	assert (syntax_cursor.offset == 1);

	struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);

	struct syntax_tree * parent = syntax_cursor.symbol->parent_tree;
	struct syntax_tree * starting_point = syntax_cursor.symbol;
	while (parent)
	{
		struct syntax_tree * to_move = parent->next;
		move_expression_list_to_new_parent (to_move, parent, starting_point, 1);
		starting_point = parent;
		parent = parent->parent_tree;
	}

	st_take (syntax_cursor.symbol);
	del_syntax_tree (syntax_cursor.symbol);
	syntax_cursor.symbol = prev;
	syntax_cursor.offset = syntax_cursor.symbol->token.size;
	rebuild_line_list();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
insert_open (void)
{
	assert (syntax_cursor.offset == 0);
	assert (syntax_cursor.symbol->token.type != token_type_none);

	struct syntax_tree * const prev = syntax_cursor.symbol->prev;
	assert (prev);
	struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_open, & open_brace_char, 1, 0, 0, 0), prev->parent_tree);
	new_node->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), new_node);

	if (prev->parent_tree == NULL)
	{
		move_expression_list_to_new_parent (syntax_cursor.symbol, new_node, new_node->sub_tree, 1);
	}
	else
	{
		struct syntax_tree * to_move = syntax_cursor.symbol;
		struct syntax_tree * new_parent = new_node;
		do
		{
			struct syntax_tree * const old_parent = to_move->parent_tree;
			struct syntax_tree * const starting_point = st_tail (new_parent->sub_tree);
			move_expression_list_to_new_parent (to_move, new_parent, starting_point, 0);
			new_parent = old_parent;
			if (old_parent)
			{
				to_move = old_parent->next;
			}
		}
		while (new_parent && to_move);
	}

	st_add (prev, new_node);
	syntax_cursor.symbol = new_node;
	syntax_cursor.offset = 1;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
insert_break (void)
{
	if (syntax_cursor.offset == 0)
	{
		assert (syntax_cursor.symbol->prev == syntax_tree);
		syntax_cursor.symbol = syntax_tree;
	}
	else
	{
		assert (syntax_cursor.offset == syntax_cursor.symbol->token.size);
		assert (syntax_cursor.symbol != syntax_tree);
	}

	struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);

	struct syntax_tree * top = syntax_cursor.symbol;
	for (; top->parent_tree; top = top->parent_tree);

	struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_break, & break_char, 1, 0, 0, 0), NULL);
	st_add (top, new_node);

	if (next)
	{
		struct syntax_tree * to_move = next;
		struct syntax_tree * starting_point = new_node;
		do
		{
			struct syntax_tree * next_to_move = to_move->parent_tree ? to_move->parent_tree->next : NULL;
			starting_point = move_expression_list_to_new_parent (to_move, NULL, starting_point, 0);
			to_move = next_to_move;
		}
		while (to_move);
	}
	syntax_cursor.symbol = new_node;
	syntax_cursor.offset = 1;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

// This is a very 'raw' function and should not be used often.
void
insert_after_node (enum token_type const type, char const c, struct syntax_tree * const node)
{
	struct syntax_tree * const new_node = new_syntax_tree (make_token (type, NULL, 0, 0, 0, 0), node->parent_tree);
	ensure_capacity (new_node, 1);
	new_node->token.bytes[0] = (type == token_type_open ? open_brace_char : (type == token_type_close ? close_brace_char : (char) c));
	new_node->token.size = 1;
	if (type == token_type_open)
	{
		new_node->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), new_node);
	}
	st_add (node, new_node);
	syntax_cursor.symbol = new_node;
	syntax_cursor.offset = 1;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
empty_string_after_node (struct syntax_tree * const node)
{
	struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_string, "\"\"", 2, 0, 0, 0), node->parent_tree);
	st_add (node, new_node);
	syntax_cursor.symbol = new_node;
	syntax_cursor.offset = 1;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
insert_close (void)
{
	if (syntax_cursor.offset != syntax_cursor.symbol->token.size)
	{
		assert (syntax_cursor.symbol->parent_tree == NULL);
		assert (syntax_cursor.symbol->prev);
		insert_after_node (token_type_close, 0, syntax_cursor.symbol->prev);
		return;
	}

	struct syntax_tree * starting_point = NULL;
	if (syntax_cursor.symbol->token.type == token_type_close)
	{
		starting_point = syntax_cursor.symbol->parent_tree ? syntax_cursor.symbol->parent_tree : syntax_cursor.symbol;
	}
	else if (syntax_cursor.symbol->token.type == token_type_open)
	{
		starting_point = syntax_cursor.symbol->sub_tree;
	}
	else
	{
		starting_point = syntax_cursor.symbol;
	}

	struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_close, & close_brace_char, 1, 0, 0, 0), starting_point->parent_tree);
	st_add (starting_point, new_node);

	struct syntax_tree * to_move = new_node->next;
	starting_point = to_move ? to_move->parent_tree : NULL;
	struct syntax_tree * new_parent = NULL;

	while (starting_point)
	{
		new_parent = starting_point->parent_tree;
		struct syntax_tree * const last_moved = move_expression_list_to_new_parent (to_move, new_parent, starting_point, 0);
		to_move = last_moved->next;
		starting_point = to_move ? to_move->parent_tree : NULL;
	}

	syntax_cursor.symbol = new_node;
	syntax_cursor.offset = 1;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
insert_space (void)
{
	assert (syntax_cursor.symbol->token.type == token_type_name && syntax_cursor.offset > 0);

	size_t const to_move = syntax_cursor.symbol->token.size - syntax_cursor.offset;
	struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_name, NULL, 0, 0, 0, 0), syntax_cursor.symbol->parent_tree);
	if (to_move > 0)
	{
		ensure_capacity (new_node, to_move);
		memcpy (new_node->token.bytes, syntax_cursor.symbol->token.bytes + syntax_cursor.offset, to_move);
		new_node->token.size = to_move;
		syntax_cursor.symbol->token.size = syntax_cursor.offset;
	}
	st_add (syntax_cursor.symbol, new_node);
	syntax_cursor.symbol = new_node;
	syntax_cursor.offset = 0;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
remove_char_from_symbol (void)
{
	ensure_capacity (syntax_cursor.symbol, syntax_cursor.symbol->token.size);
	memmove (syntax_cursor.symbol->token.bytes + (syntax_cursor.offset - 1), syntax_cursor.symbol->token.bytes + syntax_cursor.offset, syntax_cursor.symbol->token.size - syntax_cursor.offset);
	syntax_cursor.symbol->token.size -= 1;
	syntax_cursor.offset -= 1;
	dont_delete_empty_name = 1;
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
apply_backspace (void)
{
	switch (syntax_cursor.symbol->token.type)
	{
	case token_type_name:
	{
		if (syntax_cursor.offset > 0)
		{
			remove_char_from_symbol ();
		}
		else
		{
			struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
			switch (prev->token.type)
			{
			case token_type_name:
			{
				ensure_capacity (prev, prev->token.size + syntax_cursor.symbol->token.size);
				memcpy (prev->token.bytes + prev->token.size, syntax_cursor.symbol->token.bytes, syntax_cursor.symbol->token.size);
				syntax_cursor.offset = prev->token.size;
				prev->token.size += syntax_cursor.symbol->token.size;
				st_take (syntax_cursor.symbol);
				assert (! syntax_cursor.symbol->sub_tree);
				del_syntax_tree (syntax_cursor.symbol);
				syntax_cursor.symbol = prev;
				dont_delete_empty_name = 1;
				rebuild_line_list ();
				line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
				resize ();
			}
			break;
			case token_type_open:
			{
				syntax_cursor.symbol = prev;
				syntax_cursor.offset = prev->token.size;
				remove_open ();
			}
			break;
			case token_type_close:
			{
				syntax_cursor.symbol = prev;
				syntax_cursor.offset = prev->token.size;
				remove_close ();
			}
			break;
			case token_type_string:
			{
				// Just move to the end of prev, don't delete the string.
				syntax_cursor.symbol = prev;
				syntax_cursor.offset = prev->token.size;
			}
			break;
			case token_type_none:
			{
				assert (prev == syntax_tree);
			}
			break;
			case token_type_comment:
			{
				assert (0);
			}
			break;
			}
		}
	}
	break;
	case token_type_open:
	{
		if (syntax_cursor.offset > 0)
		{
			remove_open ();
		}
	}
	break;
	case token_type_close:
	{
		if (syntax_cursor.offset > 0)
		{
			remove_close ();
		}
	}
	break;
	case token_type_string:
	{
		if (syntax_cursor.offset == 1 || syntax_cursor.offset == syntax_cursor.symbol->token.size)
		{
			struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
			assert (prev);
			st_take (syntax_cursor.symbol);
			del_syntax_tree (syntax_cursor.symbol);
			syntax_cursor.symbol = prev;
			syntax_cursor.offset = prev->token.size;
			rebuild_line_list ();
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		else
		{
			remove_char_from_symbol ();
		}
	}
	break;
	case token_type_none:
		assert (syntax_cursor.symbol == syntax_tree);
	break;
	case token_type_comment:
	{
		assert (0);
	}
	break;
	case token_type_break:
	{
		struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
		struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);

		st_take (syntax_cursor.symbol);
		del_syntax_tree (syntax_cursor.symbol);

		if (prev && next)
		{
			syntax_cursor.symbol = prev;
			syntax_cursor.offset = prev->token.size;

			struct syntax_tree * starting_point = get_last_syntax_tree_node (prev);
			if ((starting_point->token.type == token_type_close) && (starting_point->parent_tree != NULL))
			{
				starting_point = starting_point->parent_tree;
			}
			struct syntax_tree * to_move = next;
			while (to_move && to_move->token.type != token_type_break)
			{
				struct syntax_tree * const parent = starting_point->parent_tree;
				starting_point = move_expression_list_to_new_parent (to_move, parent, starting_point, 1);
				if ((starting_point->token.type == token_type_close) && (starting_point->parent_tree != NULL))
				{
					starting_point = starting_point->parent_tree;
				}
				to_move = starting_point;
				for (; to_move->parent_tree; to_move = to_move->parent_tree);
				to_move = to_move->next;
			}
		}

		rebuild_line_list ();
		line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
		resize ();
	}
	break;
	}

	if (syntax_cursor.symbol == syntax_tree)
	{
		struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);
		if (next)
		{
			syntax_cursor.symbol = next;
			syntax_cursor.offset = 0;
		}
	}
}

void
insert_char_to_symbol (char const c)
{
	ensure_capacity (syntax_cursor.symbol, syntax_cursor.symbol->token.size + 1);
	memmove (syntax_cursor.symbol->token.bytes + (syntax_cursor.offset + 1), syntax_cursor.symbol->token.bytes + syntax_cursor.offset, syntax_cursor.symbol->token.size - syntax_cursor.offset);
	syntax_cursor.symbol->token.bytes [syntax_cursor.offset] = (char) c;
	syntax_cursor.symbol->token.size += 1;
	syntax_cursor.offset += 1;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
insert_char_to_name (char const c)
{
	assert (syntax_cursor.symbol->token.type == token_type_name);
	insert_char_to_symbol (c);
}

void
insert_char_to_string (char const c)
{
	assert (syntax_cursor.symbol->token.type == token_type_string);
	assert (syntax_cursor.offset > 0);
	assert (syntax_cursor.offset < syntax_cursor.symbol->token.size);
	insert_char_to_symbol (c);
}

void
insert_after_root (enum token_type const type,  char const c)
{
	assert (syntax_cursor.symbol == syntax_tree);
	assert (type != token_type_none);
	assert (syntax_tree->parent_tree == NULL);
	assert (! syntax_tree->next && ! syntax_tree->prev);
	insert_after_node (type, c, syntax_tree);
}

void
insert_after_last (enum token_type const type, int const c)
{
	assert (type != token_type_none);
	struct syntax_tree * const new_node = new_syntax_tree (make_token (type, NULL, 0, 0, 0, 0), NULL);
	ensure_capacity (new_node, 1);
	new_node->token.bytes [0] = (type == token_type_open ? open_brace_char : (type == token_type_close ? close_brace_char : (char) c));
	new_node->token.size = 1;
	if (type == token_type_open)
	{
		new_node->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), new_node);
	}
	switch (syntax_cursor.symbol->token.type)
	{
	case token_type_open:
	{
		assert (syntax_cursor.symbol->sub_tree);
		st_add (syntax_cursor.symbol->sub_tree, new_node);
		new_node->parent_tree = syntax_cursor.symbol;
	}
	break;
	case token_type_close:
	{
		if (syntax_cursor.symbol->parent_tree)
		{
			st_add (syntax_cursor.symbol->parent_tree, new_node);
			new_node->parent_tree = syntax_cursor.symbol->parent_tree->parent_tree;
		}
		else
		{
			st_add (syntax_cursor.symbol, new_node);
			new_node->parent_tree = syntax_cursor.symbol->parent_tree;
		}
	}
	break;
	case token_type_string:
	case token_type_name:
	{
		st_add (syntax_cursor.symbol, new_node);
		new_node->parent_tree = syntax_cursor.symbol->parent_tree;
	}
	break;
	case token_type_none:
		assert (0);
	break;
	case token_type_comment:
	{
		assert (0);
	}
	break;
	}

	syntax_cursor.symbol = new_node;
	syntax_cursor.offset = 1;
	rebuild_line_list ();
	line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
	resize ();
}

void
insert_char_after_brace (char const c)
{
	assert (syntax_cursor.offset == 1);
	assert (syntax_cursor.symbol->token.type == token_type_open || syntax_cursor.symbol->token.type == token_type_close);
	struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);
	if (next)
	{
		switch (next->token.type)
		{
		case token_type_name:
		{
			if (syntax_cursor.symbol->token.type == token_type_open)
			{
				syntax_cursor.symbol = next;
				syntax_cursor.offset = 0;
				insert_char_to_name ((char) c);
			}
			else
			{
				assert (next->prev);
				insert_after_node (token_type_name, c, next->prev);
			}
		}
		break;
		case token_type_open:
		{
			struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_name, NULL, 0, 0, 0, 0), next->parent_tree);
			ensure_capacity (new_node, 1);
			new_node->token.bytes [0] = c;
			new_node->token.size = 1;
			assert (next->prev);
			st_add (next->prev, new_node);
			syntax_cursor.symbol = new_node;
			syntax_cursor.offset = 1;
			rebuild_line_list ();
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		break;
		case token_type_close:
		{
			struct syntax_tree * const prev = next->prev;
			assert (prev);
			struct syntax_tree * const parent = prev->parent_tree;
			struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_name, NULL, 0, 0, 0, 0), parent);
			ensure_capacity (new_node, 1);
			new_node->token.bytes [0] = c;
			new_node->token.size = 1;
			st_add (prev, new_node);
			syntax_cursor.symbol = new_node;
			syntax_cursor.offset = 1;
			rebuild_line_list ();
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		break;
		case token_type_string:
		{
			assert (next->prev);
			insert_after_node (token_type_name, c, next->prev);
		}
		break;
		case token_type_none:
			assert (0);
		break;
		case token_type_comment:
		{
			assert (0);
		}
		break;
		}
	}
	else
	{
		insert_after_last (token_type_name, c);
	}
}

void
move_right (void)
{
	if (line_cursor.x < maxx)
	{
		line_cursor.x += 1;
		syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);
		line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
		redisplay ();
	}
}

void
remove_if_empty_name (struct syntax_tree * const node)
{
	if (node && node->token.type == token_type_name && node->token.size == 0)
	{
		struct syntax_tree * prev = find_previous_symbol (syntax_cursor.symbol);
		assert (prev);
		syntax_cursor.symbol = prev;
		syntax_cursor.offset = prev->token.size;
		st_take (node);
		del_syntax_tree (node);
		rebuild_line_list ();
		line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
		resize ();
	}
}

void
insert_char_to_any (int const c)
{
	switch (syntax_cursor.symbol->token.type)
	{
	case token_type_name:
	{
		insert_char_to_name ((char) c);
	}
	break;
	case token_type_open:
	{
		if (syntax_cursor.offset == 0)
		{
			// move the cursor to the end of the previous symbol and call the else of this if.
			struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
			assert (prev);
			switch (prev->token.type)
			{
			case token_type_name:
			{
				syntax_cursor.symbol = prev;
				syntax_cursor.offset = prev->token.size;
				insert_space ();
				insert_char_to_name (c);
			}
			break;
			case token_type_none:
			{
				syntax_cursor.symbol = prev;
				syntax_cursor.offset = 0;
				assert (prev == syntax_tree);
				insert_after_node (token_type_name, (char) c, prev);
			}
			break;
			case token_type_open:
			case token_type_close:
			{
				syntax_cursor.symbol = prev;
				syntax_cursor.offset = prev->token.size;
				insert_char_after_brace (c);
			}
			break;
			case token_type_string:
			{
				insert_after_node (token_type_name, c, prev);
			}
			break;
			case token_type_comment:
			{
				assert (0);
			}
			break;
			}
		}
		else
		{
			insert_char_after_brace (c);
		}
	}
	break;
	case token_type_close:
	{
		if (syntax_cursor.offset == 0)
		{
			// move the cursor to the end of the previous symbol and call the else of this if.
			struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
			assert (prev);
			syntax_cursor.symbol = prev;
			syntax_cursor.offset = prev->token.size;
			insert_char_to_any (c);
		}
		else
		{
			insert_char_after_brace (c);
		}
	}
	break;
	case token_type_none:
	{
		insert_after_root (token_type_name, (char) c);
	}
	break;
	case token_type_string:
	{
		if (syntax_cursor.offset == 0)
		{
			assert (syntax_cursor.symbol->prev);
			insert_after_node (token_type_name, c, syntax_cursor.symbol->prev);
		}
		else if (syntax_cursor.offset == syntax_cursor.symbol->token.size)
		{
			insert_after_node (token_type_name, c, syntax_cursor.symbol);
		}
		else
		{
			insert_char_to_string (c);
		}
	}
	break;
	case token_type_comment:
	{
		assert (0);
	}
	break;
	}
}

void
insert_double_quote_to_any (void)
{
	switch (syntax_cursor.symbol->token.type)
	{
	case token_type_name:
	case token_type_string:
	{
		if (syntax_cursor.offset == 0)
		{
			struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
			assert (prev);
			syntax_cursor.symbol = prev;
			syntax_cursor.offset = prev->token.size;
			insert_double_quote_to_any ();
		}
		else if (syntax_cursor.offset == syntax_cursor.symbol->token.size)
		{
			empty_string_after_node (syntax_cursor.symbol);
		}
		else
		{
			if (syntax_cursor.symbol->token.type == token_type_name)
			{
				insert_space ();
				insert_double_quote_to_any ();
			}
			else
			{
				char const * const bytes = syntax_cursor.symbol->token.bytes;
				char const * cp = bytes + syntax_cursor.offset - 1;
				size_t backslash_count = 0;
				for (; cp > bytes && (* cp == '\\'); ++ backslash_count, -- cp);
				if (backslash_count % 2)
				{
					insert_char_to_string ('"');
				}
				else
				{
					size_t const to_move = (syntax_cursor.symbol->token.size - syntax_cursor.offset) - 1;
					if (to_move > 0)
					{
						struct syntax_tree * const new_node = new_syntax_tree (make_token (token_type_string, NULL, 0, 0, 0, 0), syntax_cursor.symbol->parent_tree);
						ensure_capacity (new_node, to_move + 2);
						memcpy (new_node->token.bytes + 1, syntax_cursor.symbol->token.bytes + syntax_cursor.offset, to_move);
						new_node->token.bytes[0] = '"';
						new_node->token.bytes[to_move + 1] = '"';
						new_node->token.size = to_move + 2;
						syntax_cursor.symbol->token.bytes[syntax_cursor.offset] = '"';
						syntax_cursor.symbol->token.size = syntax_cursor.offset + 1;
						st_add (syntax_cursor.symbol, new_node);
						syntax_cursor.symbol = new_node;
						syntax_cursor.offset = 0;
					}
					else
					{
						syntax_cursor.offset = syntax_cursor.symbol->token.size;
					}
					insert_double_quote_to_any ();
				}
			}
		}
	}
	break;
	case token_type_none:
	{
		assert (syntax_cursor.symbol == syntax_tree);
		empty_string_after_node (syntax_cursor.symbol);
	}
	break;
	case token_type_open:
	{
		if (syntax_cursor.symbol->sub_tree == syntax_cursor.symbol->sub_tree->next)
		{
			empty_string_after_node (syntax_cursor.symbol->sub_tree);
		}
		else
		{
			syntax_cursor.symbol = syntax_cursor.symbol->sub_tree->next;
			syntax_cursor.offset = 0;
			insert_double_quote_to_any ();
		}
	}
	break;
	case token_type_close:
	{
		empty_string_after_node (syntax_cursor.symbol->parent_tree);
	}
	break;
	case token_type_comment:
	{
		assert (0);
	}
	break;
	}
}

int
main (int const argc, char const * const * const argv)
{
	init_ncurses ();

	if (argc < 2)
	{
		token_list = lex (default_file_content, 0);
		syntax_tree = parse (token_list);
		rebuild_line_list ();
		syntax_cursor.symbol = syntax_tree;
		syntax_cursor.offset = 0;
		line_cursor.x = 0;
		line_cursor.y = 0;
		resize ();
		write_meta (line_cursor, syntax_cursor);
		move_cursor (line_cursor);
		refresh ();
	}

	assert (argc < 3);

	if (argc == 2)
	{
		open_file (argv [1]);
		resize ();
		write_meta (line_cursor, syntax_cursor);
		move_cursor (line_cursor);
		refresh ();
	}

	int c = 0;
	while ((c = getch ()) != 17) // ctrl-q
	{
		if (0)
		{
			printw ("%s - %d\n", keyname (c), c);
			continue;
		}
		struct syntax_tree * const last_pos = syntax_cursor.symbol;
		dont_delete_empty_name = 0;
		switch (c)
		{
		case KEY_DC:
		{
			move_right ();
			apply_backspace ();
		}
		break;
		case KEY_MOUSE:
		{
			if (getmouse (& mouse) == OK)
			{
				line_cursor.y = mouse.y;
				line_cursor.x = mouse.x;
				syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);
				line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
				redisplay ();
			}
		}
		break;
		case 2: // ctrl-b
		{
			if (0)
			{
				if (! build_command)
				{
					prompt_build_command ();
				}
				run_build_command ();
			}
			else
			{
				build_this_file ();
			}
		}
		break;
		case 4: // ctrl-d
		{
			toggle_light_dark ();
		}
		break;
		case 6: // ctrl-f
		{
			prompt_find ();
		}
		break;
		case 7: // ctrl-g
		{
			editor_find_next ();
		}
		break;
		case 14: // ctrl-n
		{
			new_file ();
		}
		break;
		case 15: // ctrl-o
		{
			prompt_open_file ();
		}
		break;
		case 11: // ctrl-k
		{
			prompt_build_command ();
		}
		break;
		case 18: // ctrl-r
		{
			prompt_run_command ();
		}
		break;
		case 19: // ctrl-s
		{
			prompt_write_file ();
		}
		break;
		case KEY_PPAGE:
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			if (line_offset >= maxy)
			{
				line_offset -= maxy;
			}
			else
			{
				size_t const remaining_y = maxy - line_offset;
				line_offset = 0;
				if (line_cursor.y >= remaining_y)
				{
					line_cursor.y -= remaining_y;
				}
				else
				{
					line_cursor.y = 0;
				}
			}
			syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		break;
		case KEY_NPAGE:
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			size_t const last_line = line_count (line_list);
			if (last_line > maxy)
			{
				size_t const max_offset = (last_line - maxy);
				if ((max_offset - line_offset) >= maxy)
				{
					line_offset += maxy;
				}
				else
				{
					size_t const remaining_y = maxy - (max_offset - line_offset);
					line_offset = max_offset;
					if ((maxy - line_cursor.y) >= remaining_y)
					{
						line_cursor.y += remaining_y;
					}
					else
					{
						line_cursor.y = maxy;
					}
				}
			}
			else
			{
				line_cursor.y = last_line;
			}
			syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		break;
		case 519: // ctrl-del
		{
			if (syntax_cursor.offset < syntax_cursor.symbol->token.size)
			{
				if (syntax_cursor.offset > 0)
				{
					if (syntax_cursor.symbol->token.type == token_type_name)
					{
						insert_space ();
					}
					else if (syntax_cursor.symbol->token.type == token_type_string)
					{
						syntax_cursor.symbol->token.bytes[syntax_cursor.offset] = '"';
						syntax_cursor.symbol->token.size = syntax_cursor.offset + 1;
					}
				}
				syntax_cursor.offset = syntax_cursor.symbol->token.size;
			}
			else
			{
				struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);
				if (next)
				{
					syntax_cursor.symbol = next;
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
				}
				else
				{
					continue;
				}
			}
			switch (syntax_cursor.symbol->token.type)
			{
			case token_type_open:
			case token_type_close:
			{
				apply_backspace ();
			}
			break;
			case token_type_name:
			case token_type_string:
			{
				struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
				assert (prev);
				st_take (syntax_cursor.symbol);
				del_syntax_tree (syntax_cursor.symbol);
				syntax_cursor.symbol = prev;
				syntax_cursor.offset = prev->token.size;
			}
			break;
			case token_type_none:
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			break;
			case token_type_comment:
			{
				assert (0);
			}
			break;
			}
			rebuild_line_list ();
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		break;
		case 545: // ctrl-left
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			else if (syntax_cursor.offset > 0)
			{
				syntax_cursor.offset = 0;
			}
			else
			{
				struct syntax_tree * const prev = find_previous_symbol (syntax_cursor.symbol);
				if (prev && prev->token.type != token_type_none)
				{
					syntax_cursor.symbol = prev;
					syntax_cursor.offset = 0;
				}
				
			}
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			redisplay ();
		}
		break;
		case 560: // ctrl-right
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			else if (syntax_cursor.offset < syntax_cursor.symbol->token.size)
			{
				syntax_cursor.offset = syntax_cursor.symbol->token.size;
			}
			else
			{
				struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);
				if (next)
				{
					syntax_cursor.symbol = next;
					syntax_cursor.offset = next->token.size;
				}
				else
				{
					continue;
				}
				
			}
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			redisplay ();
		}
		break;
		case 566: // ctrl-up
		{
			if (syntax_cursor.symbol->token.type == token_type_open)
			{
				syntax_cursor.offset = 0;
			}
			else
			{
				if (syntax_cursor.symbol->parent_tree)
				{
					syntax_cursor.symbol = syntax_cursor.symbol->parent_tree;
					syntax_cursor.offset = 0;
				}
			}
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			if (line_cursor.y < 0)
			{
				line_offset += line_cursor.y;
				line_cursor.y = 0;
			}
			redisplay ();
		}
		break;
		case 525: // ctrl-down
		{
			if (syntax_cursor.symbol->token.type == token_type_open)
			{
				syntax_cursor.symbol = st_tail (syntax_cursor.symbol->sub_tree);
				syntax_cursor.offset = syntax_cursor.symbol->token.size;
			}
			else
			{
				if (syntax_cursor.symbol->parent_tree)
				{
					syntax_cursor.symbol = st_tail (syntax_cursor.symbol->parent_tree->sub_tree);
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
				}
			}
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			if (line_cursor.y > maxy)
			{
				line_offset += (line_cursor.y - maxy) + 1;
				line_cursor.y = maxy - 1;
			}
			redisplay ();
		}
		break;
		case KEY_RESIZE:
		{
			dont_delete_empty_name = 1;
			resize ();
		}
		break;
		case KEY_LEFT:
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			else if (line_cursor.x > 0)
			{
				line_cursor.x -= 1;
				syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);
				line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
				redisplay ();
			}
		}
		break;
		case KEY_RIGHT:
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			move_right ();
		}
		break;
		case KEY_UP:
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			else if (line_cursor.y > 0)
			{
				line_cursor.y -= 1;
			}
			else if (line_offset > 0)
			{
				-- line_offset;
			}
			syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		break;
		case KEY_DOWN:
		{
			if (syntax_cursor.symbol->token.type == token_type_none)
			{
				assert (syntax_cursor.symbol == syntax_tree);
				continue;
			}
			else if (line_cursor.y < maxy)
			{
				line_cursor.y += 1;
			}
			else
			{
				size_t const count = line_count (line_list);
				if (count > maxy && line_offset < (count - maxy))
				{
					++ line_offset;
				}
			}
			syntax_cursor = line_to_syntax (line_list, syntax_tree, line_cursor, line_offset);
			line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
			resize ();
		}
		break;
		case KEY_END:
		{
			struct line_list * const line = find_line_for_symbol (line_list, syntax_cursor.symbol);
			if (line)
			{
				syntax_cursor.symbol = line->end;
				syntax_cursor.offset = syntax_cursor.symbol->token.size;
				line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
				redisplay ();
			}
		}
		break;
		case KEY_HOME:
		{
			struct line_list * const line = find_line_for_symbol (line_list, syntax_cursor.symbol);
			if (line)
			{
				syntax_cursor.symbol = line->begin;
				syntax_cursor.offset = 0;
				line_cursor = syntax_to_line (line_list, syntax_cursor, line_offset);
				redisplay ();
			}
		}
		break;
		case KEY_BACKSPACE:
		{
			apply_backspace ();
		}
		break;
		case ' ':
		{
			if (syntax_cursor.symbol->token.type == token_type_string)
			{
				if (syntax_cursor.offset > 0 && syntax_cursor.offset < syntax_cursor.symbol->token.size)
				{
					insert_char_to_string (c);
				}
			}
			else
			{
				dont_delete_empty_name = 1;
				if (syntax_cursor.symbol->token.type == token_type_name && syntax_cursor.offset > 0)
				{
					insert_space ();
				}
			}
		}
		break;
		case '(':
		case '[':
		{
			switch (syntax_cursor.symbol->token.type)
			{
			case token_type_name:
			{
				if (syntax_cursor.offset == 0)
				{
					// Insert brace before.
					insert_open ();
				}
				else if (syntax_cursor.offset < syntax_cursor.symbol->token.size)
				{
					// split into two
					insert_space ();
					insert_open ();
				}
				else
				{
					// Insert brace after.
					struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);
					if (next)
					{
						syntax_cursor.symbol = next;
						syntax_cursor.offset = 0;
						insert_open ();
					}
					else
					{
						insert_after_last (token_type_open, 0);
					}
				}
			}
			break;
			case token_type_open:
			case token_type_close:
			{
				if (syntax_cursor.offset > 0)
				{
					struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);
					if (next)
					{
						syntax_cursor.symbol = next;
						syntax_cursor.offset = 0;
						insert_open ();
					}
					else
					{
						insert_after_last (token_type_open, 0);
					}
				}
				else
				{
					insert_open ();
				}
			}
			break;
			case token_type_none:
			{
				insert_after_root (token_type_open, 0);
			}
			break;
			case token_type_string:
			{
				if (syntax_cursor.offset == 0)
				{
					insert_open ();
				}
				else if (syntax_cursor.offset == syntax_cursor.symbol->token.size)
				{
					struct syntax_tree * const next = find_next_symbol (syntax_cursor.symbol);
					if (next)
					{
						syntax_cursor.symbol = next;
						syntax_cursor.offset = 0;
						insert_open ();
					}
					else
					{
						insert_after_last (token_type_open, 0);
					}
				}
				else
				{
					insert_char_to_string (c);
				}
			}
			break;
			case token_type_comment:
			{
				assert (0);
			}
			break;
			}
		}
		break;
		case ')':
		case ']':
		{
			switch (syntax_cursor.symbol->token.type)
			{
			case token_type_name:
			{
				if (syntax_cursor.offset == 0)
				{
					// Insert brace before.
					syntax_cursor.symbol = find_previous_symbol (syntax_cursor.symbol);
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
					insert_close ();
				}
				else if (syntax_cursor.offset < syntax_cursor.symbol->token.size)
				{
					// split into two
					insert_space ();
					syntax_cursor.symbol = find_previous_symbol (syntax_cursor.symbol);
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
					insert_close ();
				}
				else
				{
					// Insert brace after.
					insert_close ();
				}
			}
			break;
			case token_type_open:
			case token_type_close:
			{
				insert_close ();
			}
			break;
			case token_type_none:
			{
				insert_after_root (token_type_close, 0);
			}
			break;
			case token_type_string:
			{
				if (syntax_cursor.offset == 0)
				{
					// Insert brace before.
					syntax_cursor.symbol = find_previous_symbol (syntax_cursor.symbol);
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
					insert_close ();
				}
				else if (syntax_cursor.offset < syntax_cursor.symbol->token.size)
				{
					insert_char_to_any (c);
				}
				else
				{
					// Insert brace after.
					insert_close ();
				}
			}
			break;
			case token_type_comment:
			{
				assert (0);
			}
			break;
			}
		}
		break;
		case '"':
		{
			insert_double_quote_to_any ();
		}
		break;
		case '\n':
		{
			switch (syntax_cursor.symbol->token.type)
			{
			case token_type_name:
			{
				if (syntax_cursor.offset == 0)
				{
					syntax_cursor.symbol = find_previous_symbol (syntax_cursor.symbol);
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
					insert_break ();
				}
				else if (syntax_cursor.offset < syntax_cursor.symbol->token.size)
				{
					insert_space ();
					syntax_cursor.symbol = find_previous_symbol (syntax_cursor.symbol);
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
					insert_break ();
				}
				else
				{
					insert_break ();
				}
			}
			break;
			case token_type_open:
			case token_type_close:
			{
				insert_break ();
			}
			break;
			case token_type_break:
			case token_type_none:
			{
				continue;
			}
			break;
			case token_type_string:
			{
				if (syntax_cursor.offset == 0)
				{
					syntax_cursor.symbol = find_previous_symbol (syntax_cursor.symbol);
					syntax_cursor.offset = syntax_cursor.symbol->token.size;
					insert_break ();
				}
				else if (syntax_cursor.offset < syntax_cursor.symbol->token.size)
				{
#warning Implement this.
					assert (0);
				}
				else
				{
					insert_break ();
				}
			}
			break;
			case token_type_comment:
			{
				assert (0);
			}
			break;
			}
		}
		break;
		default:
		{
			if (isprint ((char) c))
			{
				insert_char_to_any (c);
			}
		}
		}
		if (! dont_delete_empty_name)
		{
			remove_if_empty_name (last_pos);
		}
		write_meta (line_cursor, syntax_cursor);
		move_cursor (line_cursor);
		refresh ();
	}
	endwin ();

	del_line_list (line_list);
	del_syntax_tree (syntax_tree);
	del_token_list (token_list);

	return 0;
}
