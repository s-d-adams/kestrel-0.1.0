/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <string.h>

#include "eval.h"

struct string_list *
eval_call (struct context * const ctx, struct syntax_tree * pn)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	struct syntax_tree * what = st_at (pn->sub_tree, 0);
	struct syntax_tree * ctxName = context_findLocal (ctx, what->token);

	line (ctx, pn->token.line);
	if ((token_type_name != what->token.type) || ! t_strcmp (what->token, "rec") || (ctxName && is_type_function (ctxName->sub_tree)))
	{
		/* Then it is required to be a function object. */
		struct token tmp = context_make_temporary (ctx);
		end = sl_copy_append (end, "({void**");
		end = sl_copy_append_t (end, tmp);
		end = sl_copy_append (end, "=(void**)");
		end = sl_splice (end, eval (ctx, what));
		if (ctx->has_error)
		{
			return NULL;
		}
		struct syntax_tree * nType = ctx->lastExpressionType;
		int const impure = ! t_strcmp (nType->sub_tree->next->token, "imp");
		end = sl_copy_append (end, ";");
		end = sl_splice (end, retainObject (tmp));
		end = sl_copy_append (end, "((");
		end = sl_splice (end, functionToCExpr (ctx, nType, NULL, "void"));
		end = sl_copy_append (end, ")*");
		end = sl_copy_append_t (end, tmp);
		end = sl_copy_append (end, ")(");
		end = sl_copy_append_t (end, tmp);
		if (ctx->insidePureFunction->i && impure)
		{
			context_set_error (ctx, "Impure function called inside pure function.", what);
			return NULL;
		}
		if (impure)
		{
			ctx->impureFunctionCalled->i = 1;
		}
		for (unsigned i = 1; i < st_count (pn->sub_tree); ++i)
		{
			end = sl_copy_append (end, ",");
			end = sl_splice (end, eval (ctx, st_at (pn->sub_tree, i)));
			if (ctx->has_error)
			{
				return NULL;
			}
		}
		end = sl_copy_append (end, ");})");
		ctx->lastExpressionType = st_last_non_close (nType->sub_tree);
		return begin;
	}
	else
	{
		ctxName = context_findGlobal (ctx, what->token);
	}

	if (ctx->insidePureFunction->i)
	{
		if (!ctxName)
		{
			
			context_set_error (ctx, "Name not in context.", what);
			return NULL;
		}
		if (token_type_open != ctxName->sub_tree->token.type)
		{
			
			context_set_error (ctx, "Name not bound to a function.", what);
			return NULL;
		}
	}

	line (ctx, what->token.line);
	end = sl_copy_append_t (end, what->token);
	end = sl_copy_append (end, "(");

	int const impure = (!ctxName || ! t_strcmp (st_at (ctxName->sub_tree->sub_tree, 0)->token, "imp"));
	if (ctxName)
	{
		if ((st_count (pn->sub_tree) - 1) != (st_count (ctxName->sub_tree->sub_tree) - (impure ? 2 : 1)))
		{
			
			context_set_error (ctx, "Incorrect number of arguments passed to function.", pn);
			return NULL;
		}
	}
	if (ctx->insidePureFunction->i && impure)
	{
		
		context_set_error (ctx, "Impure function called inside pure function.", what);
		return NULL;
	}
	if (impure)
	{
		ctx->impureFunctionCalled->i = 1;
	}
	if (! ctxName)
	{
		ctx->expressionIsUnknownFunctionCall->i = 1;
	}
	for (unsigned i = 1; i < st_count (pn->sub_tree); ++i)
	{
		if (i > 1)
		{
			end = sl_copy_append (end, ",");
		}
		end = sl_splice (end, eval (ctx, st_at (pn->sub_tree, i)));
		if (ctx->has_error)
		{
			return NULL;
		}
	}
	end = sl_copy_append (end, ")");

	if (! ctxName)
	{
		ctx->lastExpressionType = ctx->unknown_type;
	}
	else
	{
		ctx->lastExpressionType = st_last_non_close (ctxName->sub_tree->sub_tree);
	}
	
	return begin;
}
