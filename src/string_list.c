/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "string_list.h"
#include "lex.h"

struct string_list *
new_string_list (char * const string, size_t const size, size_t const capacity)
{
	struct string_list * const ret = (struct string_list *) malloc (sizeof (struct string_list));
	ret->prev = NULL;
	ret->next = NULL;
	ret->string = string;
	ret->size = size;
	ret->capacity = capacity;
	return ret;
}

void
del_string_list (struct string_list * const list)
{
	if (list->capacity)
	{
		free (list->string);
	}
	struct string_list * const next = list->next;
	free (list);
	if (next)
	{
		del_string_list (next);
	}
}

struct string_list *
append_string_list (struct string_list * const list, struct string_list * const node)
{
	node->prev = list;
	node->next = list->next;
	list->next = node;
	if (node->next)
	{
		node->next->prev = node;
	}
	return node;
}

void
prefix_string_list (struct string_list * const list, struct string_list * const node)
{
	node->next = list;
	node->prev = list->prev;
	list->prev = node;
	if (node->prev)
	{
		node->prev->next = node;
	}
}

void
take_string_list (struct string_list * const node)
{
	struct string_list * const prev = node->prev;
	struct string_list * const next = node->next;
	node->next = NULL;
	node->prev = NULL;
	if (prev)
	{
		prev->next = next;
	}
	if (next)
	{
		next->prev = prev;
	}
}

struct string_list *
sl_splice (struct string_list * const dest, struct string_list * const src)
{
	if (! src)
	{
		return NULL;
	}
	struct string_list * const next = dest->next;
	src->prev = dest;
	dest->next = src;
	struct string_list * it = src;
	for (; it->next; it = it->next);
	it->next = next;
	if (next)
	{
		next->prev = it;
		for (; it->next; it = it->next);
	}
	return it;
}

struct string_list *
sl_copy_t (struct token const t)
{
	return new_string_list (strndup (t.bytes, t.size), t.size, t.size);
}

struct string_list *
sl_copy_str (char const * const string)
{
	size_t const len = strlen (string);
	return new_string_list (strdup (string), len, len);
}

struct string_list *
sl_copy_append_t (struct string_list * const list, struct token const t)
{
	return append_string_list (list, sl_copy_t (t));
}

struct string_list *
sl_copy_append (struct string_list * const list, char const * const string)
{
	return append_string_list (list, sl_copy_str (string));
}

struct string_list *
sl_copy_append_strref (struct string_list * const list, char const * const s, size_t const size)
{
	return append_string_list (list, new_string_list (strndup (s, size), size, size));
}

struct string_list *
sl_copy_append_uint (struct string_list * const list, unsigned const u)
{
	int const lsize = snprintf (NULL, 0, "%u", u) + 1;
	char * buf = (char *) malloc (lsize * sizeof (char));
	snprintf (buf, lsize, "%u", u);
	buf[lsize - 1] = '\0';
	return append_string_list (list, new_string_list (buf, lsize - 1, lsize));
}

char *
sl_uint (unsigned const u)
{
	int const lsize = snprintf (NULL, 0, "%u", u) + 1;
	char * buf = (char *) malloc (lsize * sizeof (char));
	snprintf (buf, lsize, "%u", u);
	buf[lsize - 1] = '\0';
	return buf;
}

char *
sl_cat (char const * const a, char const * const b)
{
	int const size = snprintf (NULL, 0, "%s%s", a, b) + 1;
	char * buf = (char *) malloc (size * sizeof (char));
	snprintf (buf, 0, "%s%s", a, b);
	buf[size - 1] = '\0';
	return buf;
}

size_t
sl_fold (char ** const dest_ptr, struct string_list * const sl)
{
	assert (dest_ptr);
	struct string_list * it = NULL;
	size_t size = 0;
	for (it = sl; it; it = it->next)
	{
		size += it->size;
	}
	* dest_ptr = (char *) malloc ((size + 1) * sizeof (char));
	(* dest_ptr)[size] = '\0';
	char * dest = * dest_ptr;
	for (it = sl; it; it = it->next)
	{
		if (it->size)
		{
			assert (it->string);
			memcpy (dest, it->string, it->size * sizeof (char));
			dest += it->size;
		}
	}
	return size;
}

char const *
sl_find (struct string_list * map, char const * key, size_t const * const len)
{
	struct string_list * it = map->next;
	for (; it; it = it->next->next)
	{
		assert (it->next);
		size_t const keylen = len ? * len : strlen (key);
		if (it->size != keylen)
		{
			continue;
		}
		if (! strncmp (key, it->string, keylen))
		{
			return it->next->string;
		}
	}
	return NULL;
}
