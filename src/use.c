/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <stdlib.h>

#include "eval.h"
#include "compiler.h"

struct string_list *
eval_use_module (struct context * const ctx, struct token const _moduleName)
{
	char * moduleName = (char *) calloc (_moduleName.size + 1, sizeof (char));
	memcpy (moduleName, _moduleName.bytes, _moduleName.size);
	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;

	/* If not already cached then load the file, parse, and eval. */
	char const * const cached = sl_find (ctx->use_cache, _moduleName.bytes, & _moduleName.size);
	if (! cached)
	{
		struct string_list * const begin_c = new_string_list (NULL, 0, 0), * end_c;
		end_c = begin_c;

		int isHeader;
		char const * error_message = NULL;
		struct syntax_tree * parsedFile = compiler_parse_module (ctx, moduleName, & isHeader, NULL, & error_message);
		if (error_message)
		{
			context_set_error (ctx, error_message, NULL);
			del_string_list (begin_c);
			del_string_list (begin);
			free (moduleName);
			return NULL;
		}
		char * filePathForUse = compiler_file_path_of_module_with_extension (ctx, moduleName, (isHeader ? "kh" : "k"));
		free (moduleName);
		line_with_file (ctx, 1, filePathForUse);
		free (filePathForUse);
		struct syntax_tree * it = parsedFile->next;
		for (; it; it = it->next)
		{
			struct syntax_tree * nRoot = it;
			if (isHeader)
			{
				end_c = sl_splice (end_c, eval (ctx, nRoot));
				if (ctx->has_error)
				{
					return NULL;
				}
			}
			else
			{
				if (st_count (nRoot->sub_tree) > 0
					&& token_type_name == st_at (nRoot->sub_tree, 0)->token.type
					&& ! t_strcmp (st_at (nRoot->sub_tree, 0)->token, "def"))
				{
					end_c = sl_splice (end_c, eval_def_as_dec (ctx, nRoot));
				}
			}
		}

		char * use_result = NULL;
		sl_fold (& use_result, begin_c);

		struct string_list * uend = ctx->use_cache;
		for (; uend->next; uend = uend->next);

		uend = sl_copy_append_t (uend, _moduleName);
		uend = sl_copy_append (uend, use_result);

		end = sl_copy_append (end, use_result);
		free (use_result);
	}
	else
	{
		end = sl_copy_append (end, cached);
	}
	ctx->lastExpressionType = NULL;
	return begin;
}

struct string_list *
eval_use (struct context * const ctx, struct syntax_tree * pn)
{
	if (2 != st_count (pn->sub_tree))
	{
		context_set_error (ctx, "Incorrect number of expressions passed to 'use'", pn);
		return NULL;
	}
	struct syntax_tree * nName = st_at (pn->sub_tree, 1);
	if (token_type_name != nName->token.type)
	{
		context_set_error (ctx, "'use' expects a module name", nName);
		return NULL;
	}

	struct string_list * const begin = new_string_list (NULL, 0, 0), * end;
	end = begin;
	end = sl_splice (end, eval_use_module (ctx, nName->token));
	char * currentFilePath = compiler_file_path_of_module_with_extension (ctx, ctx->currentModuleName, "k");
	line_with_file (ctx, pn->token.line, currentFilePath);
	free (currentFilePath);

	return begin;
}
