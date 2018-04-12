/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "display.h"

static
size_t
num_indivisible_subexpressions (struct syntax_tree * const st)
{
	if (st && st->sub_tree && st->sub_tree->next)
	{
		return st->sub_tree->next->token.type == token_type_name ? 2 : 1;
	}
	else
	{
		return 1;
	}
}

static
int
token_types_separated_by_space (enum token_type const first, enum token_type const second)
{
	return ((second == token_type_name) && ((first == token_type_name) || (first == token_type_close) || (first == token_type_string)))
	       || ((second == token_type_open) && ((first == token_type_name) || (first == token_type_close) || (first == token_type_string)))
	       || ((second == token_type_string) && ((first == token_type_name) || (first == token_type_close) || (first == token_type_string)));
}

void
display_syntax_tree_minimal (struct syntax_tree * const syntax_tree_root, void (* write_handler) (void * data, char const * output, size_t const length), void * write_handler_data)
{
	int last_is_name = 0;

	struct syntax_tree * it = syntax_tree_root->next;
	for (; it; it = it->next)
	{
		if (last_is_name && it->token.type == token_type_name)
		{
			write_handler (write_handler_data, " ", 1);
		}

		last_is_name = (it->token.type == token_type_name);

		write_handler (write_handler_data, it->token.bytes, it->token.size);

		if (it->sub_tree)
		{
			display_syntax_tree_minimal (it->sub_tree, write_handler, write_handler_data);
		}
	}
}

int
display_syntax_tree_single_line (struct syntax_tree * const syntax_tree_root, struct syntax_tree * const stop_after_node, int const continue_to_parent, void (* write_handler) (void * data, char const * output, size_t const length, struct syntax_tree * const node), void * write_handler_data)
{
	enum token_type last_token_type = token_type_none;

	// Allow for starting anywhere in the list.
	struct syntax_tree * it = syntax_tree_root->token.type == token_type_none ? syntax_tree_root->next : syntax_tree_root;
	struct syntax_tree * last = NULL;
	for (; it; it = it->next)
	{
		last = it;

		if (token_types_separated_by_space (last_token_type, it->token.type))
		{
			write_handler (write_handler_data, " ", 1, NULL);
		}

		last_token_type = it->token.type;

		write_handler (write_handler_data, it->token.bytes, it->token.size, it);

		if (it == stop_after_node)
		{
			return 1;
		}

		if (it->sub_tree && it->sub_tree->next)
		{
			if (display_syntax_tree_single_line (it->sub_tree, stop_after_node, 0, write_handler, write_handler_data))
			{
				return 1;
			}

			last_token_type = token_type_close;
		}
	}

	if (continue_to_parent && last && last->parent_tree && last->parent_tree->token.type != token_type_none)
	{
		struct syntax_tree * const parents_next = last->parent_tree->next;
		if (parents_next)
		{
			if (parents_next->token.type != token_type_close)
			{
				write_handler (write_handler_data, " ", 1, NULL);
			}
			display_syntax_tree_single_line (parents_next, stop_after_node, 1, write_handler, write_handler_data);
		}
	}

	return 0;
}

void
display_syntax_tree_debug (struct syntax_tree * const syntax_tree_root, size_t const depth, void (* write_handler) (void * data, char const * output, size_t const length), void * write_handler_data)
{
	struct syntax_tree * it = syntax_tree_root->token.type == token_type_none ? syntax_tree_root->next : syntax_tree_root;
	for (; it; it = it->next)
	{
		size_t i;
		for (i = 0; i < depth; ++ i)
		{
			write_handler (write_handler_data, " ", 1);
		}
		write_handler (write_handler_data, it->token.bytes, it->token.size);
		write_handler (write_handler_data, "\n", 1);
		if (it->sub_tree)
		{
			display_syntax_tree_debug (it->sub_tree, depth + 1, write_handler, write_handler_data);
		}
	}
}

static
struct collapsable_list *
new_collapsable_list (struct syntax_tree * const node, char const collapsed, char const dirty)
{
	struct collapsable_list * const ret = (struct collapsable_list *) malloc (sizeof (struct collapsable_list));
	ret->prev = ret->next = NULL;
	ret->node = node;
	ret->collapsed = collapsed;
	ret->dirty = dirty;
	ret->columns_set = 0;
	ret->columns = 0;
	return ret;
}

static
void
cl_add (struct collapsable_list * list, struct collapsable_list * node)
{
	node->next = list->next;
	list->next = node;
	node->prev = list;
	if (node->next)
	{
		node->next->prev = node;
	}
}

static
struct collapsable_list *
cl_tail (struct collapsable_list * list)
{
	for (; list->next; list = list->next);
	return list;
}

static
void
del_collapsable_list (struct collapsable_list * collapsable_list_root)
{
	struct collapsable_list * it, * next;
	for (it = collapsable_list_root; it; it = next)
	{
		next = it->next;
		free (it);
	}
}

static
struct line_list *
new_line_list (struct syntax_tree * const begin, struct syntax_tree * const end)
{
	struct line_list * ret = (struct line_list *) malloc (sizeof (struct line_list));
	ret->prev = ret->next = NULL;
	ret->begin = begin;
	ret->end = end;
	ret->collapsables = NULL;
	return ret;
}

void
del_line_list (struct line_list * line_list_root)
{
	struct line_list * it = line_list_root, * next;
	for (; it; it = next)
	{
		next = it->next;
		if (it->collapsables)
		{
			del_collapsable_list (it->collapsables);
		}
		free (it);
	}
}

void
ll_add (struct line_list * const list, struct line_list * const node)
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
ll_take (struct line_list * const node)
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

struct line_list *
ll_tail (struct line_list * list)
{
	for (; list->next; list = list->next);
	return list;
}

static
int
num_chars_in_expression (struct syntax_tree * const begin, struct syntax_tree * const end, size_t * const character_count, int const continue_to_parent)
{
	enum token_type last_token_type = token_type_none;

	struct syntax_tree * last = NULL;

	struct syntax_tree * it = begin->token.type == token_type_none ? begin->next : begin;
	for (; it; it = it->next)
	{
		last = it;

		// Account for spaces.
		if (token_types_separated_by_space (last_token_type, it->token.type))
		{
			* character_count += 1;
		}

		* character_count += it->token.size;

		last_token_type = it->token.type;

		if (it == end)
		{
			return 1;
		}

		if (it->sub_tree)
		{
			int const rc = num_chars_in_expression (it->sub_tree, end, character_count, 0);

			if (rc)
			{
				return 1;
			}

			last_token_type = token_type_close;
		}
	}

	if (continue_to_parent && last && last->parent_tree)
	{
		struct syntax_tree * const parents_next = last->parent_tree->next;
		if (parents_next)
		{
			if (parents_next->token.type != token_type_close)
			{
				* character_count += 1;
			}
			num_chars_in_expression (parents_next, end, character_count, 1);
		}
	}

	return 0;
}

static
size_t
get_expression_count (struct syntax_tree * const root)
{
	size_t ret = 0;

	struct syntax_tree * it = root->next;
	for (; it; it = it->next)
	{
		if (it->token.type != token_type_close)
		{
			++ ret;
		}
	}

	return ret;
}

static
struct syntax_tree *
find_last_open_with_multiple_subexpressions (struct syntax_tree * root)
{
	if (root->token.type != token_type_open)
	{
		return NULL;
	}
	struct syntax_tree * it = st_tail (root->sub_tree);
	for (; it->token.type != token_type_none; it = it->prev)
	{
		if (it->token.type == token_type_open)
		{
			struct syntax_tree * const ret = find_last_open_with_multiple_subexpressions (it);
			if (ret)
			{
				return ret;
			}
			if (it->sub_tree->next && get_expression_count (it->sub_tree) > (it->sub_tree->next->token.type == token_type_name ? 2 : 1))
			{
				return it;
			}
		}
	}
	if (root->sub_tree->next && get_expression_count (root->sub_tree) > (root->sub_tree->next->token.type == token_type_name ? 2 : 1))
	{
		return root;
	}
	else
	{
		return NULL;
	}
}

struct line_list *
find_line_for_symbol (struct line_list * const root, struct syntax_tree * const symbol)
{
	struct line_list * it = root->begin == NULL ? root->next : root;
	for (; it; it = it->next)
	{
		struct syntax_tree * exp = it->begin;
		do
		{
			if (exp == symbol)
			{
				return it;
			}
			if (exp == it->end)
			{
				break;
			}
			exp = find_next_symbol (exp);
		}
		while (exp);
	}

	return NULL;
}

static
struct collapsable_list *
find_collapsable_nodes (struct syntax_tree * const syntax_tree_root)
{
	struct collapsable_list * const root = new_collapsable_list (NULL, 0, 0);

	struct syntax_tree * exp = find_last_open_with_multiple_subexpressions (syntax_tree_root);
	if (! exp)
	{
		return root;
	}

	do
	{
		struct collapsable_list * const next = new_collapsable_list (exp, 0, 0);
		cl_add (cl_tail (root), next);

		if (exp == syntax_tree_root)
		{
			break;
		}

		do
		{
			exp = find_previous_symbol (exp);
		}
		while ((exp && exp->token.type != token_type_none) && ! (exp->sub_tree && (get_expression_count (exp->sub_tree) > num_indivisible_subexpressions (exp))));
	}
	while (exp && exp->token.type != token_type_none);

	return root;
}

static
size_t
compute_indentation (struct line_list * const line_list_root, struct line_list const * const line)
{
	size_t indentation = 0;
//	if (! line || ! line->begin)
//	{
//		return 0;
//	}
	struct syntax_tree * it = line->begin->parent_tree;
	struct line_list const * previous_up = NULL;
	while (it)
	{
		struct line_list const * const up = find_line_for_symbol (line_list_root, it);
		if (! up)
		{
			break;
		}
		if (up != previous_up)
		{
			struct syntax_tree * end = it;
			size_t const ninds = num_indivisible_subexpressions (it);
			if (ninds > 1)
			{
				size_t num = ninds - 1;
				end = it->sub_tree;
				for (; num; -- num)
				{
					end = it->sub_tree->next;
					assert (end && end->token.type != token_type_none);
				}
				end = get_last_syntax_tree_node (end);
			}
			num_chars_in_expression (up->begin,  end, & indentation, 1);
			if (ninds > 1)
			{
				indentation += 1;
			}
		}
		it = up->begin->parent_tree;
		previous_up = up;
	}
	return indentation;
}

void
display_line (struct line_list * const line_list_root, struct line_list * const line, void (* write_handler) (void * data, char const * output, size_t const length, struct syntax_tree * const node), void * write_handler_data)
{
	size_t const indentation = compute_indentation (line_list_root, line);

	size_t i;
	for (i = 0; i < indentation; ++ i)
	{
		write_handler (write_handler_data, " ", 1, NULL);
	}

	display_syntax_tree_single_line (line->begin, line->end, 1, write_handler, write_handler_data);
}

void
display_line_list (struct line_list * const line_list_root, size_t const * const first, size_t const * const count, struct syntax_tree * const stop_after_node, void (* write_handler) (void * data, char const * output, size_t const length, struct syntax_tree * const node), void * write_handler_data)
{
	size_t start_counter = 0;
	size_t finish_counter = 0;
	int is_first_line = 1;
	struct line_list * tmp = line_list_root->begin == NULL ? line_list_root->next : line_list_root;
	for (; tmp; tmp = tmp->next)
	{
		if (first && start_counter < * first)
		{
			++ start_counter;
			continue;
		}
		else if (count)
		{
			++ finish_counter;
		}
		if (! is_first_line)
		{
			write_handler (write_handler_data, "\n", 1, NULL);
		}
		else
		{
			is_first_line = 0;
		}
		display_line (line_list_root, tmp, write_handler, write_handler_data);
		if (count && finish_counter == * count)
		{
			break;
		}
		if (stop_after_node && tmp->end == stop_after_node)
		{
			break;
		}
	}
}

struct line_list_counter_data
{
	size_t current_line_width;
	size_t largest_line_width;
};

static
void
line_list_counter (void * data, char const * const output, size_t const length, struct syntax_tree * const node)
{
	(void) node;
	struct line_list_counter_data * const d = (struct line_list_counter_data *) data;

	d->current_line_width += length;

	if (d->current_line_width > d->largest_line_width)
	{
		d->largest_line_width = d->current_line_width;
	}

	if (length == 1 && * output == '\n')
	{
		d->current_line_width = 0;
	}
}

size_t
compute_line_list_column_width (struct line_list * const line, struct syntax_tree * const stop_after_node)
{
	struct line_list_counter_data d;
	d.current_line_width = 0;
	d.largest_line_width = 0;
	display_line_list (line, NULL, NULL, stop_after_node, line_list_counter, & d);
	return d.largest_line_width;
}

struct line_list *
collapse_line_list_at_expression (struct line_list * const root, struct syntax_tree * const exp)
{
	assert (exp);
	assert (exp->sub_tree);

	struct line_list * const line = find_line_for_symbol (root, exp);
	assert (line);

	struct syntax_tree * it = exp->sub_tree->next;
	int skip = num_indivisible_subexpressions (exp);
	for (; it; it = it->next)
	{
		if (skip)
		{
			-- skip;
			continue;
		}

		if (it->token.type == token_type_close)
		{
			continue;
		}

		struct line_list * const original = find_line_for_symbol (line, it);
		assert (original);

		struct line_list * const new_line = new_line_list (it, original->end);

		original->end = find_previous_symbol (it);
		assert (original->end && original->end->token.type != token_type_none);

		ll_add (original, new_line);
	}

	return root;
}

static
struct line_list *
uncollapse_line_list_at_expression (struct line_list * const root, struct syntax_tree * const exp)
{
	assert (exp);
	assert (exp->sub_tree);

	struct line_list * const line = find_line_for_symbol (root, exp);
	assert (line);

	struct syntax_tree * it = exp->sub_tree->next;
	int skip = num_indivisible_subexpressions (exp);
	for (; it; it = it->next)
	{
		if (skip)
		{
			-- skip;
			continue;
		}

		if (it->token.type == token_type_close)
		{
			continue;
		}

		struct line_list * const original = find_line_for_symbol (line, it);
		assert (original);

		struct line_list * const prev = original->prev;
		assert (prev->begin);

		prev->end = original->end;

		ll_take (original);
		del_line_list (original);
	}

	return root;
}

struct line_list *
collapse_at_flagged_nodes (struct line_list * root)
{
	struct collapsable_list * const collapsables = root->collapsables;
	assert (collapsables);

	struct collapsable_list * it = collapsables->next;
	for (; it; it = it->next)
	{
		if (it->dirty)
		{
			root = (it->collapsed ? collapse_line_list_at_expression : uncollapse_line_list_at_expression) (root, it->node);
			it->dirty = 0;
		}
	}

	return root;
}

static
void
increment_collapsables (struct line_list * const root)
{
	struct collapsable_list * const collapsables = root->collapsables;
	assert (collapsables);

	struct collapsable_list * it = collapsables->next;
	for (; it; it = it->next)
	{
		it->collapsed = ! it->collapsed;
		it->dirty = ! it->dirty;
		if (it->collapsed)
		{
			break;
		}
	}
}

static
void
decrement_collapsables (struct line_list * const root)
{
	struct collapsable_list * const collapsables = root->collapsables;
	assert (collapsables);

	struct collapsable_list * it = collapsables->next;
	for (; it; it = it->next)
	{
		it->collapsed = ! it->collapsed;
		it->dirty = ! it->dirty;
		if (! it->collapsed)
		{
			break;
		}
	}
}

struct line_list *
split_top_level_expressions (struct syntax_tree * const syntax_tree_root)
{
	struct line_list * const line_list_root = new_line_list (NULL, NULL);

	struct syntax_tree * it = syntax_tree_root->next;
	for (; it; it = it->next)
	{
		struct line_list * const line = new_line_list (it, get_last_syntax_tree_node (it));
		line->collapsables = find_collapsable_nodes (it);
		ll_add (ll_tail (line_list_root), line);
	}

	return line_list_root;
}

static
size_t
collapsable_count (struct collapsable_list * const cl)
{
	size_t count = 0;
	struct collapsable_list * it = cl->next;
	for (; it; it = it->next)
	{
		(void) it;
		++ count;
	}
	return count;
}

struct line_list *
fit_line_list_to_columns (struct line_list * const line_list_root, size_t const columns)
{
	struct line_list * it = line_list_root->next;
	for (; it; it = it->next)
	{
		if (! it->collapsables)
		{
			continue;
		}
		struct syntax_tree * const expression_end = get_last_syntax_tree_node (it->begin);
		size_t const count = collapsable_count (it->collapsables);
		// The "Last Resort" strategy if we ever get problems with the other strategies.
		// O(n), but without expensive operations inside the loop.
		if (count > 16384)
		{
			struct collapsable_list * c = it->collapsables->next;
			for (; c; c = c->next)
			{
				if (! c->collapsed)
				{
					c->collapsed = 1;
					c->dirty = ! c->dirty;
				}
			}
			collapse_at_flagged_nodes (it);
			it->collapsables->columns = compute_line_list_column_width (it, expression_end);
			it->collapsables->columns_set = 1;
		}
		// The "Multiplying" strategy.  O(n).
		// The limit of 128 here is a guess calculated from the number of possibilities handled well by the counting strategy.
		// 128 is the square root of that number, hence the O(n^2) strategy would likely struggle beyond this number.
		else if (count > 128)
		{
			struct collapsable_list * c = it->collapsables->next;
			for (; c; c = c->next)
			{
				if (c->collapsed)
				{
					c->collapsed = 0;
					c->dirty = ! c->dirty;
				}
			}
			collapse_at_flagged_nodes (it);
			size_t computed_width = compute_line_list_column_width (it, expression_end);
			c = cl_tail (it->collapsables);
			for (; c->node; c = c->prev)
			{
				c->collapsed = 1;
				c->dirty = 1;
				collapse_at_flagged_nodes (it);
				computed_width = compute_line_list_column_width (it, expression_end);
				if (computed_width <= columns)
				{
					break;
				}
			}
			it->collapsables->columns = computed_width;
			it->collapsables->columns_set = 1;
		}
		// The "Bubbling" strategy.  O(n^2).
		else if (count > 14)
		{
			struct collapsable_list * c = it->collapsables->next;
			for (; c; c = c->next)
			{
				if (c->collapsed)
				{
					c->collapsed = 0;
					c->dirty = ! c->dirty;
				}
			}
			collapse_at_flagged_nodes (it);
			size_t computed_width = compute_line_list_column_width (it, expression_end);
			int i, j;
			for (i = count; i > 0; -- i)
			{
				j = 0;
				c = it->collapsables->next;
				for (; c; c = c->next)
				{
					c->collapsed = 1;
					c->dirty = 1;
					collapse_at_flagged_nodes (it);
					computed_width = compute_line_list_column_width (it, expression_end);
					++ j;
					if ((computed_width <= columns) || j == i)
					{
						break;
					}
					c->collapsed = 0;
					c->dirty = 1;
				}
				if (computed_width <= columns)
				{
					break;
				}
			}
			it->collapsables->columns = computed_width;
			it->collapsables->columns_set = 1;
		}
		// The "Counting" strategy.  O(2^n).
		// Discovered lag with the counting strategy around 15 collapsable expressions.
		// Given the complexity that means this strategy can process at least 16384 possibilities in realtime.
		else
		{
			size_t computed_width = compute_line_list_column_width (it, expression_end);
			if (it->collapsables->columns_set && it->collapsables->columns < columns)
			{
				while ((computed_width < columns) && (! is_fully_uncollapsed (it->collapsables)))
				{
					decrement_collapsables (it);
					collapse_at_flagged_nodes (it);
					computed_width = compute_line_list_column_width (it, expression_end);
				}
			}
			while ((computed_width > columns) && (! is_fully_collapsed (it->collapsables)))
			{
				increment_collapsables (it);
				collapse_at_flagged_nodes (it);
				computed_width = compute_line_list_column_width (it, expression_end);
			}
			it->collapsables->columns = computed_width;
			it->collapsables->columns_set = 1;
		}
	}
	return line_list_root;
}

int
is_fully_collapsed (struct collapsable_list const * const collapsables)
{
	struct collapsable_list * it = collapsables->next;
	for (; it; it = it->next)
	{
		if (! it->collapsed)
		{
			return 0;
		}
	}

	return 1;
}

int
is_fully_uncollapsed (struct collapsable_list const * const collapsables)
{
	struct collapsable_list * it = collapsables->next;
	for (; it; it = it->next)
	{
		if (it->collapsed)
		{
			return 0;
		}
	}

	return 1;
}

struct syntax_tree_cursor_model
line_to_syntax (struct line_list * const line_list_root, struct syntax_tree * const syntax_tree_root, struct line_list_cursor_model const line_model, size_t const line_offset)
{
	struct syntax_tree_cursor_model ret;

	size_t line = 0;
	struct line_list * last = NULL;
	struct line_list * it = line_list_root->next;
	for (; it; it = it->next)
	{
		last = it;
		if (line < line_model.y + line_offset)
		{
			++ line;
			continue;
		}
		if (line > line_model.y + line_offset)
		{
			break;
		}
		size_t remaining_x = line_model.x;
		size_t symbol_column = compute_indentation (line_list_root, it);
		if (remaining_x <= symbol_column)
		{
			ret.symbol = it->begin;
			ret.offset = 0;
			return ret;
		}
		remaining_x -= symbol_column;
		enum token_type previous_token_type = token_type_none;
		struct syntax_tree * symbol = it->begin;
		for (;;)
		{
			if (! symbol)
			{
				break;
			}
			if (token_types_separated_by_space (previous_token_type, symbol->token.type))
			{
				remaining_x -= 1;
			}
			if (remaining_x <= symbol->token.size)
			{
				ret.symbol = symbol;
				ret.offset = remaining_x;
				return ret;
			}
			if (symbol == it->end)
			{
				break;
			}
			remaining_x -= symbol->token.size;
			previous_token_type = symbol->token.type;
			symbol = find_next_symbol (symbol);
		}
		ret.symbol = it->end;
		ret.offset = it->end->token.size;
		return ret;
	}
	ret.symbol = last ? last->end : syntax_tree_root;
	ret.offset = last ? last->end->token.size : 0;
	return ret;
}

struct line_list_cursor_model
syntax_to_line (struct line_list * const line_list_root, struct syntax_tree_cursor_model const syntax_model, size_t const line_offset)
{
	struct line_list_cursor_model ret;

	struct line_list * const line = find_line_for_symbol (line_list_root, syntax_model.symbol);
	if (! line)
	{
		ret.y = 0;
		ret.x = 0;
		return ret;
	}

	size_t y = 0;
	struct line_list * it = line_list_root->next;
	for (; it; it = it->next)
	{
		if (it == line)
		{
			break;
		}
		y += 1;
	}

	size_t x = compute_indentation (line_list_root, line);
	num_chars_in_expression (line->begin, syntax_model.symbol, & x, 1);
	x -= syntax_model.symbol->token.size;
	x += syntax_model.offset;

	ret.y = y - line_offset;
	ret.x = x;
	return ret;
}

size_t
line_count (struct line_list * const root)
{
	size_t count = 0;
	struct line_list * it = root->next;
	for (; it; it = it->next)
	{
		++ count;
	}
	return count;
}
