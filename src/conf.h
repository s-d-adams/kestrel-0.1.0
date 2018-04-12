/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#pragma once

#include "string_list.h"

enum output_type { output_type_executable, output_type_dynamic_library, output_type_static_library };

struct conf
{
	int noCppCompile;
	char * outFilepath;
	int selfDebug;
	int debug;
	int boundsChecking;
	int tidy;
	struct string_list * inFilePaths;
	enum output_type output_type;
	int createHeaders;
	struct string_list * includePaths;
	struct string_list * libraryPaths;
	struct string_list * libraries;
	int staticLinkage;
};

struct conf
default_conf (void);

void
del_conf (struct conf *);

// Returns NULL on success, an error message otherwise.
char const *
config_read_command_line (struct conf * const conf, unsigned const argc, const char * const * const argv);
