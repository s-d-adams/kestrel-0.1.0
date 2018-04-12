/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "eval.h"

static
struct string_list *
eval_let_dec_as_variable (struct context * const ctx, struct syntax_tree * dec, struct syntax_tree * objects)
{
	size_t const num_subexpressions = st_count (dec->sub_tree);
	int const short_form = (2 == num_subexpressions);

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	struct syntax_tree * const name  = st_at (dec->sub_tree, 0);
	struct syntax_tree * const value = st_at (dec->sub_tree, short_form ? 1 : 2);

	struct string_list * const evaluated_value = eval (ctx, value);
	if (ctx->has_error)
	{
		return NULL;
	}

	struct syntax_tree * const type  = short_form ? ctx->lastExpressionType : st_at (dec->sub_tree, 1);

	int const reference_counted = is_type_reference_counted (ctx, type);

	line (ctx, type->token.line);
	end = sl_splice (end, eval_type (ctx, type, 0));
	if (ctx->has_error)
	{
		return NULL;
	}
	line(ctx, st_at (dec->sub_tree, 0)->token.line);
	end = sl_copy_append_t (end, name->token);
	end = sl_copy_append (end, "=");
	end = sl_splice (end, evaluated_value);

	if ((! short_form) && (! types_equal_modulo_mutability (ctx->lastExpressionType, type)))
	{
		context_set_error (ctx, "Type of expression does not match the expected type.", dec);
		return NULL;
	}
	end = sl_copy_append (end, ";");
	name_stack_push (& ctx->stack->sub_tree, name->token, type, NULL);

	if (reference_counted)
	{
		struct syntax_tree * t = new_syntax_tree (name->token, NULL);
		t->token.capacity = 0;
		t->sub_tree = type;
		st_add (objects, t);
	}

	ctx->lastExpressionType = NULL;

	return begin;
}

struct string_list *
eval_let (struct context * const ctx, struct syntax_tree * pn)
{
	struct string_list * const b_decs = new_string_list (NULL, 0, 0), * e_decs;
	e_decs = b_decs;

	assert (token_type_open == pn->token.type);
	assert (! t_strcmp (pn->sub_tree->next->token, "let"));
	if (3 != st_count (pn->sub_tree))
	{
		context_set_error (ctx, "Incorrect number of expressions passed to 'let'", pn);
		return NULL;
	}

	struct syntax_tree * decs = st_at (pn->sub_tree, 1);

	struct syntax_tree * objects = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);

	size_t const numDecs = (token_type_open == st_at (decs->sub_tree, 0)->token.type ? st_count (decs->sub_tree) : 1);
	if (numDecs > 1)
	{
		for (size_t i = 0; i < numDecs; ++ i)
		{
			e_decs = sl_splice (e_decs, eval_let_dec_as_variable (ctx, st_at (decs->sub_tree, i), objects));
		}
	}
	else
	{
		e_decs = sl_splice (e_decs, eval_let_dec_as_variable (ctx, decs, objects));
	}

	struct string_list * const exp1 = eval (ctx, st_at (pn->sub_tree, 2));
	if (ctx->has_error)
	{
		return NULL;
	}

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	int const result_is_unknown = (ctx->lastExpressionType == ctx->unknown_type);
	int const result_is_void = is_type_void (ctx->lastExpressionType) || result_is_unknown;

	if (result_is_void && ctx->insidePureFunction->i)
	{
		context_set_error (ctx, "Return type of 'let' expression in pure function is void.", pn);
		return NULL;
	}

	line (ctx, pn->token.line);
	if (result_is_void)
	{
		end = sl_copy_append (end, "{");
	}
	else
	{
		end = sl_copy_append (end, "({");
	}
	end = sl_splice (end, b_decs);

	struct token tmp;
	if (! result_is_void)
	{
		tmp = context_make_temporary (ctx);
	}

	if (! result_is_void)
	{
		end = sl_splice (end, eval_type (ctx, ctx->lastExpressionType, 0));
		if (ctx->has_error)
		{
			return NULL;
		}
		end = sl_copy_append_t (end, tmp);
		end = sl_copy_append (end, "=");
	}
	end = sl_splice (end, exp1);
	end = sl_copy_append (end, ";");

	struct syntax_tree * ob = objects->next;
	for (; ob; ob = ob->next)
	{
		end = sl_splice (end, releaseObject (ctx, ob->token, ob->sub_tree));
		ob->sub_tree = NULL;
	}
	del_syntax_tree (objects);
	for (unsigned i=0; i<numDecs; ++i)
	{
		name_stack_pop (& ctx->stack->sub_tree);
	}

	if (result_is_void)
	{
		end = sl_copy_append (end, "}");
	}
	else
	{
		end = sl_copy_append_t (end, tmp);
		end = sl_copy_append (end, ";})");
		free (tmp.bytes);
	}

	return begin;
}
