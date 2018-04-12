/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include "eval.h"

/*(construct TYPE ((FIELD VALUE) ...))*/
struct string_list *
eval_construct (struct context * const ctx, struct syntax_tree * pn)
{
	if (2 != st_count (pn->sub_tree))
	{
		context_set_error (ctx, "Incorrect number of expressions passed to construction.", pn);
		return NULL;
	}

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	struct syntax_tree * const qualifiedStructType = st_at (pn->sub_tree, 0);
	struct syntax_tree * nValue = st_at (pn->sub_tree, 1);

	int const is_mutable = is_type_mutable (qualifiedStructType);

	if (is_mutable && ctx->insidePureFunction->i)
	{
		context_set_error (ctx, "Constructing a mutable inside a pure function.", qualifiedStructType);
		return NULL;
	}

	struct syntax_tree * const structType = is_mutable ? st_at (qualifiedStructType->sub_tree, 1) : qualifiedStructType;

	if (! is_type_struct_name (ctx, structType))
	{
		context_set_error (ctx, "Expected a known struct type name.", structType);
		return NULL;
	}

	struct syntax_tree * const ctxStruct = context_findStruct (ctx, structType->token);

	struct token tmp = context_make_temporary (ctx);

	line (ctx, pn->token.line);
	end = sl_copy_append (end, "({struct ");
	end = sl_copy_append_t (end, structType->token);
	end = sl_copy_append (end, "*");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "=");

	line (ctx, nValue->token.line);
	end = sl_copy_append (end, "(struct ");
	end = sl_copy_append_t (end, structType->token);
	end = sl_copy_append (end, "*)malloc(sizeof(unsigned)+sizeof(struct ");
	end = sl_copy_append_t (end, structType->token);
	end = sl_copy_append (end, "));{");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "=(struct ");
	end = sl_copy_append_t (end, structType->token);
	end = sl_copy_append (end, "*)(((unsigned*)");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, ")+1);((unsigned*)");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, ")[-1]=1;");
	end = sl_copy_append (end, "*");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "=(struct ");
	end = sl_copy_append_t (end, structType->token);
	end = sl_copy_append (end, "){");

	size_t const numFields = st_count (ctxStruct->next->sub_tree->sub_tree);
	if (numFields != st_count (nValue->sub_tree))
	{
		
		context_set_error (ctx, "Mismatched number of fields in struct.", nValue);
		return NULL;
	}
	struct string_list * fieldNames = new_string_list (NULL, 0, 0), * fne;
	fne = fieldNames;
	for (size_t i = 0; i < numFields; ++ i)
	{
		fne = sl_copy_append_t (fne, st_at (st_at (ctxStruct->next->sub_tree->sub_tree, i)->sub_tree, 0)->token);
	}
	for (size_t i = 0; i < numFields; ++ i)
	{
		struct syntax_tree * nFieldValue = st_at (nValue->sub_tree, i);
		if (nFieldValue->token.type != token_type_open)
		{
			
			context_set_error (ctx, "Ill-formed struct field list.", nFieldValue);
			return NULL;
		}
		struct string_list * it = fieldNames->next;
		for (; it; it = it->next)
		{
			if (! t_strncmp (st_at (nFieldValue->sub_tree, 0)->token, it->string, it->size))
			{
				break;
			}
		}
		if (! it)
		{
			
			context_set_error (ctx, "Field name already counted or not known.", st_at (nFieldValue->sub_tree, 0));
			return NULL;
		}
		else
		{
			take_string_list (it);
			del_string_list (it);
		}

		end = sl_copy_append (end, ".");
		end = sl_copy_append_t (end, st_at (nFieldValue->sub_tree, 0)->token);
		end = sl_copy_append (end, "=");

		end = sl_splice (end, eval (ctx, st_at (nFieldValue->sub_tree, 1)));
		if (ctx->has_error)
		{
			return NULL;
		}
		end = sl_copy_append (end, ",");
		struct syntax_tree * expectedType = NULL;
		for (unsigned i = 0; i < numFields; ++ i)
		{
			if (token_equal (st_at (st_at (ctxStruct->next->sub_tree->sub_tree, i)->sub_tree, 0)->token, st_at (nFieldValue->sub_tree, 0)->token))
			{
				expectedType = st_at (st_at (ctxStruct->next->sub_tree->sub_tree, i)->sub_tree, 1);
			}
		}
		if (! types_equal_modulo_mutability (ctx->lastExpressionType, expectedType))
		{
			
			context_set_error (ctx, "Value given to struct field has wrong type.", st_at (nFieldValue->sub_tree, 1));
			return NULL;
		}
	}
	end = sl_copy_append (end, "};");
	end = sl_copy_append (end, "};");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, ";})");

	ctx->lastExpressionType = qualifiedStructType;

	return begin;
}
