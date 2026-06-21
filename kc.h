#pragma once

#include "common.h"

typedef struct kc_session {
	const char *input_file;
	const char *target;
	struct expression_stack tl_exps;
	struct scope *scope;
	Syminfo symbols;
	IR_toplevel ir;
	CG_module cg_module;
	bool run_p;
	bool link_p;
	char cwd[PATH_MAX];
} KC_session;
