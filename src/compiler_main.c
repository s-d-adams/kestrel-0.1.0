/* Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED. */
#include <stdio.h>

#include "conf.h"
#include "context.h"
#include "compiler.h"

int main (int const argc, char const * const * const argv)
{
	int rc = 0;
	struct conf config;
	struct context ctx;
	
	config = default_conf ();
	char const * msg = config_read_command_line (& config, argc, argv);
	if (msg)
	{
		printf ("%s\n", msg);
		rc = 1;
		goto exit1;
	}
	context_init (& ctx, config);
	msg = compiler_compile (& ctx);
	if (msg)
	{
		printf ("%s\n", msg);
		rc = 1;
		goto exit2;
	}

exit2:
	del_context (& ctx);
exit1:
	del_conf (& config);
	return rc;
}
