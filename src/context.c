/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "context.h"
#include "eval.h"

void
int_stack_push (struct int_stack ** const stack, int const i)
{
	struct int_stack * next = (struct int_stack *) malloc (sizeof (struct int_stack));
	next->prev = * stack;
	next->i = i;
	* stack = next;
}

void
int_stack_pop (struct int_stack ** const stack)
{
	assert ((* stack)->prev);
	struct int_stack * prev = (* stack)->prev;
	free (* stack);
	* stack = prev;
}

void
context_init (struct context * const ctx, struct conf const conf)
{
	ctx->conf = conf;
	ctx->lastExpressionType = NULL;
	ctx->lambdaCounter = 0;
	ctx->lambdaDepth = 0;
	ctx->isDefAsDecMode = 0;
	ctx->has_error = 0;
	ctx->insidePureFunction = NULL;
	int_stack_push (& ctx->insidePureFunction, 0);
	ctx->impureFunctionCalled = NULL;
	int_stack_push (& ctx->impureFunctionCalled, 0);
	ctx->expressionIsUnknownFunctionCall = NULL;
	int_stack_push (& ctx->expressionIsUnknownFunctionCall, 0);
	ctx->moduleToFileNameMap = new_string_list (NULL, 0, 0);
	ctx->use_cache = new_string_list (NULL, 0, 0);

	ctx->lambdaTypeStack = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	ctx->lambdaTypeStack->sub_tree = NULL;

	ctx->namesClosedOver = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	ctx->namesClosedOver->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);

	ctx->lambdas = new_string_list (NULL, 0, 0);

	ctx->moduleToParsedMap = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);

	ctx->globals = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);

	ctx->stack = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	ctx->stack->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);

	ctx->unknown_type = new_syntax_tree (make_str_token ("_lc_unknown_type"), NULL);

	ctx->temporary_counter = 0;
}

void
context_clear (struct context * const ctx)
{
	while (ctx->stack->prev)
	{
		name_stack_stack_pop (& ctx->stack);
	}
	name_stack_clear (& ctx->stack->sub_tree);
	name_stack_clear (& ctx->globals);
	ctx->lastExpressionType = NULL;
	if (ctx->currentModuleName)
	{
		free (ctx->currentModuleName);
		ctx->currentModuleName = NULL;
	}
	assert (ctx->lambdas && ! ctx->lambdas->string);
	ctx->lambdaCounter = 0;
	ctx->temporary_counter = 0;
}

void
del_context (struct context * const ctx)
{
	context_clear (ctx);
	del_syntax_tree (ctx->unknown_type);
	del_syntax_tree (ctx->stack);
	del_syntax_tree (ctx->globals);
	del_syntax_tree (ctx->moduleToParsedMap);
	del_string_list (ctx->lambdas);
	del_syntax_tree (ctx->namesClosedOver);
	del_syntax_tree (ctx->lambdaTypeStack);
	del_string_list (ctx->use_cache);
	del_string_list (ctx->moduleToFileNameMap);
	assert (! ctx->impureFunctionCalled->prev);
	free (ctx->impureFunctionCalled);
	assert (! ctx->insidePureFunction->prev);
	free (ctx->insidePureFunction);
	assert (! ctx->expressionIsUnknownFunctionCall->prev);
	free (ctx->expressionIsUnknownFunctionCall);
}

static
struct syntax_tree *
context_findNameFromList (struct token const name, struct syntax_tree * names)
{
	struct syntax_tree * it = names->prev;
	for (; it; it = it->prev->prev)
	{
		assert (it->next);
		if (token_equal (name, it->token))
		{
			return it;
		}
		assert (it->prev);
	}
	return NULL;
}

static
struct syntax_tree *
context_findNameFromStackOfLists (struct token const name, struct syntax_tree * names)
{
	return context_findNameFromList (name, names->sub_tree);
}

struct syntax_tree * context_findGlobal (struct context * const ctx, struct token const name)
{
	return context_findNameFromList (name, ctx->globals);
}

struct syntax_tree * context_findLocal (struct context * const ctx, struct token const name)
{
	return context_findNameFromStackOfLists (name, ctx->stack);
}

struct syntax_tree * context_findStruct (struct context * const ctx, struct token const name)
{
	struct syntax_tree * ctxStruct = context_findGlobal (ctx, name);
	if (ctxStruct && ! t_strcmp (ctxStruct->sub_tree->token, "struct"))
	{
		return ctxStruct;
	}
	return NULL;
}

struct syntax_tree * context_findNameInScope (struct context * const ctx, struct token const name)
{
	struct syntax_tree * local = context_findLocal (ctx, name);
	return (local ? local : context_findGlobal (ctx, name));
}

struct syntax_tree * context_findClosedOverName (struct context * const ctx, struct token const name)
{
	assert (ctx->stack->prev);
	return context_findNameFromList (name, ctx->stack->prev);
}

void
name_stack_push (struct syntax_tree ** name_stack, struct token const name, struct syntax_tree * const type, struct syntax_tree * const value)
{
	struct syntax_tree * it = * name_stack;
	it->next = new_syntax_tree (name, NULL);
	it->next->token.capacity = 0;
	it->next->prev = it;
	it->next->sub_tree = type;
	it->next->next = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	it->next->next->prev = it->next;
	it->next->next->sub_tree = value;
	(* name_stack) = it->next->next;
}

void
name_stack_pop (struct syntax_tree ** name_stack)
{
	struct syntax_tree * it = (* name_stack)->prev;
	if (it)
	{
		assert (it->prev);
		(* name_stack) = it->prev;
		it->prev->next = NULL;

		it->next->sub_tree = NULL;
		it->next->prev = NULL;
		del_syntax_tree (it->next);

		it->next = NULL;
		it->prev = NULL;
		it->sub_tree = NULL;
		del_syntax_tree (it);
	}
}

void
name_stack_clear (struct syntax_tree ** name_stack)
{
	while ((* name_stack)->prev)
	{
		name_stack_pop (name_stack);
	}
}

void
name_stack_stack_push (struct syntax_tree ** name_stack_stack)
{
	(* name_stack_stack)->next = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	(* name_stack_stack)->next->sub_tree = new_syntax_tree (make_token (token_type_none, NULL, 0, 0, 0, 0), NULL);
	(* name_stack_stack)->next->prev = * name_stack_stack;
	(* name_stack_stack) = (* name_stack_stack)->next;
}

void
name_stack_stack_pop (struct syntax_tree ** name_stack_stack)
{
	name_stack_clear (& (* name_stack_stack)->sub_tree);

	struct syntax_tree * it = (* name_stack_stack)->sub_tree;
	assert (! it->prev);
	assert (! it->sub_tree);
	del_syntax_tree (it);

	it = * name_stack_stack;
	* name_stack_stack = it->prev;
	(* name_stack_stack)->next = NULL;

	it->prev = NULL;
	assert (! it->next);
	it->sub_tree = NULL;
	del_syntax_tree (it);
}

void
line_with_file (struct context * const ctx, const unsigned l, char const * file)
{
	if (0)
//	if (ctx->conf.debug)
	{
		struct string_list * const begin = sl_copy_str ("\n#line "), * end;
		end = sl_copy_append_uint (begin, l);
		if (file)
		{
			end = sl_copy_append (end, " ");
			end = sl_copy_append (end, file);
		}
		end = sl_copy_append (end, "\n");
//			rec (begin);
//FIXME: fixme
	}
}

void
line (struct context * const ctx, const unsigned l)
{
	line_with_file (ctx, l, NULL);
}

void
context_set_error (struct context * const ctx, char const * const message, struct syntax_tree * const node)
{
	if (ctx->has_error)
	{
		return;
	}
	ctx->has_error = 1;
	ctx->error_report.message = message;
	ctx->error_report.node = node;
}

struct token
context_make_temporary (struct context * const ctx)
{
	char * temporary_name = NULL;
	asprintf (& temporary_name, "_lc_tmp%lu", ctx->temporary_counter ++);

	size_t const size = strlen (temporary_name);
	struct token ret = make_token (token_type_name, temporary_name, size, 0, 0, 0);
	ret.capacity = size;

	return ret;
}

void
context_reset_temporary_names (struct context * const ctx)
{
	ctx->temporary_counter = 0;
}
