/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include "eval.h"

struct string_list *
eval_cond (struct context * const ctx, struct syntax_tree * pn)
{
	size_t const condSize = st_count (pn->sub_tree);
	if (3 > condSize)
	{
		
		context_set_error (ctx, "Incorrect number of expressions in 'cond'", pn);
		return NULL;
	}
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	struct syntax_tree * type = NULL;
	line (ctx, pn->token.line);
	for (unsigned i = 1; i < condSize - 1; ++i)
	{
		if (token_type_open != (st_at (pn->sub_tree, i))->token.type || 2 != st_count (st_at (pn->sub_tree, i)->sub_tree))
		{
			
			context_set_error (ctx, "Incorrect number of expressions in 'cond' pair", st_at (pn->sub_tree, i));
			return NULL;
		}
		end = sl_copy_append (end, "(");
		end = sl_splice (end, eval (ctx, st_at (st_at (pn->sub_tree, i)->sub_tree, 0)));
		if (ctx->has_error)
		{
			return NULL;
		}
		end = sl_copy_append (end, "?");
		end = sl_splice (end, eval (ctx, st_at (st_at (pn->sub_tree, i)->sub_tree, 1)));
		if (ctx->has_error)
		{
			return NULL;
		}
		if (type == NULL)
		{
			type = ctx->lastExpressionType;
		}
		else if (! types_equal_modulo_mutability (type, ctx->lastExpressionType))
		{
			
			context_set_error (ctx, "Result type doesn't match previous result types", st_at (st_at (pn->sub_tree, i)->sub_tree, 1));
			return NULL;
		}
		end = sl_copy_append (end, ":");
	}
	end = sl_splice (end, eval (ctx, st_last_non_close (pn->sub_tree)));
	if (ctx->has_error)
	{
		return NULL;
	}
	if (! types_equal_modulo_mutability (type, ctx->lastExpressionType))
	{
		
		context_set_error (ctx, "Result type doesn't match previous result types", st_last_non_close (pn->sub_tree));
		return NULL;
	}
	size_t i;
	for (i = 0; i < condSize - 2; ++ i)
	{
		end = sl_copy_append (end, ")");
	}
	return begin;
}
