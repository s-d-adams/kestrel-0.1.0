/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include "eval.h"
#include "string_list.h"
#include "parse.h"
#include "display.h"

struct sl_write_handler_data
{
	struct string_list * begin;
	struct string_list * end;
};

static
void sl_write_handler (void * data, char const * output, size_t const length)
{
	struct sl_write_handler_data * d = (struct sl_write_handler_data *) data;
	d->end = sl_copy_append_strref (d->end, output, length);
}

struct string_list *
eval_as_header (struct context * const ctx, struct syntax_tree * pn)
{
	(void) ctx;
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	if (token_type_open != pn->token.type)
	{
		return begin;
	}
	if (4 != st_count (pn->sub_tree))
	{
		return begin;
	}
	if (t_strcmp (st_at (pn->sub_tree, 0)->token, "def"))
	{
		return begin;
	}
	if (! t_strcmp (st_at (pn->sub_tree, 2)->token, "struct"))
	{
		struct sl_write_handler_data d;
		d.begin = d.end = end;
		display_syntax_tree_minimal (pn, sl_write_handler, & d);
		return begin;
	}
	else
	{
		struct sl_write_handler_data d;
		d.begin = d.end = end;
		end = sl_copy_append (end, "(dec ");
		end = sl_copy_append_t (end, st_at (pn->sub_tree, 1)->token);
		display_syntax_tree_minimal (st_at (pn->sub_tree, 2), sl_write_handler, & d);
		end = d.end;
		end = sl_copy_append (end, ")");
		return begin;
	}
}
