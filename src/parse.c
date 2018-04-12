/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

struct syntax_tree *
new_syntax_tree (struct token const token, struct syntax_tree * const parent_tree)
{
	struct syntax_tree * ret = (struct syntax_tree *) malloc (sizeof (struct syntax_tree));
	ret->prev = NULL;
	ret->next = NULL;
	ret->token = token;
	ret->sub_tree = NULL;
	ret->parent_tree = parent_tree;
	return ret;
}

void
del_syntax_tree (struct syntax_tree * const syntax_tree)
{
	struct syntax_tree * it, * next;
	for (it = syntax_tree; it; it = next)
	{
		next = it->next;
		if (it->sub_tree)
		{
			del_syntax_tree (it->sub_tree);
		}
		if (it->token.capacity)
		{
			free (it->token.bytes);
		}
		free (it);
	}
}

size_t
st_count (struct syntax_tree * st)
{
	assert (st);
	size_t size = 0;
	for (st = st->next; st && st->next; st = st->next, ++ size);
	return size;
}

struct syntax_tree *
st_tail (struct syntax_tree * list)
{
	for (; list->next; list = list->next);
	return list;
}

void
st_add (struct syntax_tree * list, struct syntax_tree * node)
{
	node->next = list->next;
	list->next = node;
	node->prev = list;
	if (node->next)
	{
		node->next->prev = node;
	}
}

void
st_take (struct syntax_tree * node)
{
	if (node->prev)
	{
		node->prev->next = node->next;
	}
	if (node->next)
	{
		node->next->prev = node->prev;
	}
	node->prev = node->next = NULL;
}

struct syntax_tree *
st_at (struct syntax_tree * root, size_t index)
{
	assert (root);
	++ index; // Skip the root node.
	for (; index; -- index, root = root->next)
	{
		assert (root->next);
	}
	return root;
}

struct syntax_tree *
st_last_non_close (struct syntax_tree * list)
{
	for (; list->next && list->next->token.type != token_type_close; list = list->next);
	return list;
}

struct syntax_tree *
st_copy (struct syntax_tree * const st)
{
	struct syntax_tree * const root = new_syntax_tree (make_token (st->token.type, strndup (st->token.bytes, st->token.size), st->token.size, st->token.index, st->token.index, st->token.column), st->parent_tree);
	if (st->sub_tree)
	{
		root->sub_tree = st_copy (st->sub_tree);
	}
	if (st->next)
	{
		root->next = st_copy (st->next);
	}
	return root;
}

// Takes a double-pointer to allow the algorithm to pass back the next token to parse when returning from a recursive call.
static
struct syntax_tree *
parse_recursive (struct syntax_tree * root, struct token_list ** const token_list_ptr)
{
	struct syntax_tree * tail = root;
	struct token_list * it = * token_list_ptr;
	while (it)
	{
		assert (it->token.type != token_type_none);

		if (it->token.type == token_type_break)
		{
			break;
		}

		struct syntax_tree * next = new_syntax_tree (it->token, root->parent_tree);
		st_add (tail, next);
		tail = next;

		// Update the progress through the token list.
		* token_list_ptr = it->next;

		if (next->token.type == token_type_close)
		{
			return root;
		}
		else if (next->token.type == token_type_open)
		{
			next->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, next->token.index, next->token.line, next->token.column), next);
			parse_recursive (next->sub_tree, token_list_ptr);
			it = * token_list_ptr;
		}
		else
		{
			 it = it->next;
		}
	}

	// If this is reached then the expression had more open braces than close braces, or hit a break symbol.
	return tail;
}

struct syntax_tree *
parse (struct token_list * const token_list_root)
{
	if (! token_list_root)
	{
		return NULL;
	}

	struct syntax_tree * root = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	struct syntax_tree * tail = root;

	struct token_list * token_list = token_list_root->next;

	do
	{
		tail = parse_recursive (tail, & token_list);
		if (token_list && token_list->token.type == token_type_break)
		{
			st_add (tail, new_syntax_tree (token_list->token, NULL));
			token_list = token_list->next;
			tail = tail->next;
		}
	}
	while (token_list);

	return root;
}

void
ensure_capacity (struct syntax_tree * const syntax_tree, size_t const requested_capacity)
{
	if (syntax_tree->token.capacity < requested_capacity)
	{
		size_t cap = 1;
		while (cap < requested_capacity)
		{
			cap *= 2;
		}
		char * const bytes = (char *) malloc (cap * sizeof (char));
		memcpy (bytes, syntax_tree->token.bytes, syntax_tree->token.size);
		if (syntax_tree->token.capacity)
		{
			free (syntax_tree->token.bytes);
		}
		syntax_tree->token.bytes = bytes;
		syntax_tree->token.capacity = cap;
	}
}

struct syntax_tree *
get_last_syntax_tree_node (struct syntax_tree * const syntax_tree_root)
{
	if (syntax_tree_root->sub_tree && (syntax_tree_root->sub_tree->next))
	{
		return get_last_syntax_tree_node (st_tail (syntax_tree_root->sub_tree));
	}
	else
	{
		return syntax_tree_root;
	}
}

struct syntax_tree *
find_previous_symbol (struct syntax_tree * const symbol)
{
	if (symbol->token.type == token_type_none)
	{
		assert (! symbol->parent_tree);
		return NULL;
	}

	struct syntax_tree * const list_prev = symbol->prev;
	assert (list_prev);

	if (list_prev->token.type == token_type_open)
	{
		return get_last_syntax_tree_node (list_prev);
	}

	if (list_prev->token.type == token_type_none)
	{
		return symbol->parent_tree ? symbol->parent_tree : list_prev;
	}

	return list_prev;
}

struct syntax_tree *
find_next_symbol (struct syntax_tree * const symbol)
{
	if (symbol->token.type == token_type_open)
	{
		assert (symbol->sub_tree);
		return symbol->sub_tree->next;
	}

	struct syntax_tree * const list_next = symbol->next;

	if (! list_next)
	{
		if (symbol->parent_tree)
		{
			return symbol->parent_tree->next;
		}
		return NULL;
	}

	return list_next;
}

int
syntax_tree_equal (struct syntax_tree * a, struct syntax_tree * b)
{
	assert (! ((a == NULL) && (b == NULL)));
	if ((a == NULL) ^ (b == NULL))
	{
		return 0;
	}
	if (! token_equal (a->token, b->token))
	{
		return 0;
	}
	if (a->sub_tree)
	{
		if (! b->sub_tree)
		{
			return 0;
		}
		if (st_count (a->sub_tree) != st_count (b->sub_tree))
		{
			return 0;
		}
	}
	else if (b->sub_tree)
	{
		return 0;
	}
	size_t i;
	if (a->sub_tree == NULL)
	{
		return b->sub_tree == NULL;
	}
	size_t const size = st_count (a->sub_tree);
	for (i = 0; i < size; ++ i)
	{
		if (! syntax_tree_equal (st_at (a->sub_tree, i), st_at (b->sub_tree, i)))
		{
			return 0;
		}
	}
	return 1;
}
