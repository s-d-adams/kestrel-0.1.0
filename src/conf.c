/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "conf.h"

struct conf
default_conf (void)
{
	struct conf c;
	c.noCppCompile = 0;
	c.outFilepath = strdup ("a.out");
	c.selfDebug = 0;
	c.debug = 0;
	c.boundsChecking = 0;
	c.tidy = 0;
	c.output_type = output_type_executable;
	c.createHeaders = 0;
	c.staticLinkage = 0;
	c.outFilepath = NULL;
	c.inFilePaths = new_string_list (NULL, 0, 0);
	c.includePaths = new_string_list (NULL, 0, 0);
	c.libraryPaths = new_string_list (NULL, 0, 0);
	c.libraries = new_string_list (NULL, 0, 0);
	return c;
}

void
del_conf (struct conf * const c)
{
	if (c->outFilepath)
	{
		free (c->outFilepath);
	}
	del_string_list (c->inFilePaths);
	del_string_list (c->includePaths);
	del_string_list (c->libraryPaths);
	del_string_list (c->libraries);
}

char const * config_read_command_line (struct conf * const conf, unsigned const argc, const char * const * const argv)
{
	if (argc < 2)
	{
		return "Usage:\n\tkc [-c] [-o OUTFILE] INFILE...";
	}

	int argIsInFile = 0;
	for (unsigned i = 1; i < (unsigned)argc; ++i) {
		if (argIsInFile) {
			conf->inFilePaths = sl_copy_append (conf->inFilePaths, argv[i]);
		}
		else if (!strcmp("-c", argv[i]))
			conf->noCppCompile = 1;
		else if (!strcmp("-o", argv[i])) {
			++i;
			conf->outFilepath = strdup (argv[i]);
		}
		else if (!strcmp("--self-debug", argv[i]))
			conf->selfDebug = 1;
		else if (!strcmp("-g", argv[i]))
			conf->debug = 1;
		else if (!strcmp("--enable-bounds-checking", argv[i]))
			conf->boundsChecking = 1;
		else if (!strcmp("--tidy", argv[i]))
			conf->tidy = 1;
		else if (!strcmp("--dynamic-library", argv[i]))
			conf->output_type = output_type_dynamic_library;
		else if (!strcmp("--static-library", argv[i]))
			conf->output_type = output_type_static_library;
		else if (!strcmp("--create-headers", argv[i]))
			conf->createHeaders = 1;
		else if (! strcmp("-I", argv[i])) {
			++i;
			conf->includePaths = sl_copy_append (conf->includePaths, argv[i]);
		}
		else if (! strcmp("-L", argv[i])) {
			++i;
			conf->libraryPaths = sl_copy_append (conf->libraryPaths, argv[i]);
		}
		else if (! strcmp("-l", argv[i])) {
			++i;
			conf->libraries = sl_copy_append (conf->libraries, argv[i]);
		}
		else if (!strcmp("--static-linkage", argv[i]))
			conf->staticLinkage = 1;
		else if ('-' == argv[i][0])
		{
			return "Argument not recognised.";
		}
		else {
			argIsInFile = 1;
			--i;
			continue;
		}
	}
	for (; conf->inFilePaths->prev; conf->inFilePaths = conf->inFilePaths->prev);
	for (; conf->includePaths->prev; conf->includePaths = conf->includePaths->prev);
	for (; conf->libraryPaths->prev; conf->libraryPaths = conf->libraryPaths->prev);
	for (; conf->libraries->prev; conf->libraries = conf->libraries->prev);
	return NULL;
}
