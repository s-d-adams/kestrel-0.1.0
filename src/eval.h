/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include "parse.h"
#include "string_list.h"
#include "keyword.h"

struct string_list *
eval (struct context * const ctx, struct syntax_tree * pn);

/*(assign NAME EXPR)*/
struct string_list *
eval_assign (struct context * const ctx, struct syntax_tree * pn);

/*(addr NAME)*/
struct string_list *
eval_addr (struct context * const ctx, struct syntax_tree * pn);

struct string_list *
eval_as_header (struct context * const ctx, struct syntax_tree * pn);

struct string_list *
eval_call (struct context * const ctx, struct syntax_tree * pn);

/*(cond (COND EXPR) ... DEFAULT)*/
struct string_list *
eval_cond (struct context * const ctx, struct syntax_tree * pn);

/*(construct TYPE ((FIELD EXPR) ...))*/
struct string_list *
eval_construct (struct context * const ctx, struct syntax_tree * pn);

/*(construct_array TYPE (EXPR ...))*/
struct string_list *
eval_construct_array (struct context * const ctx, struct syntax_tree * pn);

/*(dec NAME TYPE)       general*/
/*(dec NAME (TYPE ...)) function*/
/*(dec NAME struct)     struct*/
struct string_list *
eval_dec (struct context * const ctx, struct syntax_tree * pn);

/*(def NAME TYPE VALUE)                 general*/
/*(def NAME (TYPE ...) (NAME ... EXPR)) function*/
/*(def NAME struct ((NAME TYPE) ...)))  struct*/
struct string_list *
eval_def (struct context * const ctx, struct syntax_tree * pn);

struct string_list *
eval_def_as_dec (struct context * const ctx, struct syntax_tree * pn);

struct string_list *
eval_def_struct_with_name (struct context * const ctx, struct syntax_tree * const structName, struct syntax_tree * nFields, char const * const lambdaFunction);

struct string_list *
eval_def_function_with_name (struct context * const ctx, struct token const functionName, struct syntax_tree *nTypes, struct syntax_tree *nExpr);

/*(deref NAME)*/
struct string_list *
eval_deref (struct context * const ctx, struct syntax_tree * pn);

/*(do IMPURE_EXPRESSION ... EXPRESSION)*/
struct string_list *
eval_do (struct context * const ctx, struct syntax_tree * pn);

/*(field STRUCT FIELD)*/
struct string_list *
eval_field (struct context * const ctx, struct syntax_tree * pn);

/*(inc "stdio.h")*/
struct string_list *
eval_inc (struct context * const ctx, struct syntax_tree * pn);

struct string_list *
eval_keyword (struct context * const ctx, struct syntax_tree * const pn);

/*(lam (TYPE ...) (NAME ... EXPR))*/
/*(lam (TYPE) EXPR)*/
struct string_list *
eval_lam (struct context * const ctx, struct syntax_tree * pn);

/*(let (NAME TYPE VALUE) EXPR)*/
/*(let ((NAME TYPE VALUE) ...) EXPR)*/
struct string_list *
eval_let (struct context * const ctx, struct syntax_tree * pn);

/*(map ARRAY FUNCTION)*/
struct string_list *
eval_map (struct context * const ctx, struct syntax_tree * pn);

struct string_list *
eval_name (struct context * const ctx, struct syntax_tree * pn);

/*(use BASENAME)*/
struct string_list *
eval_use (struct context * const ctx, struct syntax_tree * pn);
