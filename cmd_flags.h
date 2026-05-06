#pragma once

/* Command line flags parsing */

typedef struct cmd_args Cmd_args;
typedef struct cmd_flag_def Cmd_flag_def;
typedef struct cmd_flags Cmd_flags;
typedef Cmd_args (*Cmd_flag_handler)(Cmd_flags *flags, Cmd_flag_def *fd, Cmd_args args, void *data);

struct cmd_args {
	char **argv;
	int argc;
};

struct cmd_flag_def {
	const char *name;
	const char *long_name;
	const char *desc;
	void *data;
	Cmd_flag_handler handler;
};

struct cmd_flags {
	Cmd_flag_def *elems;
	Cmd_flag_handler default_handler;
	void *default_data;
	uint32_t len, cap;
};

#define cmd_flags_add(flags_ptr, ...)				\
	cmd_flags_add_impl_(flags_ptr, (Cmd_flag_def){__VA_ARGS__})

Cmd_args cmd_args_next(Cmd_args args);
void cmd_parse_flags(Cmd_flags *flags, int argc, char **argv);
Cmd_flags cmd_flags_make(void *default_data, Cmd_flag_handler default_handler, const char *help_desc);
void cmd_flags_add_impl_(Cmd_flags *flags, Cmd_flag_def fd);
