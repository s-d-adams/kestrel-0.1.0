/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include "eval.h"

struct string_list *
eval_addr (struct context * const ctx, struct syntax_tree * pn)
{
	if (ctx->insidePureFunction->i)
	{
		context_set_error (ctx, "Unexpected addr expression inside a pure function.", pn);
		return NULL;
	}
	if (st_count (pn->sub_tree) != 2)
	{
		context_set_error (ctx, "addr expression expects a single argument.", pn);
		return NULL;
	}
	struct syntax_tree * nName = st_at (pn->sub_tree, 1);
	if (nName->token.type != token_type_name)
	{
		context_set_error (ctx, "Argument to addr not a name.", nName);
		return NULL;
	}

	struct syntax_tree * const name_in_scope = context_findNameInScope (ctx, nName->token);
	if (! name_in_scope)
	{
		
		context_set_error (ctx, "Argument to addr undefined.", nName);
		return NULL;
	}

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	end = sl_copy_append (end, "&");
	end = sl_copy_append_t (end, nName->token);

	struct syntax_tree * const t = new_syntax_tree (make_token (token_type_open, NULL, 0, 0, 0, 0), NULL);
	t->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	st_add (t->sub_tree, new_syntax_tree (make_str_token ("ptr"), t));
	st_add (t->sub_tree->next, st_copy (name_in_scope->sub_tree));
	st_add (t->sub_tree->next->next, new_syntax_tree (make_token (token_type_close, & close_brace_char, 1, 0, 0, 0), NULL));

	ctx->lastExpressionType = t;

	return begin;
}
