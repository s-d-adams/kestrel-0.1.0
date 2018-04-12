/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <assert.h>
#include <stdlib.h>

#include "eval.h"
#include "compiler.h"
#include "keyword.h"

struct string_list *
eval_def_as_variable (struct context * const ctx, struct syntax_tree * pn)
{
	struct syntax_tree * n_name = st_at (pn->sub_tree, 1);
	struct syntax_tree * n_type = st_at (pn->sub_tree, 2);
	line (ctx, pn->token.line);
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	end = sl_splice (end, eval_type (ctx, n_type, 0));
	if (ctx->has_error)
	{
		return NULL;
	}

	line (ctx, st_at (pn->sub_tree, 1)->token.line);
	end = sl_copy_append_t (end, n_name->token);
	end = sl_copy_append (end, "=");
	end = sl_splice (end, eval (ctx, st_at (pn->sub_tree, 3)));
	if (ctx->has_error)
	{
		return NULL;
	}
	if (! types_equal_modulo_mutability (ctx->lastExpressionType, n_type))
	{
		if (! (is_type_pointer (n_type) && is_type_null (ctx->lastExpressionType)))
		{
			context_set_error (ctx, "Value does not match the declared type", st_at (pn->sub_tree, 3));
			return NULL;
		}
	}
	end = sl_copy_append (end, ";");
	name_stack_push (& ctx->globals, n_name->token, n_type, NULL);
	ctx->lastExpressionType = NULL;
	return begin;
}

struct string_list *
eval_def_function_with_name (struct context * const ctx, struct token const functionName, struct syntax_tree * const n_args, struct syntax_tree * const nExpr)
{
	struct string_list * const begin_1 = new_string_list (NULL, 0, 0);
	struct string_list * end_1 = begin_1;

	if (st_count (n_args->sub_tree) < 1)
	{
		
		context_set_error (ctx, "Unexpected empty expression.", n_args);
		return NULL;
	}
	int const is_imp = ! t_strcmp (st_at (n_args->sub_tree, 0)->token, "imp");
	int_stack_push (& ctx->insidePureFunction, ! is_imp);
	int_stack_push (& ctx->impureFunctionCalled, 0);
	unsigned const numTypes = (st_count (n_args->sub_tree) - (ctx->insidePureFunction->i ? 0 : 1));
	if (numTypes < 1)
	{
		context_set_error (ctx, "'imp' is not a parameter type.", n_args->sub_tree->next);
		return NULL;
	}

	name_stack_stack_push (& ctx->stack);
	name_stack_push (& ctx->globals, functionName, n_args, NULL);

	struct syntax_tree * const function_return_type_node = st_last_non_close (n_args->sub_tree);

	char * functionType;
	sl_fold (& functionType, eval_type (ctx, function_return_type_node, 1));

	int const function_is_void_return = is_type_void (function_return_type_node);
	if (function_is_void_return && ! is_imp)
	{
		context_set_error (ctx, "Function defined as pure but returns void.", function_return_type_node);
	}

	end_1 = sl_copy_append (end_1, functionType);
	end_1 = sl_copy_append_t (end_1, functionName);
	end_1 = sl_copy_append (end_1, "(");

	struct syntax_tree * objects = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);

	if (! ctx->lambdaDepth && 1 == numTypes)
	{
		end_1 = sl_copy_append (end_1, "void");
	}
	else
	{
		if (ctx->lambdaDepth)
		{
			end_1 = sl_copy_append (end_1, "struct _lc_ctx");
			end_1 = sl_copy_append_uint (end_1, ctx->lambdaCounter);
			end_1 = sl_copy_append (end_1, "*_lc_ctx");
		}
		for (int i = 0, end = (numTypes - 1); i < end; ++i)
		{
			if (ctx->lambdaDepth || i > 0)
			{
				end_1 = sl_copy_append (end_1, ",");
			}
			struct syntax_tree * const n_arg = st_at (n_args->sub_tree, i + (ctx->insidePureFunction->i ? 0 : 1));
			struct token param = st_at (n_arg->sub_tree, 0)->token;
			param.capacity = 0;
			struct syntax_tree * nParamType = st_at (n_arg->sub_tree, 1);
			name_stack_push (& ctx->stack->sub_tree, param, nParamType, NULL);
			end_1 = sl_splice (end_1, eval_type (ctx, nParamType, 0));
			if (ctx->has_error)
			{
				return NULL;
			}
			end_1 = sl_copy_append_t (end_1, param);
			if (is_type_reference_counted (ctx, nParamType))
			{
				struct syntax_tree * t = new_syntax_tree (param, NULL);
				t->token.capacity = 0;
				t->sub_tree = nParamType;
				st_add (objects, t);
			}
		}
	};
	end_1 = sl_copy_append (end_1, "){");
	struct string_list * body = eval (ctx, nExpr);
	if (ctx->has_error)
	{
		return NULL;
	}
	int const body_is_unknown_return = ctx->lastExpressionType == ctx->unknown_type;
	int const body_is_void_return = is_type_void (ctx->lastExpressionType);

	//FIXME: This temporary can be eliminated in the case when there is no reference counting to be done
	struct token tmp = context_make_temporary (ctx);

	if (function_is_void_return)
	{
		if (! (body_is_unknown_return || body_is_void_return))
		{
			struct string_list * return_type = eval_type (ctx, ctx->lastExpressionType, 0);
			if (ctx->has_error)
			{
				return NULL;
			}
			//FIXME: This temporary can be eliminated in the case when there is no reference counting to be done
			end_1 = sl_splice (end_1, return_type);
			end_1 = sl_copy_append (end_1, "");
			end_1 = sl_copy_append_t (end_1, tmp);
			end_1 = sl_copy_append (end_1, "=");
		}
	}
	else
	{
		if (! types_equal_modulo_mutability (ctx->lastExpressionType, st_last_non_close (n_args->sub_tree)))
		{
			context_set_error (ctx, "Function result doesn't match return type.", nExpr);
			return NULL;
		}
		//FIXME: This temporary can be eliminated in the case when there is no reference counting to be done
		if (! (body_is_unknown_return || body_is_void_return))
		{
			end_1 = sl_copy_append (end_1, functionType);
			end_1 = sl_copy_append (end_1, "");
			end_1 = sl_copy_append_t (end_1, tmp);
			end_1 = sl_copy_append (end_1, "=");
		}
	}
	free (functionType);

	end_1 = sl_splice (end_1, body);
	end_1 = sl_copy_append (end_1, ";");

	if (ctx->lambdaDepth)
	{
		end_1 = sl_copy_append (end_1, "_lc_release_lc_ctx");
		end_1 = sl_copy_append_uint (end_1, ctx->lambdaCounter);
		end_1 = sl_copy_append (end_1, "(_lc_ctx);");
	}

	struct syntax_tree * ob = objects->next;
	for (; ob; ob = ob->next)
	{
		end_1 = sl_splice (end_1, releaseObject (ctx, ob->token, ob->sub_tree));
		ob->sub_tree = NULL;
	}
	del_syntax_tree (objects);

	if (function_is_void_return)
	{
		if (! (body_is_unknown_return || body_is_void_return))
		{
			if (is_type_reference_counted (ctx, ctx->lastExpressionType))
			{
				end_1 = sl_splice (end_1, releaseObject (ctx, tmp, ctx->lastExpressionType));
				if (ctx->has_error)
				{
					return NULL;
				}
			}
		}
		end_1 = sl_copy_append (end_1, "return;}");
	}
	else
	{
		end_1 = sl_copy_append (end_1, "return ");
		end_1 = sl_copy_append_t (end_1, tmp);
		end_1 = sl_copy_append (end_1, ";}");
	}

	free (tmp.bytes);

	name_stack_stack_pop (& ctx->stack);
	ctx->lastExpressionType = NULL;
	if (!ctx->insidePureFunction->i && ! ctx->impureFunctionCalled->i)
	{
//FIXME: Provide the outermost open brace of the function definition as the expression.
		struct error_report msg = {"Function defined as impure but calls no impure functions.", nExpr};
		compiler_warn (ctx, msg);
	}
	int_stack_pop (& ctx->insidePureFunction);
	int_stack_pop (& ctx->impureFunctionCalled);

	struct string_list * lambdas_end = ctx->lambdas;
	ctx->lambdas = new_string_list (NULL, 0, 0);
	struct string_list * lambdas_begin = lambdas_end;
	for (; lambdas_begin->prev; lambdas_begin = lambdas_begin->prev);

	lambdas_end = sl_splice (lambdas_end, begin_1);

	if (ctx->lambdaDepth == 0)
	{
		context_reset_temporary_names (ctx);
	}

	return lambdas_begin;
}

/*(def NAME (TYPE ...) (NAME ... EXPR)) function*/
struct string_list *
eval_def_as_function (struct context * const ctx, struct syntax_tree * const pn)
{
	struct syntax_tree * nName  = st_at (pn->sub_tree, 1);
	struct syntax_tree * nTypes = st_at (pn->sub_tree, 2);
	struct syntax_tree * nExpr  = st_at (pn->sub_tree, 3);
	line (ctx, pn->token.line);
	return eval_def_function_with_name (ctx, nName->token, nTypes, nExpr);
}

struct string_list *
eval_def_struct_with_name (struct context * const ctx, struct syntax_tree * const structName, struct syntax_tree * const nFields, char const * const lambdaFunction)
{
	int const size = st_count (nFields->sub_tree);
	if (! lambdaFunction && size < 1)
	{
		context_set_error (ctx, "Unexpected empty expression", nFields);
		return NULL;
	}

	// 's' is for the struct definition. 'r' is for the release function body.
	struct string_list * const begin_s = new_string_list (NULL, 0, 0), * end_s;
	end_s = begin_s;
	struct string_list * const begin_r = new_string_list (NULL, 0, 0), * end_r;
	end_r = begin_r;

	if (lambdaFunction)
	{
		end_s = sl_copy_append (end_s, "\n#pragma pack(push, 1)\n");
	}

	end_s = sl_copy_append (end_s, "struct ");
	end_s = sl_copy_append_t (end_s, structName->token);
	end_s = sl_copy_append (end_s, "{");

	if (lambdaFunction)
	{
		end_s = sl_copy_append (end_s, lambdaFunction);
		end_s = sl_copy_append (end_s, "void (*_lc_rel)(struct ");
		end_s = sl_copy_append_t (end_s, structName->token);
		end_s = sl_copy_append (end_s, " const*);");
	}

	for (int i = 0; i < size; ++ i)
	{
		struct syntax_tree * nField = st_at (nFields->sub_tree, i);
		if (2 != st_count (nField->sub_tree))
		{
			context_set_error (ctx, "Incorrect number of expression in struct field definition", nField);
			return NULL;
		}
		struct syntax_tree * nName = st_at (nField->sub_tree, 0);
		if (token_type_name != nName->token.type)
		{
			context_set_error (ctx, "Expression expected to be a name", nName);
			return NULL;
		}
		struct syntax_tree * nType = st_at (nField->sub_tree, 1);

		struct string_list * const type_sl = eval_type (ctx, nType, 1);
		if (! type_sl)
		{
			context_set_error (ctx, "Struct field type not known", nType);
			return NULL;
		}
		end_s = sl_splice (end_s, type_sl);
		end_s = sl_copy_append_t (end_s, nName->token);
		end_s = sl_copy_append (end_s, ";");

		if (is_type_reference_counted (ctx, nType))
		{
			end_r = sl_splice (end_r, releaseObject (ctx, t_prefix ("st->", nName->token), nType));
		}
	}
	end_s = sl_copy_append (end_s, "};");

	if (lambdaFunction)
	{
		end_s = sl_copy_append (end_s, "\n#pragma pack(pop)\n");
	}

	//FIXME: Should there be a check that the name is already in use here?
	name_stack_push (& ctx->globals, structName->token, new_syntax_tree (make_str_token ("struct"), NULL), nFields);

	/* Release function */
	end_s = sl_copy_append (end_s, "static inline void _lc_release");
	end_s = sl_copy_append_t (end_s, structName->token);
	end_s = sl_copy_append (end_s, "(");
	end_s = sl_splice (end_s, eval_type (ctx, structName, 0));
	end_s = sl_copy_append (end_s, "st){");
	end_s = sl_copy_append (end_s, "if(!--");
	end_s = sl_splice (end_s, referenceCount (make_str_token ("st")));
	end_s = sl_copy_append (end_s, "){");
	end_s = sl_splice (end_s, begin_r);
	end_s = sl_copy_append (end_s, "free(((unsigned*)st)-1);");
	end_s = sl_copy_append (end_s, "}}");

	ctx->lastExpressionType = NULL;

	return begin_s;
}

/*(def NAME struct ((NAME TYPE) ...))  struct*/
struct string_list *
eval_def_as_struct (struct context * const ctx, struct syntax_tree * pn)
{
	struct syntax_tree * nName   = st_at (pn->sub_tree, 1);
	struct syntax_tree * nFields = st_at (pn->sub_tree, 3);
	line (ctx, pn->token.line);
	return eval_def_struct_with_name (ctx, nName, nFields, NULL);
}

/*(def NAME TYPE VALUE)                 general*/
struct string_list *
eval_def (struct context * const ctx, struct syntax_tree * pn)
{
	assert (token_type_open == pn->token.type);
	assert (! t_strcmp (pn->sub_tree->next->token, "def"));
	if (4 != st_count (pn->sub_tree))
	{
		context_set_error (ctx, "Incorrect number of expressions in 'def'", pn);
		return NULL;
	}

//	struct syntax_tree * nDefName = st_at (pn->sub_tree, 1);
	struct syntax_tree * nType    = st_at (pn->sub_tree, 2);
//	struct syntax_tree * nValue   = st_at (pn->sub_tree, 3);
	/* Handle unbound definition case. */

	if (is_type_primative (nType) || is_type_pointer (nType))
	{
		return eval_def_as_variable (ctx, pn);
	}
	else if (! t_strcmp (nType->token, "struct"))
	{
		return eval_def_as_struct (ctx, pn);
	}
	else if (token_type_open == nType->token.type)
	{
		/*it's a function!*/
		struct string_list * const ret = eval_def_as_function (ctx, pn);
		ctx->lastExpressionType = NULL;
		return ret;
	}
	else
	{
		context_set_error (ctx, "'def' type not recognised", pn);
		return NULL;
	}
}
