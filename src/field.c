/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <assert.h>
#include <stdlib.h>

#include "eval.h"

struct string_list *
eval_field (struct context * const ctx, struct syntax_tree * pn)
{
	int const is_explicit = ! t_strcmp (pn->sub_tree->next->token, "field");
	size_t const expected_subexpression_count = is_explicit ? 3 : 2;
	size_t const num_subexpressions = st_count (pn->sub_tree);

	if (expected_subexpression_count > num_subexpressions)
	{
		context_set_error (ctx, "Incorrect number of expressions to struct field reference.", pn);
		return NULL;
	}

	struct syntax_tree * nStructExpr = st_at (pn->sub_tree, is_explicit ? 1 : 0);
	struct syntax_tree * n_first_field_name  = st_at (pn->sub_tree, is_explicit ? 2 : 1);

	if (token_type_name != n_first_field_name->token.type)
	{
		context_set_error (ctx, "Field expects a name.", n_first_field_name);
		return NULL;
	}

	struct string_list * const exp1 = eval (ctx, nStructExpr);
	if (ctx->has_error)
	{
		return NULL;
	}
	struct syntax_tree * nQualifiedStructType = ctx->lastExpressionType;

	int const is_mutable = is_type_mutable (nQualifiedStructType);

	struct syntax_tree * const nStructType = is_mutable ? st_at (nQualifiedStructType->sub_tree, 1) : nQualifiedStructType;

	struct syntax_tree * ctxRootStructType = context_findStruct (ctx, nStructType->token);
	if (!ctxRootStructType)
	{
		context_set_error (ctx, "Struct type passed to field not known.", nStructExpr);
		return NULL;
	}

	struct token tmp0 = context_make_temporary (ctx);
	struct token tmp1 = context_make_temporary (ctx);

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	line (ctx, pn->token.line);
	end = sl_copy_append (end, "({");
	end = sl_splice (end, eval_type (ctx, nStructType, 0));
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, "=");
	end = sl_splice (end, exp1);
	end = sl_copy_append (end, ";");

	struct string_list * const chain_b = new_string_list (NULL, 0, 0), * chain_e; // The string list to contain the arrow operator chain.
	chain_e = chain_b;
	struct syntax_tree * struct_lookup = ctxRootStructType; // The struct being indexed.
	int field_is_reference_counted = 0;
	struct syntax_tree * n_field_name = n_first_field_name; // The name of the field to access.
	struct syntax_tree * n_field_type_name = NULL;
	for (; n_field_name->token.type == token_type_name; n_field_name = n_field_name->next)
	{
		struct syntax_tree * const struct_type_description = struct_lookup->next->sub_tree;
	
		// Seeks through the type description to verify the struct has the requested field.
		size_t const size = st_count (struct_type_description->sub_tree);
		size_t index = 0;
		for (; index < size; ++index)
		{
			if (token_equal (st_at (st_at (struct_type_description->sub_tree, index)->sub_tree, 0)->token, n_field_name->token))
			{
				break;
			}
		}
		if (size == index)
		{
			context_set_error (ctx, "Field name not part of struct.", n_field_name);
			return NULL;
		}
	
		n_field_type_name = st_at (st_at (struct_type_description->sub_tree, index)->sub_tree, 1);
	
		field_is_reference_counted = is_type_reference_counted (ctx, n_field_type_name);
	
		// Need to chain these in a loop
		chain_e = sl_copy_append (chain_e, "->");
		chain_e = sl_copy_append_t (chain_e, n_field_name->token);

		if (field_is_reference_counted)
		{
			struct_lookup = context_findStruct (ctx, n_field_type_name->token);
		}

		if ((! field_is_reference_counted) || (! struct_lookup))
		{
			if (n_field_name->next && n_field_name->next->token.type != token_type_close)
			{
				context_set_error (ctx, "Attempting to access a member of a non-struct type.", n_field_name->next);
				return NULL;
			}
			else
			{
				break;
			}
		}
	}

	assert (n_field_type_name);

	line (ctx, n_first_field_name->token.line);
	end = sl_splice (end, eval_type (ctx, n_field_type_name, 0));
	end = sl_copy_append_t (end, tmp1);
	end = sl_copy_append (end, "=");
	end = sl_copy_append_t (end, tmp0);

	end = sl_splice (end, chain_b);

	end = sl_copy_append (end, ";");
	if (field_is_reference_counted)
	{
		end = sl_splice (end, retainObject (tmp1));
	}
	end = sl_splice (end, releaseObject (ctx, tmp0, nStructType));
	end = sl_copy_append_t (end, tmp1);
	end = sl_copy_append (end, ";})");
	free (tmp0.bytes);
	free (tmp1.bytes);
	ctx->lastExpressionType = n_field_type_name;
	return begin;
}
