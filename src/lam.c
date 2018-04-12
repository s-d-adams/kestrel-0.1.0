/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "compiler.h"

struct string_list *
eval_lam (struct context * const ctx, struct syntax_tree * pn)
{
	assert(token_type_open == pn->token.type);
	if (3 != st_count (pn->sub_tree))
	{
		context_set_error (ctx, "Incorrect number of expressions passed to 'lam'", pn);
		return NULL;
	}
	struct syntax_tree * n_args = st_at (pn->sub_tree, 1);
	struct syntax_tree * n_expr = st_at (pn->sub_tree, 2);

	ctx->namesClosedOver->next = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	ctx->namesClosedOver->next->prev = ctx->namesClosedOver;
	ctx->namesClosedOver = ctx->namesClosedOver->next;
	struct string_list * lambdaOutput;
	{
		char * lambdaName = NULL;
		asprintf (& lambdaName, "_lc_lam%lu", ctx->lambdaCounter);
		++ ctx->lambdaDepth;
		struct syntax_tree * t = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
		t->sub_tree = n_args;
		t->prev = ctx->lambdaTypeStack;
		ctx->lambdaTypeStack->next = t;
		ctx->lambdaTypeStack = t;
		lambdaOutput = eval_def_function_with_name (ctx, make_str_token (lambdaName), n_args, n_expr);
		if (ctx->has_error)
		{
			return NULL;
		}
		free (lambdaName);
		ctx->lambdaTypeStack = ctx->lambdaTypeStack->prev;
		ctx->lambdaTypeStack->next = NULL;
		t->prev = t->sub_tree = NULL;
		del_syntax_tree (t);
		-- ctx->lambdaDepth;
	}
	struct syntax_tree * namesClosedOver = ctx->namesClosedOver->sub_tree;
	ctx->namesClosedOver->sub_tree = NULL;
	ctx->namesClosedOver = ctx->namesClosedOver->prev;
	ctx->namesClosedOver->next->prev = NULL;
	del_syntax_tree (ctx->namesClosedOver->next);
	ctx->namesClosedOver->next = NULL;

	struct syntax_tree * nFields = new_syntax_tree (make_token (token_type_open, & open_brace_char, 1, 0, 0, 0), NULL);
	nFields->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	struct syntax_tree * it = namesClosedOver;
	for (; it; it = it->next)
	{
		struct token const fieldName = it->token;
		struct syntax_tree * nField = new_syntax_tree (make_token (token_type_open, & open_brace_char, 1, 0, 0, 0), nFields);
		nField->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
		st_add (st_last_non_close (nField->sub_tree), new_syntax_tree (fieldName, nField));
		struct syntax_tree * ctxField = context_findNameInScope (ctx, fieldName);
		assert(ctxField);
		st_add (st_last_non_close (nField->sub_tree), ctxField->sub_tree);
		st_add (st_last_non_close (nFields->sub_tree), nField);
	}
	st_add (st_last_non_close (nFields->sub_tree), new_syntax_tree (make_token (token_type_close, & close_brace_char, 1, 0, 0, 0), nFields));

	struct string_list * closureOutput;
	{
		char * contextName = NULL;
		asprintf (& contextName, "_lc_ctx%lu", ctx->lambdaCounter);
		char * contextType = NULL;
		asprintf (& contextType, "struct %s", contextName);
		struct string_list * const begin_l = new_string_list (NULL, 0, 0), * end_l;
		end_l = begin_l;
		end_l = sl_splice (end_l, functionToCExpr (ctx, n_args, "_lc_fn", contextType));
		end_l = sl_copy_append (end_l, ";");
		char * lambdaAsCDeclaration;
		sl_fold (& lambdaAsCDeclaration, begin_l);
		closureOutput = eval_def_struct_with_name (ctx, new_syntax_tree (make_str_token (contextName), NULL), nFields, lambdaAsCDeclaration);
		free (contextName);
		free (contextType);
		free (lambdaAsCDeclaration);
	}

	struct string_list * closureEnd = closureOutput;
	for (; closureEnd->next; closureEnd = closureEnd->next);
	closureEnd = sl_splice (closureEnd, lambdaOutput);

	ctx->lambdas = sl_splice (ctx->lambdas, closureOutput);

	char * lamC = sl_uint (ctx->lambdaCounter);

	struct token tmp = context_make_temporary (ctx);

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	end = sl_copy_append (end, "({struct _lc_ctx");
	end = sl_copy_append (end, lamC);
	end = sl_copy_append (end, "*");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "=(struct _lc_ctx");
	end = sl_copy_append (end, lamC);
	end = sl_copy_append (end, "*)");
	end = sl_copy_append (end, "malloc(sizeof(unsigned int)+sizeof(struct _lc_ctx");
	end = sl_copy_append (end, lamC);
	end = sl_copy_append (end, "));");
	end = sl_copy_append (end, "(*((unsigned*)");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "))=1;");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "=(struct _lc_ctx");
	end = sl_copy_append (end, lamC);
	end = sl_copy_append (end, "*)(((unsigned *)");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, ") + 1);");
	end = sl_copy_append (end, "");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "->_lc_fn=_lc_lam");
	end = sl_copy_append (end, lamC);
	end = sl_copy_append (end, ";");
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, "->_lc_rel=_lc_release_lc_ctx");
	end = sl_copy_append (end, lamC);
	end = sl_copy_append (end,  ";");
	free (lamC);
	for (it = namesClosedOver; it; it = it->next)
	{
		struct token const nameClosedOver = it->token;
		struct syntax_tree * ctxName = context_findNameInScope (ctx, nameClosedOver);
		if (! is_type_primative (ctxName->sub_tree))
		{
			end = sl_splice (end, retainObject (nameClosedOver));
		}
		end = sl_copy_append_t (end, tmp);
		end = sl_copy_append (end, "->");
		end = sl_copy_append_t (end, nameClosedOver);
		end = sl_copy_append (end, "=");
		end = sl_copy_append_t (end, nameClosedOver);
		end = sl_copy_append (end, ";");
	}
	end = sl_copy_append_t (end, tmp);
	end = sl_copy_append (end, ";})");

	free (tmp.bytes);
	del_syntax_tree (namesClosedOver);

	line (ctx, pn->token.line);
//	rec(ss.str());

	ctx->lastExpressionType = n_args;

	++ ctx->lambdaCounter;

	return begin;
}
