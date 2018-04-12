/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <assert.h>
#include <stdlib.h>

#include "eval.h"

struct string_list *
eval_do (struct context * const ctx, struct syntax_tree * pn)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	line (ctx, pn->token.line);
	end = sl_copy_append (end, "({");

	assert (token_type_open == pn->token.type);
	assert (! t_strcmp (pn->sub_tree->next->token, "do"));
	unsigned const numExpressions = st_count (pn->sub_tree);
	if (3 > numExpressions)
	{
		context_set_error (ctx, "Incorrect number of expressions passed to 'do'", pn);
		return NULL;
	}

	unsigned const onePastLastSideEffect = numExpressions - 1;
	for (unsigned i = 1; i < onePastLastSideEffect; ++ i)
	{
		struct syntax_tree * sideEffect = st_at (pn->sub_tree, i);
		int_stack_push (& ctx->impureFunctionCalled, 0);

		struct string_list * const exp = eval (ctx, sideEffect);
		if (ctx->has_error)
		{
			return NULL;
		}

		struct syntax_tree * sideEffectType = ctx->lastExpressionType;
		if (! ctx->impureFunctionCalled->i)
		{
			context_set_error (ctx, "'do' expects an impure expression", sideEffect);
			return NULL;
		}
		int_stack_pop (& ctx->impureFunctionCalled);
		int const isPrimative = is_type_primative (sideEffectType);
		int const isUnknown = sideEffectType == ctx->unknown_type;
		int const isVoid = is_type_void (sideEffectType);
		struct token tmp;
		if (isPrimative || isUnknown)
		{
			end = sl_copy_append (end, "(void)");
		}
		else if (! isVoid)
		{
			tmp = context_make_temporary (ctx);
			end = sl_copy_append (end, "{");
			end = sl_splice (end, eval_type (ctx, sideEffectType, 0));
			end = sl_copy_append_t (end, tmp);
			end = sl_copy_append (end, "=");
		}
		end = sl_splice (end, exp);
		if (isPrimative || isUnknown || isVoid)
		{
			end = sl_copy_append (end, ";");
		}
		else
		{
			end = sl_copy_append (end, ";");
			end = sl_splice (end, releaseObject (ctx, tmp, sideEffectType));
			end = sl_copy_append (end, "}");
			free (tmp.bytes);
		}
	}
	ctx->impureFunctionCalled->i = 1;
	end = sl_splice (end, eval (ctx, st_at (pn->sub_tree, onePastLastSideEffect)));
	if (ctx->has_error)
	{
		return NULL;
	}
	end = sl_copy_append (end, ";})");

	return begin;
}
