#pragma once

#include "common.h"

typedef struct kc_session {
	const char *input_file;
	const char *target;
	IR_toplevel ir;
	struct scope *scope;
	CG_module cg_module;
	bool run_p;
	char cwd[PATH_MAX];
} KC_session;
