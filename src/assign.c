/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <stdlib.h>

#include "eval.h"

struct string_list *
eval_assign (struct context * const ctx, struct syntax_tree * const pn)
{
	if (ctx->insidePureFunction->i)
	{
		context_set_error (ctx, "Unexpected assign expression inside pure function.", pn);
		return NULL;
	}

	if (st_count (pn->sub_tree) != 3)
	{
		context_set_error (ctx, "Unexpected number of expressions.", pn);
		return NULL;
	}

	struct syntax_tree * n_assignee = st_at (pn->sub_tree, 1);
	struct syntax_tree * n_value = st_at (pn->sub_tree, 2);

	struct string_list * evaluated_assignee_sl = eval (ctx, n_assignee);
	if (ctx->has_error)
	{
		return NULL;
	}
	char * evaluated_assignee;
	sl_fold (& evaluated_assignee, evaluated_assignee_sl);
	del_string_list (evaluated_assignee_sl);

	struct syntax_tree * assignee_type = ctx->lastExpressionType;

	int const reference_counted = is_type_reference_counted (ctx, assignee_type);

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	struct string_list * exp = eval (ctx, n_value);
	if (ctx->has_error)
	{
		return NULL;
	}
	if (! types_equal_modulo_mutability (ctx->lastExpressionType, assignee_type))
	{
		context_set_error (ctx, "Assign expression type doesn't match target's' type.", n_assignee);
		return NULL;
	}

	if (reference_counted)
	{
		struct token tmp0 = context_make_temporary (ctx);
		struct token tmp1 = context_make_temporary (ctx);

		end = sl_copy_append (end, "{");

		end = sl_splice (end, eval_type (ctx, assignee_type, 0));
		if (ctx->has_error)
		{
			return NULL;
		}
		end = sl_copy_append_t (end, tmp0);
		end = sl_copy_append (end, "=");
		end = sl_copy_append (end, evaluated_assignee);
		end = sl_copy_append (end, ";");

		end = sl_splice (end, eval_type (ctx, assignee_type, 0));
		if (ctx->has_error)
		{
			return NULL;
		}
		end = sl_copy_append_t (end, tmp1);
		end = sl_copy_append (end, "=");
		end = sl_splice (end, exp);
		end = sl_copy_append (end, ";");

		// The reference count on the result of exp is already at the needed level, no need to retain further.

		end = sl_copy_append (end, evaluated_assignee);
		end = sl_copy_append (end, "=");
		end = sl_copy_append_t (end, tmp1);
		end = sl_copy_append (end, ";");

		end = sl_splice (end, releaseObject (ctx, tmp0, assignee_type)); // Consume assignee evaluated first time.
		end = sl_splice (end, releaseObject (ctx, tmp0, assignee_type)); // Consume assignee evaluated second time.
		end = sl_splice (end, releaseObject (ctx, tmp0, assignee_type)); // Consume assignee replaced.

		end = sl_copy_append (end, "}");

		free (tmp0.bytes);
		free (tmp1.bytes);
	}
	else
	{
		end = sl_copy_append (end, evaluated_assignee);
		end = sl_copy_append (end, "=");
		end = sl_splice (end, exp);
	}

	// The semicolon is added by the calling expression.

	// Forces the expression to be used inside a 'do' expression or a void function.
	ctx->lastExpressionType = new_syntax_tree (make_str_token ("void"), NULL);
	ctx->impureFunctionCalled->i = 1;

	return begin;
}
