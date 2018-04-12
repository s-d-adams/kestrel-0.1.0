/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "eval.h"

struct string_list *
eval_deref (struct context * const ctx, struct syntax_tree * pn)
{
	if (ctx->insidePureFunction->i)
	{
		context_set_error (ctx, "Unexpected deref expression inside a pure function.", pn);
		return NULL;
	}
	if (st_count (pn->sub_tree) != 2)
	{
		context_set_error (ctx, "deref expression expects a single argument.", pn);
		return NULL;
	}
	struct syntax_tree * nName = st_at (pn->sub_tree, 1);
	if (nName->token.type != token_type_name)
	{
		context_set_error (ctx, "Argument to deref not a name.", nName);
		return NULL;
	}

	struct syntax_tree * const name_in_scope = context_findNameInScope (ctx, nName->token);
	if (! name_in_scope)
	{
		context_set_error (ctx, "Argument to deref undefined.", nName);
		return NULL;
	}

	struct syntax_tree * ptr_type = name_in_scope->sub_tree;
	if (is_type_mutable (ptr_type))
	{
		ptr_type = st_at (ptr_type->sub_tree, 1);
	}

	struct syntax_tree * underlying_type = st_at (ptr_type->sub_tree, 1);

	int const reference_counted = is_type_reference_counted (ctx, underlying_type);

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	if (reference_counted)
	{
		end = sl_copy_append (end, "(*({");
		struct token d;
		asprintf (& d.bytes, "*%.*s", (int) nName->token.size, nName->token.bytes);
		d.size = d.capacity = nName->token.size + 1;
		end = sl_splice (end, retainObject (d));
		free (d.bytes);
		end = sl_copy_append_t (end, nName->token);
		end = sl_copy_append (end, ";}))");
	}
	else
	{
		end = sl_copy_append (end, "*");
		end = sl_copy_append_t (end, nName->token);
	}

	ctx->lastExpressionType = underlying_type;

	return begin;
}
