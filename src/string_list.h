/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include <stddef.h>

#include "lex.h"

struct string_list
{
	struct string_list * prev;
	struct string_list * next;

	char * string;
	size_t size;
	size_t capacity;
};

struct string_list *
new_string_list (char * const string, size_t const size, size_t const capacity);

void
del_string_list (struct string_list * const list);

struct string_list *
append_string_list (struct string_list * const list, struct string_list * const node);

void
prefix_string_list (struct string_list * const list, struct string_list * const node);

void
take_string_list (struct string_list * const node);

struct string_list *
sl_splice (struct string_list * const dest, struct string_list * const src);

struct string_list *
sl_copy_str (char const * const string);

struct string_list *
sl_copy_t (struct token const t);

struct string_list *
sl_copy_append (struct string_list * const list, char const * const string);

struct string_list *
sl_copy_append_t (struct string_list * const list, struct token const t);

struct string_list *
sl_copy_append_uint (struct string_list * const list, unsigned const u);

struct string_list *
sl_copy_append_strref (struct string_list * const list, char const * const s, size_t const size);

char *
sl_uint (unsigned const u);

char *
sl_cat (char const * const a, char const * const b);

size_t
sl_fold (char ** const dest_ptr, struct string_list * const sl);

char const *
sl_find (struct string_list * map, char const * key, size_t const * len);
