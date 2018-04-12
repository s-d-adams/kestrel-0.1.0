/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include <stddef.h>

#include "context.h"

void
compiler_warn (struct context * const ctx, struct error_report const e);

struct syntax_tree *
compiler_parse_module (struct context * const ctx, char const * const moduleName, int *isHeader, char ** fileContent, char const ** error_message);

char *
compiler_file_path_of_module_with_extension (struct context * const ctx, char const * const moduleName, char const * const extension);

char const *
compiler_compile (struct context * const ctx);
