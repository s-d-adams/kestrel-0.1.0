/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "eval.h"
#include "type.h"

struct string_list *
eval_name (struct context * const ctx, struct syntax_tree * pn)
{
	assert (token_type_name == pn->token.type);
	 /*else non-symbol expression evald as a symbol*/;

	/* search context for pn->token
	   name not found is a compile error (no symbol named x in context)
	   if type is an array then increment the reference count
	   write the name */

	struct token name = pn->token;
	line (ctx, pn->token.line);

	if (! t_strcmp (name, "rec"))
	{
		if (!ctx->lambdaDepth)
		{
			context_set_error (ctx, "'rec' may only be used inside a 'lam'", pn);
		}
		struct string_list * const begin = sl_copy_str ("({"), * end;
		end = begin;
		end = sl_splice (end, retainObject (make_str_token ("_lc_ctx")));
		end = sl_copy_append (end, "_lc_ctx;})");
		ctx->lastExpressionType = ctx->lambdaTypeStack->sub_tree;
		return begin;
	}

	if (! t_strcmp (name, "null"))
	{
		ctx->lastExpressionType = new_syntax_tree (make_str_token ("Null"), NULL);
		return sl_copy_str ("NULL");
	}

	char const * const literalType = type_of_literal (name);
	if (literalType)
	{
		ctx->lastExpressionType = new_syntax_tree (make_str_token (literalType), NULL);
		return sl_copy_t (name);
	}
	else
	{
		int isClosedOver = 0;
		struct syntax_tree * ctxName = context_findNameInScope (ctx, name);
		if (! ctxName && ctx->lambdaDepth)
		{
			ctxName = context_findClosedOverName (ctx, name);
			if (ctxName)
			{
				isClosedOver = 1;
				struct syntax_tree * tname = new_syntax_tree (name, NULL);
				tname->token.capacity = 0;
				tname->prev = ctx->namesClosedOver->sub_tree;
				ctx->namesClosedOver->sub_tree->next = tname;
				ctx->namesClosedOver->sub_tree = tname;
			}
		}
		if (! ctxName)
		{
			if (! ctx->insidePureFunction->i)
			{
				ctx->lastExpressionType = ctx->unknown_type;
				return sl_copy_t (name);
			}
			context_set_error (ctx, "Name not in context", pn);
			return NULL;
		}
		struct token const dereferencedName = isClosedOver ? t_prefix ("_lc_ctx->", name) : name;
		ctx->lastExpressionType = ctxName->sub_tree;
		if (is_type_primative (ctxName->sub_tree))
		{
			return sl_copy_t (dereferencedName);
		}
		else
		{
			int isRetainable = 0;
			int isNotRetainable = 0;

			if (is_type_array (ctxName->sub_tree))
			{
				isRetainable = 1;
			}
			else if (is_type_pointer (ctxName->sub_tree))
			{
				isNotRetainable = 1;
			}
			else if (is_type_struct_name (ctx, ctxName->sub_tree))
			{
				isRetainable = 1;
			}
			else if (is_type_function (ctxName->sub_tree))
			{
				if ((! ctx->expressionIsUnknownFunctionCall->prev) || (! ctx->expressionIsUnknownFunctionCall->prev->i))
				{
					isRetainable = 1;
				}
				else
				{
					isNotRetainable = 1;
				}
			}

			if (isRetainable)
			{
				struct string_list * const begin = sl_copy_str ("({"), * end;
				end = begin;
				end = sl_splice (end, retainObject (dereferencedName));
				end = sl_copy_append_t (end, dereferencedName);
				end = sl_copy_append (end, ";})");
				return begin;
			}
			else if (isNotRetainable)
			{
				return sl_copy_t (dereferencedName);
			}
			else
			{
				context_set_error (ctx, "Reference to object of unknown type.", pn);
				return NULL;
			}
		}
	}
}
