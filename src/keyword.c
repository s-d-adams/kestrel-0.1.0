/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>

#include "eval.h"
#include "keyword.h"

struct keyword_evaluator
{
	char const * name;
	func_eval eval;
};

static
struct keyword_evaluator all_keywords[] =
{
	{"addr",   eval_addr},
	{"assign", eval_assign},
	{"call",   eval_call},
	{"cond",   eval_cond},
	{"dec",    eval_dec},
	{"def",    eval_def},
	{"deref",  eval_deref},
	{"do",     eval_do},
	{"doEach", eval_map},
	{"field",  eval_field},
	{"inc",    eval_inc},
	{"lam",    eval_lam},
	{"let",    eval_let},
	{"map",    eval_map},
	{"name",   eval_name},
	{"use",    eval_use},
	{NULL, NULL}
};

func_eval
eval_for_keyword (struct token name)
{
	struct keyword_evaluator * it = all_keywords;
	for (; it->name; ++ it)
	{
		if (! t_strcmp (name, it->name))
		{
			return it->eval;
		}
	}
	return NULL;
}

struct string_list *
eval_keyword (struct context * const ctx, struct syntax_tree * const pn)
{
	if (token_type_open == pn->token.type)
	{
		if (st_count (pn->sub_tree) == 0)
		{
			context_set_error (ctx, "Attempted to eval an empty s-expression.", pn);
			return NULL;
		}
		struct syntax_tree * first = pn->sub_tree->next;
		if (token_type_name != first->token.type)
		{
			context_set_error (ctx, "First symbol in s-expression not a name.", pn);
			return NULL;
		}
		func_eval keyword_eval_func = eval_for_keyword (first->token);
		if (! keyword_eval_func)
		{
			context_set_error (ctx, "Name not recognised.", pn);
			return NULL;
		}
		return keyword_eval_func (ctx, pn);
	}
	else if (token_type_string == pn->token.type)
	{
//FIXME: The original implementation just returned the node without recording anything, which seems wrong.
//		return pn;
		return sl_copy_t (pn->token);
	}
	else
	{
		context_set_error (ctx, "Unexpected symbol type.", pn);
		return NULL;
	}
}

struct string_list *
releaseObject (struct context * const ctx, struct token const name, struct syntax_tree * type)
{
	if (is_type_mutable (type))
	{
//FIXME: This might be wrong.  It might need some const-casting logic.
		return releaseObject (ctx, name, st_at (type->sub_tree, 1));
	}

	assert (is_type_reference_counted (ctx, type));

	struct string_list * const begin = new_string_list (NULL, 0, 0);
	struct string_list * end = begin;
	if (is_type_array (type))
	{
		struct syntax_tree * subType = st_at (type->sub_tree, 1);
		int const subIsArray = is_type_array (subType);
		int const subIsStruct = is_type_struct_name (ctx, subType);
		if (subIsArray || subIsStruct)
		{
			end = sl_copy_append (end, "if(1==");
			end = sl_splice (end, referenceCount (name));
			end = sl_copy_append (end, "){unsigned _lc_i,_lc_s;for(_lc_i=0,_lc_s=");
			end = sl_splice (end, arraySize (name));
			end = sl_copy_append (end, ";_lc_i<_lc_s;++_lc_i){");
			end = sl_splice (end, releaseObject (ctx, t_append (name, "[_lc_i]"), subType));
			end = sl_copy_append (end, "}}");
			end = sl_splice (end, releaseArray (name));
			return begin;
		}
		else
		{
			return releaseArray (name);
		}
	}
	else
	{
		if (is_type_struct_name (ctx, type))
		{
			end = sl_copy_append (end, "_lc_release");
			end = sl_copy_append_t (end, type->token);
			end = sl_copy_append (end, "(");
			end = sl_copy_append_t (end, name);
			end = sl_copy_append (end, ");");
			return begin;
		}
		else if (token_type_open == type->token.type)
		{
			/* Assume a function object */
			end = sl_copy_append (end, "(((void(**)(void*))");
			end = sl_copy_append_t (end, name);
			end = sl_copy_append (end, ")[1])(");
			end = sl_copy_append_t (end, name);
			end = sl_copy_append (end, ");");
			return begin;
		}
		else
		{
//FIXME: Need to handle usages of this to provide better compile errors.
			context_set_error (ctx, "Type name not known", type);
			return NULL;
		}
	}
}

static
struct string_list *
eval_actual (struct context * const ctx, struct syntax_tree * pn)
{
	if (token_type_string == pn->token.type)
	{
		line (ctx, pn->token.line);
		return sl_copy_t (pn->token);
	}
	else if (token_type_name == pn->token.type)
	{
		/*Allow lc keywords to dominate*/
		if (! eval_for_keyword (pn->token))
		{
			return eval_name (ctx, pn);
		}
	}
	else if (token_type_open == pn->token.type)
	{
		if (st_count (pn->sub_tree) == 0)
		{
			context_set_error (ctx, "Unexpected empty expression", pn);
			return NULL;
		}
		else if ((3 == st_count (pn->sub_tree)) && is_binop (pn->sub_tree->next->token))
		{
			struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
			end = begin;
			line (ctx, pn->token.line);
			end = sl_copy_append (end, "(");
			end = sl_splice (end, eval(ctx, st_at (pn->sub_tree, 1)));
			if (ctx->has_error)
			{
				return NULL;
			}
			struct syntax_tree * type1 = ctx->lastExpressionType;
			if (! is_type_primative(type1))
			{
				context_set_error (ctx, "First argument to operator not of primative type", st_at (pn->sub_tree, 2));
				return NULL;
			}
			line (ctx, st_at (pn->sub_tree, 1)->token.line);
			end = sl_copy_append_t (end, pn->sub_tree->next->token);
			end = sl_splice (end, eval (ctx, st_at (pn->sub_tree, 2)));
			if (ctx->has_error)
			{
				return NULL;
			}
			struct syntax_tree * type2 = ctx->lastExpressionType;
			if (! is_type_primative(type2))
			{
				context_set_error (ctx, "Second argument to operator not of primative type", st_at (pn->sub_tree, 2));
				return NULL;
			}
			end = sl_copy_append (end, ")");

			char * t;
			if (is_boolean_binop (pn->sub_tree->next->token))
			{
				t = strdup ("int");
			}
			else
			{
				char const * _t = precedent_type (type1->token, type2->token);
				if (! _t)
				{
					context_set_error (ctx, "Cannot determine type precedence.", pn);
					return NULL;
				}
				t = strdup (_t);
			}
			ctx->lastExpressionType = new_syntax_tree (make_token (token_type_name, t, strlen (t), 0, 0, 0), NULL);
			return begin;
		}
		else if ((2 == st_count (pn->sub_tree)) && is_unop (pn->sub_tree->next->token))
		{
			struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
			end = begin;
			line (ctx, pn->token.line);
			end = sl_copy_append (end, "(");
			line (ctx, pn->sub_tree->next->token.line);
			end = sl_copy_append_t (end, pn->sub_tree->next->token);
			end = sl_splice (end, eval (ctx, st_at (pn->sub_tree, 1)));
			if (ctx->has_error)
			{
				return NULL;
			}
			struct syntax_tree * type = ctx->lastExpressionType;
			if (! is_type_primative (type))
			{
				context_set_error (ctx, "Argument to unary operator not of primative type", st_at (pn->sub_tree, 1));
				return NULL;
			}
			end = sl_copy_append (end, ")");

			struct token const t = is_boolean_unop (pn->sub_tree->next->token) ? make_str_token ("int") : type->token;
			ctx->lastExpressionType = new_syntax_tree (t, NULL);
			return begin;
		}
		else if (pn->sub_tree && pn->sub_tree->next)
		{
			if (is_type_struct_name (ctx, pn->sub_tree->next))
			{
				return eval_construct (ctx, pn);
			}
			else if (is_type_array (pn->sub_tree->next))
			{
				return eval_construct_array (ctx, pn);
			}
			else
			{
				struct syntax_tree * const name_in_scope = context_findNameInScope (ctx, pn->sub_tree->next->token);
				if (name_in_scope && is_type_struct_name (ctx, name_in_scope->sub_tree))
				{
					return eval_field (ctx, pn);
				}
				else if (! eval_for_keyword (pn->sub_tree->next->token))
				{
					return eval_call (ctx, pn);
				}
			}
		}
	}

	return eval_keyword (ctx, pn);
}

struct string_list *
eval (struct context * const ctx, struct syntax_tree * pn)
{
	int_stack_push (& ctx->expressionIsUnknownFunctionCall, 0);
	struct string_list * result = eval_actual (ctx, pn);
	int_stack_pop (& ctx->expressionIsUnknownFunctionCall);
	return result;
}

struct string_list *
referenceCount(struct token const objName)
{
	struct string_list * const begin = sl_copy_str ("(((unsigned*)"), * end;
	end = begin;
	end = sl_copy_append_t (end, objName);
	end = sl_copy_append (end, ")[-1])");
	return begin;
}

struct string_list *
retainObject(struct token const objName)
{
	struct string_list * const begin = sl_copy_str ("++"), * end;
	end = begin;
	end = sl_splice (end, referenceCount (objName));
	end = sl_copy_append (end, ";");
	return begin;
}

struct string_list *
releaseArray(struct token const arrayName)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	end = sl_copy_append (end, "if(!--(((unsigned*)");
	end = sl_copy_append_t (end, arrayName);
	end = sl_copy_append (end, ")[-1]))free(((unsigned*)");
	end = sl_copy_append_t (end, arrayName);
	end = sl_copy_append (end, ")-2);");
	return begin;
}

struct string_list *
arraySize (struct token const arrayName)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	end = sl_copy_append (end, "(((unsigned*)");
	end = sl_copy_append_t (end, arrayName);
	end = sl_copy_append (end, ")[-2])");
	return begin;
}

struct string_list *
releaseObject (struct context * const ctx, struct token const name, struct syntax_tree * type);

struct string_list *
functionToCExpr (struct context * const ctx, struct syntax_tree * nType, char const * const name, char const * const contextType)
{
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	end = sl_splice (end, eval_type (ctx, st_last_non_close (nType->sub_tree), 1));
	end = sl_copy_append (end, "(*");
	if (name)
	{
		end = sl_copy_append (end, name);
	}
	end = sl_copy_append (end, ")(");
	if (contextType)
	{
		end = sl_copy_append (end, contextType);
		end = sl_copy_append (end, "*");
	}
	int const impure = ! t_strcmp (nType->sub_tree->next->token, "imp");
	size_t const count = st_count (nType->sub_tree) - 1;
	for (size_t i = (impure ? 1 : 0); i < count; ++ i)
	{
		if (contextType || i > (impure ? 1 : 0))
		{
			end = sl_copy_append (end, ",");
		}
		end = sl_splice (end, eval_type (ctx, st_at (st_at (nType->sub_tree, i)->sub_tree, 1), 0));
	}
	end = sl_copy_append (end, ")");
	return begin;
}
