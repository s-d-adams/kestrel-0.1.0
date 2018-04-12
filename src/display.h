/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include "parse.h"

void
display_syntax_tree_minimal (struct syntax_tree * const syntax_tree_root, void (* write_handler) (void * data, char const * output, size_t const length), void * write_handler_data);

int
display_syntax_tree_single_line (struct syntax_tree * const syntax_tree_root, struct syntax_tree * const stop_after_node, int const continue_to_parent, void (* write_handler) (void * data, char const * output, size_t const length, struct syntax_tree * const node), void * write_handler_data);

void
display_syntax_tree_debug (struct syntax_tree * const syntax_tree_root, size_t const depth, void (* write_handler) (void * data, char const * output, size_t const length), void * write_handler_data);

struct collapsable_list
{
	struct collapsable_list * prev;
	struct collapsable_list * next;

	struct syntax_tree * node;
	char collapsed;
	char dirty;
	char columns_set;
	size_t columns;
};

struct line_list
{
	struct line_list * prev;
	struct line_list * next;

	struct syntax_tree * begin;
	struct syntax_tree * end; // Not one-past-end.

	// A list of sub-expressions that can be collapsed.
	struct collapsable_list * collapsables;
};

void
del_line_list (struct line_list * line_list_root);

void
ll_add (struct line_list * list, struct line_list * node);

void
ll_take (struct line_list * node);

struct line_list *
ll_tail (struct line_list * list);

size_t
compute_line_list_column_width (struct line_list * const line_list_root, struct syntax_tree * const stop_after_node);

struct line_list *
fit_line_list_to_columns (struct line_list * const line_list_root, size_t const columns);

void
display_line (struct line_list * const line_list_root, struct line_list * const line, void (* write_handler) (void * data, char const * output, size_t const length, struct syntax_tree * const node), void * write_handler_data);

void
display_line_list (struct line_list * const line_list_root, size_t const * const first, size_t const * const count, struct syntax_tree * const stop_after_node, void (* write_handler) (void * data, char const * output, size_t const length, struct syntax_tree * const node), void * write_handler_data);

struct line_list *
split_top_level_expressions (struct syntax_tree * const syntax_tree_root);

struct line_list *
collapse_at_flagged_nodes (struct line_list * root);

int
is_fully_collapsed (struct collapsable_list const * collapsables);

int
is_fully_uncollapsed (struct collapsable_list const * collapsables);

struct syntax_tree_cursor_model
{
	struct syntax_tree * symbol;
	size_t offset;
};

struct line_list_cursor_model
{
	ssize_t y, x;
};

struct syntax_tree_cursor_model
line_to_syntax (struct line_list * const line_list_root, struct syntax_tree * const syntax_tree_root, struct line_list_cursor_model const line_model, size_t const line_offset);

struct line_list_cursor_model
syntax_to_line (struct line_list * const line_list_root, struct syntax_tree_cursor_model const syntax_model, size_t const line_offset);

struct line_list *
find_line_for_symbol (struct line_list * const root, struct syntax_tree * const symbol);

size_t
line_count (struct line_list * const root);
