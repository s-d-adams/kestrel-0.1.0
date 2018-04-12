/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "compiler.h"
#include "eval.h"
#include "parse.h"
#include "keyword.h"

void
compiler_line (struct context * const ctx, const unsigned l)
{
	(void) l;
	if (ctx->conf.debug)
	{
		char * buf;
		asprintf (& buf, "\n#line %u\n", l);
//FIXME:
//		rec (buf);
		free (buf);
	}
}

char const *
compiler_find_filename_for_module (struct context * const ctx, char const * const module)
{
	return sl_find (ctx->moduleToFileNameMap, module, NULL);
}

char *
compiler_file_path_of_module_with_extension (struct context * const ctx, char const * const moduleName, char const * const extension)
{
	char const * const originalFilePath = compiler_find_filename_for_module (ctx, moduleName);

	size_t const outputFilePathSize = strlen (originalFilePath);
	char * filePath = (char *) calloc (outputFilePathSize + strlen (extension) - 1, sizeof (char));
	memcpy (filePath, originalFilePath, outputFilePathSize - 2);
	memcpy (filePath + (outputFilePathSize - 2), extension, strlen (extension));
	return filePath;
}

static
size_t
compiler_result (struct context * const ctx, char ** const dest_ptr, struct string_list * const sl)
{
	if (0)
//	if (ctx->conf.debug)
	{
		char * currentFilePath = compiler_file_path_of_module_with_extension (ctx, ctx->currentModuleName, "lc");
		struct string_list * flb = sl_copy_str ("#line 1 \""), * fle;
		fle = flb;
		fle = sl_copy_append (fle, currentFilePath);
		fle = sl_copy_append (fle, "\"\n");
		fle = sl_splice (fle, sl);
		size_t rc = sl_fold (dest_ptr, flb);
		fle->next = NULL;
		sl->prev = NULL;
		del_string_list (flb);
		free (currentFilePath);
		return rc;
	}
	else
	{
		if (ctx->conf.tidy)
		{
			struct string_list * b, * e, * it;
			b = new_string_list (NULL, 0, 0);
			e = b;
			it = sl;
			for (; it; it = it->next)
			{
				e = sl_copy_append_strref (e, it->string, it->size);
				if (it->size == 1 && * it->string == ';')
				{
					e = sl_copy_append (e, "\n");
				}
			}
			size_t const s = sl_fold (dest_ptr, b);
			del_string_list (b);
			return s;
		}
		else
		{
			return sl_fold (dest_ptr, sl);
		}
	}
}

void compiler_handle_exception(
	char const * const type,
	struct error_report const e,
	char const * const fileName,
	char const * fileContent)
{
	size_t lineStart = 0, lineEnd = 0;
	unsigned lineCounter = 1;
	for (; lineCounter != e.node->token.line; ++lineEnd)
	{
		char const c = fileContent[lineEnd];
		if (c == '\n')
			++lineCounter;
		if (c == '\0')
			break;
		
	}
	for (lineEnd = lineStart; ; ++lineEnd) {
		char const c = fileContent[lineEnd];
		if (c == '\n' || c == '\0')
			break;
	}

	if (e.node->token.column > 80)
	{
		lineStart += (e.node->token.column - 40);
		lineEnd   =  (lineStart + 80);
	}

	if (lineEnd > (lineStart + 80))
	{
		lineEnd = lineStart + 80;
	}

	dprintf (STDERR_FILENO, "%s error : %s\n%s:%lu:%lu\n%.*s\n", type, e.message, fileName, e.node->token.line, e.node->token.column, (int) (lineEnd - lineStart), fileContent + lineStart);
	size_t i;
	for (i = 0; i < (e.node->token.column - lineStart); ++ i)
	{
		dprintf (STDERR_FILENO, "-");
	}
	dprintf (STDERR_FILENO, "^\n");

	exit(1);
}

void
compiler_warn (struct context * const ctx, struct error_report const e)
{
//FIXME: Add context printing, use compiler_handle_exception as a guide.
	(void) ctx;
	dprintf (STDERR_FILENO, "Warning : %s\n:%lu:%lu\n",  e.message, e.node->token.line, e.node->token.column);
}

char *
compiler_findPathOfFileInPaths (char const * const fileName, struct string_list * paths, char const ** error_message)
{
	struct string_list * it = paths->next;
	for (; it; it = it->next)
	{
		char * path = it->string;
		struct dirent * entry;
		DIR * dir = opendir (path);
		if (!dir)
		{
			* error_message = "Couldn't open include directory.";
			return NULL;
		}
		while ((entry = readdir(dir)))
		{
			char const * const entryFileName = entry->d_name;
			if (! strcmp (fileName, entryFileName))
			{
				closedir(dir);
				return strdup (path);
			}
		}
		closedir(dir);
	}
	return NULL;
}

static
struct syntax_tree *
compiler_find_syntax_tree_for_module (struct context * const ctx, char const * module, size_t const * const len)
{
	struct syntax_tree * it = ctx->moduleToParsedMap->next;
	for (; it; it = it->next)
	{
		size_t const keylen = len ? * len : strlen (module);
		if (it->token.size != keylen)
		{
			continue;
		}
		if (! strncmp (module, it->token.bytes, keylen))
		{
			return it->sub_tree;
		}
	}
	return NULL;
	
}

struct syntax_tree *
compiler_parse_module (struct context * const ctx, char const * const _moduleName, int *isHeader, char ** fileContent, char const ** error_message)
{
	char * moduleName = strdup (_moduleName);

	struct syntax_tree * const cached = compiler_find_syntax_tree_for_module (ctx, moduleName, NULL);
	if (cached)
	{
		return cached;
	}

	char const * const tmp_fnformod = compiler_find_filename_for_module (ctx, moduleName);
	char * moduleFilePath = tmp_fnformod ? strdup (tmp_fnformod) : NULL;

	int const isLcFile = (moduleFilePath != NULL);

	/* If module not part of the compile job then check include paths. */
	if (isLcFile)
	{
		if (strcmp (moduleFilePath + (strlen (moduleFilePath) - 3), ".lc"))
		{
			free (moduleFilePath);
			* error_message = "Input file doesn't end in '.lc'.";
			return NULL;
		}
	}
	else
	{
		char const * error_message = NULL;
		char * headerName = NULL;
		asprintf (& headerName, "%s.lh", moduleName);
		char * path = compiler_findPathOfFileInPaths (headerName, ctx->conf.includePaths, & error_message);
		if (path && strlen (path))
		{
			asprintf (& moduleFilePath, "%s%s%s", path, "/", headerName);
		}
		free (headerName);
		if (path)
		{
			free (path);
		}
	}
	if (! moduleFilePath || strlen (moduleFilePath) == 0)
	{
		* error_message = "Module name not known.";
		if (moduleFilePath)
		{
			free (moduleFilePath);
		}
		return NULL;
	}
	if (!isLcFile && isHeader)
	{
		*isHeader = 1;
	}

	char const * inFilePath = moduleFilePath;
	struct stat st;
	if (stat(inFilePath, &st))
	{
		* error_message = "Filesystem error.";
		free (moduleFilePath);
		return NULL;
	}
	char * buf = (char*)calloc(st.st_size + 1, 1);
	FILE * infile = fopen(inFilePath, "r");
	fread(buf, 1, st.st_size, infile);
	fclose(infile);
	free (moduleFilePath);

	struct token_list * tokens = lex (buf, st.st_size);
	struct syntax_tree * ast = parse (tokens);

	if (fileContent)
	{
		*fileContent = buf;
	}
	else
	{
		free(buf);
	}

	struct syntax_tree * it = ctx->moduleToParsedMap;
	for (; it->next; it = it->next);
	it->next = new_syntax_tree (make_str_token (moduleName), NULL);
	it->next->sub_tree = ast;
	it->next->prev = it;
	return ast;
}

void
compiler_write_module_to_file (struct context * const ctx, char const * const moduleName, char const * const fileExtension, char const * const content, size_t const size)
{
	char * outFileName = compiler_file_path_of_module_with_extension (ctx, moduleName, fileExtension);
	FILE * outputFile = fopen (outFileName, "w");
	fwrite (content, size, 1, outputFile);
	fclose (outputFile);
	free (outFileName);
}

static
char *
compiler_filePathToModuleName (char const * const filePath)
{
	size_t const len = strlen (filePath);
	char const * begin;
	char const * end;
	char const * it = filePath + len;
	for (; it >= filePath && * it != '/'; -- it);
	begin = it + 1;
	it = filePath + len;
	for (; it > begin && * it != '.'; -- it);
	if (it <= begin)
	{
		end = filePath + len;
	}
	else
	{
		end = it;
	}
	return strndup (begin, end - begin);
}

char const *
compiler_compile (struct context * const ctx)
{
	/* Convert file paths to module names. */
	struct string_list * it = ctx->conf.inFilePaths->next;
	for (; it; it = it->next)
	{
		char * moduleName = compiler_filePathToModuleName (it->string);
		if (compiler_find_filename_for_module (ctx, moduleName))
		{
			free (moduleName);
			return "Module name listed twice";
		}

		struct string_list * end = ctx->moduleToFileNameMap;
		for (; end->next; end = end->next);
		end = sl_copy_append (end, moduleName);
		end = sl_copy_append (end, it->string);
		free (moduleName);
	}

	it = ctx->moduleToFileNameMap->next;
	for (; it; it = it->next->next)
	{
		assert (it->next);
		char const * const moduleName = it->string;

		/* Read lc file, lex, and parse. */
		int isHeader;
		char * fileContent;
		char const * error_message = NULL;
		compiler_parse_module (ctx, moduleName,  & isHeader, & fileContent, & error_message);
		if (error_message)
		{
			return error_message;
		}

		/* Evaluate parsed lc file. */
		struct string_list * const begin_m = new_string_list (NULL, 0, 0), * end_m;
		end_m = begin_m;
		struct syntax_tree * lcCode = compiler_find_syntax_tree_for_module (ctx, moduleName, NULL);
		if (ctx->conf.boundsChecking)
		{
			end_m = sl_copy_append (end_m, "#include <assert.h>\n");
		}

		//FIXME: Make the following optimisation portable or remove it.
//		end_m = sl_copy_append (end_m, "#include <stdlib.h>\n");
		end_m = sl_copy_append (end_m, "typedef __SIZE_TYPE__ size_t;void*malloc(size_t s);void free(void*p);\n");

		ctx->currentModuleName = strdup (moduleName);
		{
			struct syntax_tree * it = lcCode->next;
			for (; it; it = it->next)
			{
				if (it->token.type == token_type_break)
				{
					continue;
				}
				end_m = sl_splice (end_m, eval (ctx, it));
				if (ctx->has_error)
				{
					char * fn = NULL;
					asprintf (& fn, "%s%s", moduleName, (isHeader ? ".lh" : ".lc"));
					compiler_handle_exception ("Compile", ctx->error_report, fn, fileContent);
					free (fn);
				}
			}
		}

		free (fileContent);

		/* Write evaluated lc code to C++ file. */
		char * result_m;
		size_t const size_m = compiler_result (ctx, & result_m, begin_m);
		compiler_write_module_to_file (ctx, moduleName, "c", result_m, size_m);
		context_clear (ctx);

		/* Compile C file to object file. */
		if (! ctx->conf.noCppCompile)
		{
			char * cFile = compiler_file_path_of_module_with_extension (ctx, moduleName, "c");
			char * cmd = NULL;
			asprintf (& cmd, "gcc -Werror -std=gnu99%s%s -c %s", ctx->conf.debug ?" -g -O0" : "", (output_type_executable != ctx->conf.output_type) ? " -fPIC" : "", cFile);
			free (cFile);
			//FIXME: Add c compiler output parsing and relate it back to the original file, line, and column.
			printf ("%s\n", cmd);
			if (system (cmd))
			{
				free (cmd);
				return "gcc failure creating object file.";
			}
			free (cmd);
		}

		if (output_type_executable != ctx->conf.output_type && ctx->conf.createHeaders)
		{
			struct syntax_tree * it = lcCode;
			struct string_list * begin = new_string_list (NULL, 0, 0), * end;
			end = begin;
			for (; it; it = it->next)
			{
				end = sl_splice (end, eval_as_header (ctx, it));
			}
			char * result = NULL;
			size_t const size = sl_fold (& result, begin);
			del_string_list (begin);
			compiler_write_module_to_file (ctx, moduleName, "lh", result, size);
		}
	}

	/* Create result from intermediate files. */
	if (! ctx->conf.noCppCompile)
	{
		struct string_list * sl = new_string_list (NULL, 0, 0);
		if (output_type_static_library == ctx->conf.output_type)
		{
			sl = sl_copy_append (sl, "ar -rs lib");
			sl = sl_copy_append (sl, ctx->conf.outFilepath);
			sl = sl_copy_append (sl, ".a");
		}
		else
		{
			sl = sl_copy_append (sl, "gcc -Werror -std=gnu99");
			if (ctx->conf.debug)
			{
				sl = sl_copy_append (sl, " -g");
			}
			if (output_type_dynamic_library == ctx->conf.output_type)
			{
				sl = sl_copy_append (sl, " -shared -o lib");
				sl = sl_copy_append (sl, ctx->conf.outFilepath);
				sl = sl_copy_append (sl, ".so");
			}
			else
			{
				sl = sl_copy_append (sl, " -o ");
				sl = sl_copy_append (sl, ctx->conf.outFilepath);
			}
			if (ctx->conf.staticLinkage)
			{
				sl = sl_copy_append (sl, " -static");
			}
		}
		struct string_list * it = ctx->conf.inFilePaths->next;
		for (; it; it = it->next)
		{
			char * oFilePath = strdup (it->string);
			size_t const oFilePathSize = strlen (oFilePath);
			oFilePath[oFilePathSize - 2] = 'o';
			oFilePath[oFilePathSize - 1] = '\0';
			sl = sl_copy_append (sl, " ");
			sl = sl_copy_append (sl, oFilePath);
			free (oFilePath);
		}
		if (output_type_static_library != ctx->conf.output_type)
		{
			struct string_list * it = ctx->conf.libraries->next;
			for (; it; it = it->next)
			{
				char * libName = it->string;
				char * libFileName;
				asprintf (& libFileName, "lib%s%s", libName, (ctx->conf.staticLinkage ? ".a" : ".so"));
				char const * error_message = NULL;
				char * libPath = compiler_findPathOfFileInPaths (libFileName, ctx->conf.libraryPaths, & error_message);
				free (libFileName);
				if (error_message)
				{
					if (libPath)
					{
						free (libPath);
					}
					return error_message;
				}
				if (libPath)
				{
					sl = sl_copy_append (sl, " -L ");
					sl = sl_copy_append (sl, libPath);
					free (libPath);
				}
				sl = sl_copy_append (sl, " -l");
				sl = sl_copy_append (sl, libName);
			}
		}
		for (; sl->prev; sl = sl->prev);
		char * cmd;
		sl_fold (& cmd, sl);
		del_string_list (sl);
		printf ("%s\n", cmd);
		if (system (cmd))
		{
			free (cmd);
			return "Failure creating binary.";
		}
		free (cmd);
	}
	return NULL;
}
