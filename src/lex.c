/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

// Allow user to customize braces.
char
open_brace_char  = '(',
close_brace_char = ')',
open_comment_char = '{',
close_comment_char = '}',
break_char = '\f';

struct token
make_token (enum token_type const type, char * const bytes, size_t const size, size_t const index, size_t const line, size_t const column)
{
	struct token ret;
	ret.type = type;
	ret.bytes = bytes;
	ret.size = size;
	ret.index = index;
	ret.line = line;
	ret.column = column;
	ret.capacity = 0;
	return ret;
}

static
struct token_list *
new_token_list (enum token_type const type, char * const bytes, size_t const size, size_t const index, size_t const line, size_t const column)
{
	struct token_list * ret = (struct token_list *) malloc (sizeof (struct token_list));
	ret->next = NULL;
	ret->token = make_token (type, bytes, size, index, line, column);
	return ret;
}

void
del_token_list (struct token_list * const token_list)
{
	struct token_list * it, * next;
	for (it = token_list; it; it = next)
	{
		next = it->next;
		if (it->token.capacity)
		{
			free (it->token.bytes);
		}
		free (it);
	}
}

int
token_equal (struct token const a, struct token const b)
{
	if ((a.type == token_type_open) && (b.type == token_type_open))
	{
		return 1;
	}
	if ((a.type == token_type_close) && (b.type == token_type_close))
	{
		return 1;
	}
	return a.type == b.type && a.size == b.size && ! strncmp (a.bytes, b.bytes, a.size);
}

static
int
is_lexical_space (int const c)
{
	return c != '\f' && isspace (c);
}

// Returns NULL if no tokens found.
struct token_list *
lex (char * const content, size_t const size)
{
	char * token_start = NULL;
	char * token_ptr   = NULL;
	char const * const content_end = content + size;

	struct token_list * token_list = new_token_list (token_type_none, NULL, 0, 0, 0, 0);
	if (size == 0)
	{
		return token_list;
	}

	struct token_list * end = token_list;
	struct token_list * next = NULL;

	size_t index = 0;
	size_t line = 0;
	size_t column = 0;
	size_t start_index = 0;
	size_t start_line = 0;
	size_t start_column = 0;
	size_t prev_line_width = 0;

	int prev_was_unpaired_backslash = 0;
	for (token_ptr = content; token_ptr <= content_end; ++token_ptr)
	{
		if (* token_ptr == '\n')
		{
			++ line;
			prev_line_width = column;
			column = 0;
		}
		else
		{
			++ column;
		}
		++ index;

		if (token_start)
		{
			if (*token_start == '"')
			{
				if (((*token_ptr == '"') && (! prev_was_unpaired_backslash)) || (token_ptr == content_end))
				{
					char * const end = token_ptr + 1;
					next = new_token_list (token_type_string, token_start, (end - token_start), start_index, start_line, start_column);
					token_start = NULL;
				}
				else
				{
					prev_was_unpaired_backslash = ((*token_ptr == '\\') && (! prev_was_unpaired_backslash));
				}
			}
			else if (*token_start == open_comment_char)
			{
				if ((*token_ptr == close_comment_char) || (token_ptr == content_end))
				{
					char * const end = token_ptr + 1;
					next = new_token_list (token_type_comment, token_start, (end - token_start), start_index, start_line, start_column);
					token_start = NULL;
				}
			}
			else if (token_ptr == content_end)
			{
				while (token_start < content_end && is_lexical_space (* token_start))
				{
					++ token_start;
				}
				while (token_start < content_end && is_lexical_space (* token_ptr))
				{
					-- token_ptr;
				}
				if (token_start < content_end)
				{
					end->next = new_token_list (token_type_name, token_start, token_ptr - token_start, start_index, start_line, start_column);
				}
				break;
			}
			else if (is_lexical_space (*token_ptr) || (*token_ptr == open_brace_char) || (*token_ptr == close_brace_char) || (*token_ptr == '\f') || (*token_ptr == '"') || (*token_ptr == open_comment_char))
			{
				next = new_token_list (token_type_name, token_start, token_ptr - token_start, start_index, start_line, start_column);
				token_start = NULL;
				-- token_ptr;
				if (*token_ptr == '\n')
				{
					-- line;
					column = prev_line_width;
				}
				else
				{
					-- column;
				}
				-- index;
			}
		}
		else if (*token_ptr == open_brace_char)
		{
			next = new_token_list (token_type_open, token_ptr, 1, index, line, column);
			token_start = NULL;
		}
		else if (*token_ptr == close_brace_char)
		{
			next = new_token_list (token_type_close, token_ptr, 1, index, line, column);
			token_start = NULL;
		}
		else if (*token_ptr == '\f')
		{
			next = new_token_list (token_type_break, token_ptr, 1, index, line, column);
			token_start = NULL;
		}
		else if (*token_ptr == '"')
		{
			token_start = token_ptr;
			start_index = index;
			start_line = line;
			start_column = column;
		}
		else if (*token_ptr == open_comment_char)
		{
			token_start = token_ptr;
			start_index = index;
			start_line = line;
			start_column = column;
		}
		else if (! is_lexical_space (*token_ptr))
		{
			token_start = token_ptr;
			start_index = index;
			start_line = line;
			start_column = column;
		}

		if (next)
		{
			end->next = next;
			end = next;
			next = NULL;
		}
	}

	return token_list;
}

int
t_strcmp (struct token t, char const * const s)
{
	if (strlen (s) != t.size)
	{
		return 1;
	}
	return strncmp (t.bytes, s, t.size);
}

int
t_strncmp (struct token t, char const * const s, size_t const l)
{
	return strncmp (t.bytes, s, t.size < l ? t.size : l);
}

struct token
t_append (struct token t, char const * const s)
{
	size_t const slen = strlen (s);
	size_t const size = t.size + slen;
	char * const bytes = (char *) malloc (size);
	memcpy (bytes, t.bytes, t.size);
	memcpy (bytes + t.size, s, slen);
	return make_token (t.type, bytes, size, 0, 0, 0);
}

struct token
t_prefix (char const * const s, struct token t)
{
	size_t const slen = strlen (s);
	size_t const size = t.size + slen;
	char * const bytes = (char *) malloc (size);
	memcpy (bytes, s, slen);
	memcpy (bytes + slen, t.bytes, t.size);
	return make_token (t.type, bytes, size, 0, 0, 0);
}

struct token
make_str_token (char const * const str)
{
	return make_token (token_type_name, strdup (str), strlen (str), 0, 0, 0);
}

struct token
subtoken (struct token const t, size_t const offset, size_t const * len)
{
	size_t const size = len ? * len : t.size - offset;
	char * const bytes = strndup (t.bytes + offset, size);
	struct token t2 = make_token (t.type, bytes, size, 0, 0, 0);
	return t2;
}
