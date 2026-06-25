#pragma once

#include "common.h"

typedef struct kc_session {
	const char *input_file;
	const char *target;
	const char *cwd;
	struct expression_stack tl_exps;
	struct scope *scope;
	struct {
		uint32_t len, cap;
		Parser *elems;
	} parsers;
	Syminfo symbols;
	IR_toplevel ir;
	CG_module cg_module;
	Mem_pool heap;
	bool run_p  : 1;
	bool link_p : 1;
	bool dump_ir_p : 1;
} KC_session;
