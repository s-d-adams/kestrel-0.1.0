#include <stdio.h>
#include <string.h>

#include "lex.h"

// Lexes the content, then prints each token on its own line with an '!' character at the end of each to make visible any unintended whitespace.
static
int
test_1 (void)
{
	static char * const file_content =
	"(foo bar (baz foo bar) ((baz foo (bar baz))))))))))) asdf)";

	struct token_list * const token_list = lex (file_content, strlen (file_content));
	if (! token_list)
	{
		return 1;
	}

	struct token_list * tmp = token_list;
	for (; tmp; tmp = tmp->next)
	{
		printf ("%.*s!\n", (int) tmp->token.size, tmp->token.bytes);
	}

	del_token_list (token_list);

	return 0;
}

int
main (void)
{
	int err = 0;

	err &= test_1 ();

	return err;
}
