/**
TODOS:
- [ ] Implement stubs for basic operators
- [ ] Floating point types
- [ ] Vector types
- [ ] For loops
- [ ] Loop break
- [ ] Loop continue
- [ ] Loop labels
- [ ] Early return
- [ ] Global variables
- [ ] Dynamic memory allocation syntax
- [ ] Static memory storage syntax?
- [ ] Local functions
- [ ] Function literals
- [ ] Proper recursive let semantics
- [ ] Expanded pattern syntax: structs, arrays, nested patterns
- [ ] Patterns in let bindings? in function definitions?
- [ ] Type classes
- [ ] Enum types? Some sort of auto-increment integer constants?
- [ ] Macros?
- [ ] Closures?
**/

#include <setjmp.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define STRVIEW_IMPLEMENTATION
#include "strview.h"
#include "cmd_flags.h"
#include "common.h"

KC_PRIVATE CG_module
compile_file(const char *filename, bool is_jit)
{
	struct strview sv = {0};
	if (sv_open_file(filename, &sv) == false) {
		fprintf(stderr, "[Error] %s: %s\n", filename, strerror(errno));
		EXIT(1);
	}
	if (sv.len == 0) {
		fprintf(stderr, "[Error] %s: %s\n", filename, "Empty file");
		EXIT(1);
	}
	Parser parser = {
		.lexer = {
			.filename = filename,
			.text     = sv.ptr,
			.length   = sv.len,
			.line     = 1,
		}
	};
	tokenize(&parser.lexer, &parser.tokens);
	struct expression_stack tl = {0};
	struct expression *exp = NULL;
	struct scope sc = {0};
	while (!parser_is_at_end(&parser)) {
		exp = parse_toplevel_expression(&parser, &sc);
		if (exp != NULL) da_append(&tl, exp);
	}
	/* Type check */
	struct typing_context ctx = {.scope = &sc};
	type_check(&parser, &ctx, &tl);
	/* Desugar */
	for (size_t i = 0; i < tl.len; ++i) {
		tl.elems[i] = ast_desugar(&parser, tl.elems[i], &sc);
	}
	/* AST => IR */
	struct ir_toplevel ir = ast_compile(&sc);
#if KC_DEBUG
	puts("[Debug]");
	for (size_t i = 0; i < ir.len; ++i) {
		if (ir.elems[i].tag == IRO_PROC) {
			ir_proc_fprint(&ir.elems[i].proc, stdout);
			fputc('\n', stdout);
		}
	}
#endif
	/* IR => ASM */
	CG_module cg_mod = {0};
	cg_emit_module_code(&cg_mod, &ir, is_jit);
	return cg_mod;
}

KC_PRIVATE int
exec_cmd(struct lines *args)
{
	char *cmd_str = concat_lines(args, " ");
	printf("[Info] %s\n", cmd_str);
	int c = system(cmd_str);
	free(cmd_str);
	return c;
}

// ld -dynamic-linker /lib/ld-linux-x86-64.so.2 /usr/lib/crt1.o /usr/lib/crti.o -lc syntax.o /usr/lib/crtn.o
KC_PRIVATE int
link_object_files(struct lines *obj_files, char *target, bool is_dll)
{
	struct lines cmd = {0};
	da_append(&cmd, "ld");
	if (is_dll) da_append(&cmd, "-shared");
	da_append(&cmd, "-dynamic-linker /lib/ld-linux-x86-64.so.2");
	da_append(&cmd, "/usr/lib/crt1.o");
	da_append(&cmd, "/usr/lib/crti.o");
	da_append(&cmd, "-lc");
	da_concat(&cmd, obj_files);
	da_append(&cmd, "/usr/lib/crtn.o");
	da_append(&cmd, "-o");
	da_append(&cmd, target);
	int c = exec_cmd(&cmd);
	da_free(&cmd);
	return c;
}

KC_PRIVATE Cmd_args
fh_set_bool_flag(UNUSED Cmd_flags *flags, UNUSED Cmd_flag_def *fd, Cmd_args args, void *data)
{
	*(bool *)data = true;
	return cmd_args_next(args);
}

KC_PRIVATE Cmd_args
fh_set_output_file(UNUSED Cmd_flags *flags, UNUSED Cmd_flag_def *fd, Cmd_args args, void *data)
{
	args = cmd_args_next(args);
	assert(args.argc > 0);
	*(char **)data = args.argv[0];
	return cmd_args_next(args);
}

KC_PRIVATE Cmd_args
fh_default_handler(UNUSED Cmd_flags *flags, UNUSED Cmd_flag_def *fd, Cmd_args args, void *data)
{
	static char input[PATH_MAX];
	struct lines *input_files = data;
	if (sscanf(args.argv[0], "%s.k", input) != 1)
		FAILWITH("[Error] Invalid input file %s\n", args.argv[0]);
	da_append(input_files, strdup(input));
	return cmd_args_next(args);
}

void
list_asm(Asm_module *m)
{
	Asm_inst *ins = asm_get_instruction_array(m);
	size_t count = asm_get_instruction_count(m);
	for (size_t i = 0; i < count; ++i) {
		if (ins[i].op == DIR_LABEL) {
			printf("[%zu] %s %s \"%s\"\n", i,
				   asm_op_code_to_str(ins[i].op),
				   asm_arg_desc_to_str(ins[i].dsc),
				   m->labels.elems[ins[i].disp].name);
		} else if (ins[i].op == OP_CALL) {
			printf("[%zu] %s %s \"%s\"\n", i,
				   asm_op_code_to_str(ins[i].op),
				   asm_arg_desc_to_str(ins[i].dsc),
				   m->labels.elems[ins[i].disp].name);
		} else {
			printf("[%zu] %s %s\n", i,
				   asm_op_code_to_str(ins[i].op),
				   asm_arg_desc_to_str(ins[i].dsc));
		}
	}
}

static bool recover_from_handler = false;
static jmp_buf recover_exec;

void
handler(int sig, siginfo_t *info, UNUSED void *ucontext)
{
	if (sig == SIGSEGV) {
		fprintf(stderr, "[ERROR] %s (0x%lx)\n", strsignal(sig), (uintptr_t)info->si_addr);
	}
	if (recover_from_handler) longjmp(recover_exec, sig);
	exit(sig);
}

void
fprint_disassembly(Asm_module *m, FILE *f)
{
	char line[0x1000];
	char temp_name[] = "XXXXXX";
	int fd = mkstemp(temp_name);
	write(fd, m->sections[ASM_SECTION_TEXT].data, m->sections[ASM_SECTION_TEXT].len);
	close(fd);
	FILE *pipe = popen(fmt_str("objdump -D -b binary -m i386:x64-32 %s", temp_name), "r");
	fgets(line, sizeof(line), pipe);
	fgets(line, sizeof(line), pipe);
	fgets(line, sizeof(line), pipe);
	for (; fgets(line, sizeof(line), pipe); fputs(line, f));
	pclose(pipe);
	remove(temp_name);
}

void
run_code_from_entry_point(CG_module *m, int(*entry_point)(int argc, char *argv[]), int argc, char *argv[])
{
	struct sigaction act = {0};
	act.sa_sigaction = handler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, NULL);
	printf("[Info] Running initializers ...\n");
	for (size_t i = 0; i < m->thunks.len; ++i) {
		void (*thunk)(void) = asm_get_label_address(&m->asm_mod, m->thunks.elems[i]);
		thunk();
	}
	printf("[Info] Running program...\n");
	recover_from_handler = true;
	int ec;
	if ((ec = setjmp(recover_exec)) == 0) {
		ec = entry_point(argc, argv);
		printf("[Info] Program exited with error code: %d\n", ec);
	} else {
		printf("[Info] Program encountered fatal error and crashed\n");
		printf("[Info] Disassembly of compiled code:\n");
		fprint_disassembly(&m->asm_mod, stderr);
	}
	recover_from_handler = false;
}

int main(int argc, char **argv)
{
	static char cwd[PATH_MAX] = {0};
	struct lines input_files = {0};
	struct lines obj_files = {0};
	CG_modules cg_modules = {0};
	char *target_file = "a.out";
	bool output_asm_p = false;
	bool run_p = false;
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		fprintf(stderr, "[Error] %s\n", strerror(errno));
		return 1;
	}
	/* process args and compile input files */
	Cmd_flags flags = cmd_flags_make(&input_files, fh_default_handler, "Usage kc [options] file...");
	cmd_flags_add(&flags,
				  .name    = "-run",
				  .desc    = "Run compiled program from memory. Do not produce an output file.",
				  .data    = &run_p,
				  .handler = fh_set_bool_flag);
	cmd_flags_add(&flags,
				  .name    = "-S",
				  .desc    = "Print out disassembly.",
				  .data    = &output_asm_p,
				  .handler = fh_set_bool_flag);
	cmd_flags_add(&flags,
				  .name    = "-o",
				  .desc    = "Specify output file name.",
				  .data    = &target_file,
				  .handler = fh_set_output_file);
	cmd_parse_flags(&flags, argc, argv);
	if (input_files.len == 0) return 0;
	for (size_t i = 0; i < input_files.len; ++i) {
		da_append(&cg_modules, compile_file(input_files.elems[i], run_p));
		da_append(&obj_files, subst_file_suffix(input_files.elems[i], "o"));
	}
	int(*entry_point)(int, char**) = NULL;
	for (size_t i = 0; i < cg_modules.len; ++i) {
		Asm_module *m = &cg_modules.elems[i].asm_mod;
#ifdef KC_DEBUG
		puts("[Debug]");
		list_asm(m);
#endif
		asm_assemble(m);
		if (run_p)
			asm_set_executable(m);
		if (entry_point == NULL)
			asm_lookup_label_address(m, "main", (void *)&entry_point);
	}
	if (output_asm_p) {
		for (size_t i = 0; i < cg_modules.len; ++i) {
			Asm_module *m = &cg_modules.elems[i].asm_mod;
			fprint_disassembly(m, stdout);
		}
	}
	if (run_p) {
		if (entry_point == NULL)
			FAILWITH("[ERROR] Unable to run compiled code. No `main` procedure found.");
		char *argv[] = {input_files.elems[0], NULL};
		run_code_from_entry_point(&cg_modules.elems[0], entry_point, 1, argv);
	} else {
		for (size_t i = 0; i < cg_modules.len; ++i) {
			Asm_module *m = &cg_modules.elems[i].asm_mod;
			asm_output_object_file(m, obj_files.elems[i]);
		}
		link_object_files(&obj_files, target_file, false);
	}
	return 0;
}
