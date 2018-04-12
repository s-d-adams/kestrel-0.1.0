/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include "lex.h"

struct syntax_tree
{
	struct syntax_tree * prev;
	struct syntax_tree * next;

	// Subexpressions, not NULL iff this token is an open brace.
	struct syntax_tree * sub_tree;

	// NULL if there is no parent.
	struct syntax_tree * parent_tree;

	// This token.
	struct token token;
};

struct syntax_tree *
new_syntax_tree (struct token const token, struct syntax_tree * const parent_tree);

void
del_syntax_tree (struct syntax_tree * const syntax_tree);

struct syntax_tree *
st_tail (struct syntax_tree * const head);

void
st_add (struct syntax_tree * list, struct syntax_tree * node);

void
st_take (struct syntax_tree * st);

// Doesn't count the root node, or last in the list, assumes it is a close brace.
size_t
st_count (struct syntax_tree * st);

// Doesn't count the root node.
struct syntax_tree *
st_at (struct syntax_tree * root, size_t index);

struct syntax_tree *
st_last_non_close (struct syntax_tree * const list);

struct syntax_tree *
st_copy (struct syntax_tree * st);

struct syntax_tree *
parse (struct token_list * const token_list_root);

void
ensure_capacity (struct syntax_tree * const syntax_tree, size_t const capacity);

struct syntax_tree *
get_last_syntax_tree_node (struct syntax_tree * const syntax_tree_root);

struct syntax_tree *
find_previous_symbol (struct syntax_tree * const symbol);

struct syntax_tree *
find_next_symbol (struct syntax_tree * const symbol);

int
syntax_tree_equal (struct syntax_tree * a, struct syntax_tree * b);
