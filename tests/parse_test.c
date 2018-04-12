#include <stdio.h>
#include <string.h>

#include "parse.h"

static
void
print_symbols_recursive (int const level, struct syntax_tree * const syntax_tree)
{
	struct syntax_tree * it = syntax_tree->next;
	for (; it; it = it->next)
	{
		int i;
		for (i = 0; i < level; ++ i)
		{
			putchar(' ');
		}

		printf ("%.*s!\n", (int) it->token.size, it->token.bytes);

		if (it->sub_tree)
		{
			print_symbols_recursive (level + 1, it->sub_tree);
		}
	}
}

static
int
test_template (char * const file_content)
{
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

	print_symbols_recursive (0, syntax_tree);

	del_syntax_tree (syntax_tree);
	del_token_list (token_list);

	return 0;
}

// Test matched parenthesis case.
static
int
test_1 (void)
{
	return test_template ("(foo bar (baz foo bar) ((baz foo (bar baz))))");
}

// Test more closes than opens.
static
int
test_2 (void)
{
	return test_template ("(foo bar (baz foo bar) ((baz foo (bar baz))))))))) asdf)");
}

// Test more opens than closes.
static
int
test_3 (void)
{
	return test_template ("(foo bar (baz foo bar) ((baz foo (bar baz");
}

int
main (void)
{
	int err = 0;

	err |= test_1 ();
	err |= test_2 ();
	err |= test_3 ();

	return err;
}
