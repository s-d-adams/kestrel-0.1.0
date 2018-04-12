/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include <string.h>

#include "conf.h"
#include "string_list.h"
#include "parse.h"

struct context;

struct error_report
{
	char const * message;
	struct syntax_tree * node;
};

typedef struct string_list * (* func_eval) (struct context *, struct syntax_tree *);

int
fl_has_keyword_named (struct token name);

struct int_stack
{
	struct int_stack * prev;
	int i;
};

void
int_stack_push (struct int_stack ** const stack, int const i);

void
int_stack_pop (struct int_stack ** const stack);

struct context
{
	struct conf conf;

	// Root is blank, key/name is token, type is sub_tree, value is next->sub_tree.
	struct syntax_tree * globals;

	// A stack of elements each of the same layout as the globals tree.
	struct syntax_tree * stack;

	struct syntax_tree * unknown_type; // A location to point lastExpressionType to if the type is unknown.
	struct syntax_tree * lastExpressionType;
	struct int_stack * insidePureFunction;
	struct int_stack * impureFunctionCalled;
	struct int_stack * expressionIsUnknownFunctionCall;

	char * currentModuleName;
	struct string_list * lambdas; // This pointer follows the end of the queue.
	size_t lambdaCounter;
	size_t lambdaDepth;
	struct syntax_tree * namesClosedOver; // Stack of lists of unique tokens.
	struct syntax_tree * lambdaTypeStack; // Stack of dummy syntax_trees, sub_tree is a weak reference to the actual value.

	int isDefAsDecMode;

	// Stores the list of pairs in a flat string list, the first in the list is a blank root node.
	struct string_list * moduleToFileNameMap;

	// Stores the key in the token and the value in the sub_tree, first node is blank root.
	struct syntax_tree * moduleToParsedMap;

	// Maps module name to the evaluation result.  The first in the list is a blank root node.
	struct string_list * use_cache;

	int has_error;
	struct error_report error_report;

	size_t temporary_counter;
};

void
context_init (struct context * const ctx, struct conf const conf);

void
context_clear (struct context * const ctx);

void
del_context (struct context * const ctx);

struct syntax_tree * context_findGlobal (struct context * const ctx, struct token const name);
struct syntax_tree * context_findLocal (struct context * const ctx, struct token const name);
struct syntax_tree * context_findStruct (struct context * const ctx, struct token const name);
/* First searches the top of the stack, then the globals. */
struct syntax_tree * context_findNameInScope (struct context * const ctx, struct token const name);
struct syntax_tree * context_findClosedOverName (struct context * const ctx, struct token const name);

void
name_stack_push (struct syntax_tree ** name_stack, struct token const name, struct syntax_tree * const type, struct syntax_tree * const value);

void
name_stack_pop (struct syntax_tree ** name_stack);

void
name_stack_clear (struct syntax_tree ** name_stack);

void
name_stack_stack_push (struct syntax_tree ** name_stack_stack);

void
name_stack_stack_pop (struct syntax_tree ** name_stack_stack);

void
line (struct context * const ctx, const unsigned l);

void
line_with_file (struct context * const ctx, const unsigned l, char const * file);

void
context_set_error (struct context * const ctx, char const * const message, struct syntax_tree * const node);

struct token
context_make_temporary (struct context *);

void
context_reset_temporary_names (struct context *);
