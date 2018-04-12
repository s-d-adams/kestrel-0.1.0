/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include <ctype.h>
#include <sys/types.h>

extern char
open_brace_char,
close_brace_char,
open_comment_char,
close_comment_char,
break_char;

enum token_type
{
	token_type_none,   // Used for list root nodes.
	token_type_open,   // Open bracket.
	token_type_close,  // Close bracket.
	token_type_name,   // Symbol name.
	token_type_string, // String literal.
	token_type_comment,
	token_type_break   // A form-feed character, used to force separation between incomplete top-level expressions.
};

struct token
{
	enum token_type type;
	char * bytes;
	size_t size;
	size_t capacity;

	size_t index;
	size_t line;
	size_t column;
};

struct token
make_token (enum token_type type, char * bytes, size_t size, size_t index, size_t line, size_t column);

struct token
make_str_token (char const * const str);

struct token_list
{
	struct token_list * next;
	struct token token;
};

void
del_token_list (struct token_list * const token_list);

int
token_equal (struct token a, struct token b);

// Returns NULL if no tokens found.
struct token_list *
lex (char * const content, size_t const size);

int
t_strcmp (struct token t, char const * const s);

int
t_strncmp (struct token t, char const * const s, size_t const l);

struct token
t_append (struct token t, char const * const s);

struct token
t_prefix (char const * const s, struct token t);

struct token
subtoken (struct token const t, size_t const offset, size_t const * len);
