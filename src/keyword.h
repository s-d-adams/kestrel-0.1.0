/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include "context.h"
#include "type.h"

#include "string_list.h"

struct string_list *
referenceCount(struct token const objName);

struct string_list *
retainObject(struct token const objName);

struct string_list *
releaseArray(struct token const arrayName);

struct string_list *
arraySize (struct token const arrayName);

struct string_list *
releaseObject (struct context * const ctx, struct token const name, struct syntax_tree * type);

struct string_list *
functionToCExpr (struct context * const ctx, struct syntax_tree * nType, char const * const name, char const * const contextType);
