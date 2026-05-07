#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_flags.h"
#include "da.h"

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

Cmd_args
cmd_args_next(Cmd_args args)
{
	args.argc--;
	args.argv++;
	return args;
}

static Cmd_args
cmd_parse_flag(Cmd_flags *flags, Cmd_args args)
{
	for (size_t i = 0; i < flags->len; ++i) {
		Cmd_flag_def *fd = &flags->elems[i];
		if (fd->name && strcmp(fd->name, args.argv[0]) == 0)
			return fd->handler(flags, fd, args, flags->elems[i].data);
		if (fd->long_name && strcmp(fd->long_name, args.argv[0]) == 0)
			return fd->handler(flags, fd, args, flags->elems[i].data);

	}
	return flags->default_handler(flags, NULL, args, flags->default_data);
}

void
cmd_parse_flags(Cmd_flags *flags, int argc, char **argv)
{
	Cmd_args args = {.argc = argc, .argv = argv};
	args = cmd_args_next(args);
	while (args.argc > 0) {
		args = cmd_parse_flag(flags, args);
	}
}

static Cmd_args
fh_default_help_handler(Cmd_flags *flags, Cmd_flag_def *self, Cmd_args args, UNUSED void *data)
{
	printf("%s\n", self->desc);
	printf("Options:\n");
	for (size_t i = 0; i < flags->len; ++i) {
		Cmd_flag_def *fd = &flags->elems[i];
		const char *desc = fd == self ? "Display this information." : fd->desc;
		int colw = 24;
		int n = 0;
		if (fd->long_name) {
			printf("  %s / %s %n", fd->long_name, fd->name, &n);
			printf("%*s", colw - n, "");
		} else {
			printf("  %s %n", fd->name, &n);
			printf("%*s", colw - n, "");
		}
		printf("%s\n", desc);
	}
	return cmd_args_next(args);
}

Cmd_flags
cmd_flags_make(void *default_data, Cmd_flag_handler default_handler, const char *help_desc)
{
	Cmd_flags flags = {
		.default_data = default_data,
		.default_handler = default_handler,
	};
	cmd_flags_add(&flags,
				  .name      = "-h",
				  .long_name = "--help",
				  .handler   = fh_default_help_handler,
				  .desc      = help_desc);
	return flags;
}

void
cmd_flags_add_impl_(Cmd_flags *flags, Cmd_flag_def fd)
{
	da_append(flags, fd);
}
