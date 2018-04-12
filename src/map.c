/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <assert.h>
#include <stdlib.h>

#include "eval.h"

/*(map ARRAY FUNCTION)*/
struct string_list *
eval_map (struct context * const ctx, struct syntax_tree * const pn)
{
	int const is_doEach = pn && pn->sub_tree && pn->sub_tree->next && ! t_strcmp (pn->sub_tree->next->token, "doEach");

	if (st_count (pn->sub_tree) != 3)
	{
		context_set_error (ctx, is_doEach ? "Incorrect number of arguments to 'doEach'." : "Incorrect number of arguments to 'map'.", pn);
		return NULL;
	}

	if (is_doEach && ctx->insidePureFunction->i)
	{
		context_set_error (ctx, "Function 'doEach' called inside pure function.", pn);
		return NULL;
	}

	struct syntax_tree * const n_array_expr = st_at (pn->sub_tree, 1);
	struct syntax_tree * const n_func_expr  = st_at (pn->sub_tree, 2);

	struct string_list * const evaluated_array = eval (ctx, n_array_expr);
	struct syntax_tree * const qualified_array_type = ctx->lastExpressionType;
	struct syntax_tree * const array_type = is_type_mutable (qualified_array_type) ? st_at (qualified_array_type->sub_tree, 1) : qualified_array_type;

	if (! is_type_array (array_type))
	{
		context_set_error (ctx, is_doEach ? "Function 'doEach' expects an array." : "Function 'map' expects an array.", n_array_expr);
		return NULL;
	}

	struct syntax_tree * const array_underlying_type = st_at (array_type->sub_tree, 1);

	int const func_expr_is_name = n_func_expr->token.type == token_type_name;

	struct syntax_tree * func_lookup = NULL;
	int is_func_global = 0;
	struct syntax_tree * func_type = NULL;
	struct string_list * evaluated_func = NULL;
	if (func_expr_is_name)
	{
		func_lookup = context_findLocal (ctx, n_func_expr->token);
		if (func_lookup && ! is_type_function (func_lookup->sub_tree))
		{
			context_set_error (ctx, is_doEach ? "Function 'doEach' expects a function." : "Function 'map' expects a function.", n_func_expr);
			return NULL;
		}
		if (func_lookup)
		{
			evaluated_func = eval (ctx, n_func_expr);
			if (ctx->has_error)
			{
				return NULL;
			}
		}

		is_func_global = (func_lookup == NULL);

		if (! func_lookup)
		{
			func_lookup = context_findGlobal (ctx, n_func_expr->token);
			if ((! func_lookup) || (! is_type_function (func_lookup->sub_tree)))
			{
				context_set_error (ctx, "Function name not known.", n_func_expr);
				return NULL;
			}
		}

		func_type = func_lookup->sub_tree;
	}
	else
	{
		is_func_global = 0;

		evaluated_func = eval (ctx, n_func_expr);
		if (ctx->has_error)
		{
			return NULL;
		}

		func_type = ctx->lastExpressionType;
		if (! is_type_function (func_type))
		{
			context_set_error (ctx, is_doEach ? "Function 'doEach' expects a function." : "Function 'map' expects a function.", n_func_expr);
			return NULL;
		}
	}

	assert (array_underlying_type);
	struct string_list * const evaluated_array_type_sl = eval_type (ctx, array_underlying_type, 0);
	char * evaluated_array_type_str;
	sl_fold (& evaluated_array_type_str, evaluated_array_type_sl);
	del_string_list (evaluated_array_type_sl);

	assert (func_type);
	int const is_func_impure = ! t_strcmp (func_type->next->token, "imp");
	if (is_func_impure && ctx->insidePureFunction->i)
	{
		context_set_error (ctx, "Impure function called inside pure function.", n_func_expr);
		return NULL;
	}
	struct syntax_tree * const func_return_type = st_last_non_close (func_type->sub_tree);
	struct string_list * const evaluated_func_return_type_sl = eval_type (ctx, func_return_type, 1);
	char * evaluated_func_return_type_str;
	sl_fold (& evaluated_func_return_type_str, evaluated_func_return_type_sl);
	del_string_list (evaluated_func_return_type_sl);

	struct token const tmp_array = context_make_temporary (ctx);

	struct token tmp_func = is_func_global ? n_func_expr->token : context_make_temporary (ctx);
	if (is_func_global)
	{
		tmp_func.capacity = 0;
	}

	struct token tmp_dest_array;
	if (! is_doEach)
	{
		tmp_dest_array = context_make_temporary (ctx);
	}

	struct token const tmp_array_count = context_make_temporary (ctx);

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	if (! is_doEach)
	{
		end = sl_copy_append (end, "(");
	}
	end = sl_copy_append (end, "{");

	end = sl_copy_append (end, evaluated_array_type_str);
	end = sl_copy_append (end, "*");
	end = sl_copy_append_t (end, tmp_array);
	end = sl_copy_append (end, "=");
	end = sl_splice (end, evaluated_array);
	end = sl_copy_append (end, ";unsigned ");
	end = sl_copy_append_t (end, tmp_array_count);
	end = sl_copy_append (end, "=((unsigned*)");
	end = sl_copy_append_t (end, tmp_array);
	end = sl_copy_append (end, ")[-2];");

	if (! is_func_global)
	{
		assert (evaluated_func);
		end = sl_copy_append (end, "void**");
		end = sl_copy_append_t (end, tmp_func);
		end = sl_copy_append (end, "=(void**)");
		end = sl_splice (end, evaluated_func);
		end = sl_copy_append (end, ";");
		end = sl_splice (end, retainObject (tmp_func));
	}

	if (! is_doEach)
	{
		end = sl_copy_append (end, evaluated_func_return_type_str);
		end = sl_copy_append (end, "*");
		end = sl_copy_append_t (end, tmp_dest_array);
		end = sl_copy_append (end, "=(");
		end = sl_copy_append (end, evaluated_func_return_type_str);
		end = sl_copy_append (end, "*)malloc((sizeof(unsigned)*2)+sizeof(");
		end = sl_copy_append (end, evaluated_func_return_type_str);
		end = sl_copy_append (end, ")*");
		end = sl_copy_append_t (end, tmp_array_count);
		end = sl_copy_append (end, ");");
		end = sl_copy_append_t (end, tmp_dest_array);
		end = sl_copy_append (end, "=(");
		end = sl_copy_append (end, evaluated_func_return_type_str);
		end = sl_copy_append (end, "*)(((unsigned*)");
		end = sl_copy_append_t (end, tmp_dest_array);
		end = sl_copy_append (end, ")+2);((unsigned*)");
		end = sl_copy_append_t (end, tmp_dest_array);
		end = sl_copy_append (end, ")[-2]=");
		end = sl_copy_append_t (end, tmp_array_count);
		end = sl_copy_append (end, ";((unsigned*)");
		end = sl_copy_append_t (end, tmp_dest_array);
		end = sl_copy_append (end, ")[-1]=1;");
	}

	struct token tmp_ri;
	if (! is_doEach)
	{
		tmp_ri = context_make_temporary (ctx);
	}
	struct token const tmp_i = context_make_temporary (ctx);
	struct token const tmp_e = context_make_temporary (ctx);

	if (! is_doEach)
	{
		end = sl_copy_append (end, evaluated_func_return_type_str);
		end = sl_copy_append (end, "*");
		end = sl_copy_append_t (end, tmp_ri);
		end = sl_copy_append (end, "=");
		end = sl_copy_append_t (end, tmp_dest_array);
		end = sl_copy_append (end, ";");
	}

	end = sl_copy_append (end, evaluated_array_type_str);
	end = sl_copy_append (end, "*");
	end = sl_copy_append_t (end, tmp_i);
	end = sl_copy_append (end, "=");
	end = sl_copy_append_t (end, tmp_array);
	end = sl_copy_append (end, ";");

	end = sl_copy_append (end, evaluated_array_type_str);
	end = sl_copy_append (end, "*");
	end = sl_copy_append_t (end, tmp_e);
	end = sl_copy_append (end, "=");
	end = sl_copy_append_t (end, tmp_array);
	end = sl_copy_append (end, "+");
	end = sl_copy_append_t (end, tmp_array_count);
	end = sl_copy_append (end, ";");

	end = sl_copy_append (end, "for(;");
	end = sl_copy_append_t (end, tmp_i);
	end = sl_copy_append (end, "!=");
	end = sl_copy_append_t (end, tmp_e);
	end = sl_copy_append (end, ";++");
	end = sl_copy_append_t (end, tmp_i);
	if (! is_doEach)
	{
		end = sl_copy_append (end, ",++");
		end = sl_copy_append_t (end, tmp_ri);
	}
	end = sl_copy_append (end, "){");

	int const func_return_type_is_reference_counted = is_type_reference_counted (ctx, func_return_type);
	struct token tmp_discard;

	if (is_doEach)
	{
		if (func_return_type_is_reference_counted)
		{
			tmp_discard = context_make_temporary (ctx);
			end = sl_copy_append (end, evaluated_func_return_type_str);
			end = sl_copy_append_t (end, tmp_discard);
			end = sl_copy_append (end, "=");
		}
		else
		{
			end = sl_copy_append (end, "(void)");
		}
	}
	else
	{
		end = sl_copy_append (end, "(*");
		end = sl_copy_append_t (end, tmp_ri);
		end = sl_copy_append (end, ")=");
	}

	if (is_type_reference_counted (ctx, array_underlying_type))
	{
		struct token tmp_elem = context_make_temporary (ctx);
		if (is_func_global)
		{
			end = sl_copy_append_t (end, tmp_func);
			end = sl_copy_append (end, "(");
		}
		else
		{
			end = sl_copy_append (end, "((");
			end = sl_splice (end, functionToCExpr (ctx, func_type, NULL, "void"));
			end = sl_copy_append (end, ")*");
			end = sl_copy_append_t (end, tmp_func);
			end = sl_copy_append (end, ")(");
			end = sl_copy_append_t (end, tmp_func);
			end = sl_copy_append (end, ",");
		}
		end = sl_copy_append (end, "({");
		end = sl_copy_append (end, evaluated_array_type_str);
		end = sl_copy_append_t (end, tmp_elem);
		end = sl_copy_append (end, "=*");
		end = sl_copy_append_t (end, tmp_i);
		end = sl_copy_append (end, ";");
		end = sl_splice (end, retainObject (tmp_elem));
		end = sl_copy_append_t (end, tmp_elem);
		end = sl_copy_append (end, "}));");
		free (tmp_elem.bytes);
	}
	else
	{
		if (is_func_global)
		{
			end = sl_copy_append_t (end, tmp_func);
			end = sl_copy_append (end, "(");
		}
		else
		{
			end = sl_copy_append (end, "((");
			end = sl_splice (end, functionToCExpr (ctx, func_type, NULL, "void"));
			end = sl_copy_append (end, ")*");
			end = sl_copy_append_t (end, tmp_func);
			end = sl_copy_append (end, ")(");
			end = sl_copy_append_t (end, tmp_func);
			end = sl_copy_append (end, ",");
		}
		end = sl_copy_append (end, "*");
		end = sl_copy_append_t (end, tmp_i);
		end = sl_copy_append (end, ");");
	}

	if (is_doEach && func_return_type_is_reference_counted)
	{
		end = sl_splice (end, releaseObject (ctx, tmp_discard, func_return_type));
	}

	end = sl_copy_append (end, "}");
	if (! is_doEach)
	{
		end = sl_copy_append_t (end, tmp_dest_array);
		end = sl_copy_append (end, ";");
	}
	end = sl_copy_append (end, "}");
	if (! is_doEach)
	{
		end = sl_copy_append (end, ")");
	}

	if (is_doEach || is_func_impure)
	{
		ctx->impureFunctionCalled->i = 1;
	}

	if (! is_doEach)
	{
		free (tmp_ri.bytes);
	}
	free (tmp_i.bytes);
	free (tmp_e.bytes);
	if (tmp_func.capacity)
	{
		free (tmp_func.bytes);
	}
	if (! is_doEach)
	{
		free (tmp_dest_array.bytes);
	}
	free (tmp_array_count.bytes);
	free (tmp_array.bytes);

	free (evaluated_func_return_type_str);
	free (evaluated_array_type_str);

	if (is_doEach)
	{
		ctx->lastExpressionType = new_syntax_tree (make_str_token ("void"), NULL);
	}
	else
	{
		struct syntax_tree * const t = new_syntax_tree (make_token (token_type_open, & open_brace_char, 1, 0, 0, 0), NULL);
		t->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), t);
		st_add (t->sub_tree, new_syntax_tree (make_str_token ("array"), t));
		st_add (t->sub_tree->next, st_copy (func_return_type));
		t->sub_tree->next->next->parent_tree = t;
		st_add (t->sub_tree->next->next, new_syntax_tree (make_token (token_type_close, & close_brace_char, 1, 0, 0, 0), t));
		t->sub_tree->next->next->next->parent_tree = t;
		ctx->lastExpressionType = t;
	}

	return begin;
}
