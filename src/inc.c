/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include "eval.h"

struct string_list *
eval_inc (struct context * const ctx, struct syntax_tree *pn)
{
	if (2 != st_count (pn->sub_tree)
	    || token_type_string != (st_at (pn->sub_tree, 1))->token.type)
	{
		context_set_error (ctx, "'inc' expects 1 string", pn);
		return NULL;
	}

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	end = sl_copy_append (end, "#include ");
	end = sl_copy_append_t (end, st_at (pn->sub_tree, 1)->token);
	end = sl_copy_append (end, "\n");
	line (ctx, pn->token.line);
	ctx->lastExpressionType = NULL;
	return begin;
}
