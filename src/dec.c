/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <assert.h>
#include <stdlib.h>

#include "eval.h"

struct string_list *
eval_dec_as_variable (struct context * const ctx, struct syntax_tree * const pn)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	line (ctx, pn->token.line);
	end = sl_copy_append (end, "extern ");
	struct syntax_tree * n_name = st_at (pn->sub_tree, 1);
	struct syntax_tree * n_type = st_at (pn->sub_tree, 2);
	end = sl_splice (end, eval_type (ctx, n_type, 0));
	if (ctx->has_error)
	{
		return NULL;
	}
	end = sl_copy_append_t (end, n_name->token);
	end = sl_copy_append (end, ";");
	ctx->lastExpressionType = NULL;

	name_stack_push (& ctx->globals, n_name->token, n_type, NULL);

	return begin;
}

/*(dec NAME (TYPE ...)) function*/
struct string_list *
eval_dec_as_function (struct context * const ctx, struct syntax_tree * pn)
{
	struct syntax_tree * nName  = st_at (pn->sub_tree, 1);
	struct syntax_tree * n_args = st_at (pn->sub_tree, 2);
	unsigned const typesSize = st_count (n_args->sub_tree);

	if (typesSize < 1)
	{
		
		context_set_error (ctx, "Unexpected empty expression", n_args);
		return NULL;
	}
	int const impure = ! t_strcmp (st_at (n_args->sub_tree, 0)->token, "imp");
	unsigned const numTypes = (typesSize - (impure ? 1 : 0));

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	name_stack_push (& ctx->globals, nName->token, n_args, NULL);

	line (ctx, pn->token.line);
	end = sl_copy_append (end, "extern ");
	struct syntax_tree * nReturn = st_last_non_close (n_args->sub_tree);
	struct string_list * return_type_sl = eval_type (ctx, nReturn, 1);
	if (ctx->has_error)
	{
		return NULL;
	}
	char * return_type = NULL;
	sl_fold (& return_type, return_type_sl);
	if (! impure && ! strcmp (return_type, "void"))
	{
		free (return_type);
		
		context_set_error (ctx, "Function declared as pure but returns void.", nReturn);
	}
	end = sl_copy_append (end, return_type);
	free (return_type);
	end = sl_copy_append_t (end, nName->token);
	end = sl_copy_append (end, "(");

	if (1 == numTypes)
	{
		end = sl_copy_append (end, "void");
	}
	else
	{
		for (int i = impure ? 1 : 0, i_end = numTypes - 1; i < i_end; ++ i)
		{
			if (i > (impure ? 1 : 0))
			{
				end = sl_copy_append (end, ",");
			}
			struct syntax_tree * nParamType = st_at (st_at (n_args->sub_tree, i), 1);
			end = sl_splice (end, eval_type (ctx, nParamType, 0));
		}
	};
	end = sl_copy_append (end, ");");

	ctx->lastExpressionType = NULL;

	return begin;
}

/*(dec NAME struct) struct*/
struct string_list *
eval_dec_as_struct(struct context * const ctx, struct syntax_tree * pn)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	if (ctx->isDefAsDecMode)
	{
		end = sl_splice (end, eval_def (ctx, pn));
	}
	else
	{
		struct syntax_tree * nName = st_at (pn->sub_tree, 1);
		if (token_type_name != nName->token.type)
		{
			
			context_set_error (ctx, "Name expected", nName);
			return NULL;
		}
		if (is_type_struct_name (ctx, nName))
		{
			
			context_set_error (ctx, "Name already defined", nName);
			return NULL;
		}

		line (ctx, pn->token.line);
		end = sl_copy_append (end, "struct ");
		end = sl_copy_append_t (end, nName->token);
		end = sl_copy_append (end, ";");

		ctx->lastExpressionType = NULL;
	}

	return begin;
}

/*(dec NAME TYPE) general*/
struct string_list *
eval_dec (struct context * const ctx, struct syntax_tree * pn)
{
	assert(token_type_open == pn->token.type);
	if (! ctx->isDefAsDecMode)
	{
		assert (! t_strcmp (pn->sub_tree->next->token, "dec"));
	}
	if ((ctx->isDefAsDecMode && 4 != st_count (pn->sub_tree))
	    || (! ctx->isDefAsDecMode && 3 != st_count (pn->sub_tree)))
	{
		
		context_set_error (ctx, "Incorrect number of expressions in 'dec'", pn);
		return NULL;
	}

	struct syntax_tree * nType = st_at (pn->sub_tree, 2);
	if (is_type_primative (nType) || is_type_pointer (nType))
	{
		return eval_dec_as_variable (ctx, pn);
	}
	else if (! t_strcmp (nType->token, "struct"))
	{
		return eval_dec_as_struct (ctx, pn);
	}
	else if (token_type_open == nType->token.type)
	{
		return eval_dec_as_function (ctx, pn);
	}
	else
	{
		context_set_error (ctx, "'dec' type not recognised", pn);
		return NULL;
	}
}

struct string_list *
eval_def_as_dec (struct context * const ctx, struct syntax_tree * pn)
{
	ctx->isDefAsDecMode = 1;
	struct string_list * const ret = eval (ctx, pn);
	if (ctx->has_error)
	{
		return NULL;
	}
	ctx->isDefAsDecMode = 0;
	return ret;
}
