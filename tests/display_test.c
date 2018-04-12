#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "display.h"

void
write_stdout (void * data, char const * const output, size_t const length, struct syntax_tree * const node)
{
	(void) data;
	(void) node;
	printf ("%.*s", (int) length, output);
}

void
write_stdout_plain (void * data, char const * const output, size_t const length)
{
	write_stdout (data, output, length, NULL);
}

int
test_display_syntax_tree_minimal (void)
{
	printf ("test_display_syntax_tree_minimal\n");

	char * const file_content = "(foo bar (baz foo) bar ((baz foo (bar baz))))";

	struct token_list * const token_list = lex (file_content, strlen (file_content));
	if (! token_list)
	{
		return 1;
	}

	struct syntax_tree * const syntax_tree = parse (token_list);
	if (! syntax_tree)
	{
		return 1;
	}

	display_syntax_tree_minimal (syntax_tree, write_stdout_plain, NULL);

	putchar ('\n');

	del_syntax_tree (syntax_tree);
	del_token_list (token_list);

	return 0;
}

int
test_display_syntax_tree_single_line (void)
{
	printf ("test_display_syntax_tree_single_line\n");

	char * const file_content = "(foo bar (baz foo) bar ((baz foo (bar baz))))";

	struct token_list * const token_list = lex (file_content, strlen (file_content));
	if (! token_list)
	{
		return 1;
	}

	struct syntax_tree * const syntax_tree = parse (token_list);
	if (! syntax_tree)
	{
		return 1;
	}

	display_syntax_tree_single_line (syntax_tree, NULL, 0, write_stdout, NULL);
	putchar ('\n');

	del_syntax_tree (syntax_tree);
	del_token_list (token_list);

	return 0;
}

static
void
print_line_list (struct line_list * const line_list_root)
{
	display_line_list (line_list_root, NULL, NULL, write_stdout, NULL);
}

int
test_fit_line_list_to_columns ()
{
	printf ("test_5\n");

	char * const file_content = "(aaa bbb (ccc ddd) eee ((fff ggg (hhh iii)) jjj kkk)) (lll mmm)";

	struct token_list * const token_list = lex (file_content, strlen (file_content));
	if (! token_list)
	{
		return 1;
	}

	struct syntax_tree * const syntax_tree = parse (token_list);
	if (! syntax_tree)
	{
		return 1;
	}

	struct line_list * line_list = split_top_level_expressions (syntax_tree);

	fit_line_list_to_columns (line_list, 30);

	print_line_list (line_list);
	putchar ('\n');

	del_line_list (line_list);
	del_syntax_tree (syntax_tree);
	del_token_list (token_list);

	return 0;
}

int
main (void)
{
	int err = 0;
	
	err |= test_display_syntax_tree_minimal ();
	err |= test_display_syntax_tree_single_line ();
	err |= test_fit_line_list_to_columns ();

	return err;
}
