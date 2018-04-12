/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include "eval.h"

/*(construct_array TYPE (EXPR ...))*/
struct string_list *
eval_construct_array (struct context * const ctx, struct syntax_tree * pn)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	struct syntax_tree * n_type = st_at (pn->sub_tree, 0);

	struct string_list * const type_sl = eval_type (ctx, n_type, 1);
	if (ctx->has_error)
	{
		return NULL;
	}
	char * type_str;
	sl_fold (& type_str, type_sl);
	del_string_list (type_sl);

	struct string_list * const array_type_sl = eval_type (ctx, st_at (n_type->sub_tree, 1), 1);
	if (ctx->has_error)
	{
		return NULL;
	}
	char * array_type_str;
	sl_fold (& array_type_str, array_type_sl);
	del_string_list (array_type_sl);

	struct token tmp0 = context_make_temporary (ctx);
	struct token tmp1 = context_make_temporary (ctx);

	struct syntax_tree * values = st_at (pn->sub_tree, 1);
	if (values->token.type != token_type_open)
	{
		context_set_error (ctx, "Array expression expects a list of elements.", values);
		return NULL;
	}
	size_t const arraySize = st_count (values->sub_tree);
	char * arraySizeStr = NULL;
	asprintf (& arraySizeStr, "%lu", arraySize);
	line (ctx, n_type->token.line);
	end = sl_copy_append (end, "({");
	end = sl_copy_append (end, array_type_str);
	line (ctx, pn->sub_tree->next->token.line);
	end = sl_copy_append (end, "*");
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, "=(");
	line (ctx, values->token.line);
	end = sl_copy_append (end, array_type_str);
	end = sl_copy_append (end, "*)malloc((sizeof(unsigned)*2)+(sizeof(");
	end = sl_copy_append (end, array_type_str);
	end = sl_copy_append (end, ")*");
	end = sl_copy_append (end, arraySizeStr);
	end = sl_copy_append (end, "));");
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, "=(");
	end = sl_copy_append (end, array_type_str);
	free (type_str);
	end = sl_copy_append (end, "*)(((unsigned*)");
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, ")+2);((unsigned*)");
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, ")[-2]=");
	end = sl_copy_append (end, arraySizeStr);
	end = sl_copy_append (end, ";((unsigned*)");
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, ")[-1]=1;{");
	end = sl_copy_append (end, "struct ");
	end = sl_copy_append_t (end, tmp1);
	end = sl_copy_append (end, "{");
	end = sl_copy_append (end, array_type_str);
	free (array_type_str);
	end = sl_copy_append (end, "x[");
	end = sl_copy_append (end, arraySizeStr);
	end = sl_copy_append (end, "];};");
	end = sl_copy_append (end, "*((struct ");
	end = sl_copy_append_t (end, tmp1);
	end = sl_copy_append (end, "*)");
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, ")=");
	end = sl_copy_append (end, "(struct ");
	end = sl_copy_append_t (end, tmp1);
	end = sl_copy_append (end, "){");
	for (size_t i = 0; i < arraySize; ++ i)
	{
		if (i > 0)
		{
			end = sl_copy_append (end, ",");
		}
		end = sl_splice (end, eval (ctx, st_at (values->sub_tree, i)));
		if (ctx->has_error)
		{
			return NULL;
		}
	}
	end = sl_copy_append (end, "};};");
	end = sl_copy_append_t (end, tmp0);
	end = sl_copy_append (end, ";})");
	free (arraySizeStr);
	free (tmp0.bytes);
	free (tmp1.bytes);

	ctx->lastExpressionType = n_type;

	return begin;
}
