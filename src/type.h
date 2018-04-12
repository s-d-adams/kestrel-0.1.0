/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

struct context;

struct string_list *
eval_type (struct context * const ctx, struct syntax_tree * pn, int const as_mutable);

int is_type_void (struct syntax_tree *);
int is_type_primative (struct syntax_tree *);
int is_type_pointer (struct syntax_tree *);
int is_type_array (struct syntax_tree *);
int is_type_struct_name (struct context *, struct syntax_tree *);
int is_type_function (struct syntax_tree *);
int is_type_mutable (struct syntax_tree *);
int is_type_reference_counted (struct context *, struct syntax_tree *);
int is_type_null (struct syntax_tree *);

char const * type_of_literal (struct token);
char const * precedent_type (struct token type1, struct token type2);

int types_equal_modulo_mutability (struct syntax_tree * const a, struct syntax_tree * const b);

int is_unop (struct token str);
int is_boolean_unop (struct token str);
int is_binop (struct token str);
int is_boolean_binop (struct token str);

