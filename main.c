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

#include <dlfcn.h>
#include <unistd.h>

#ifdef UNITY_BUILD

#define KC_PUBLIC      static
#define KC_PUBLIC_DATA static

#include "common.h"
#include "log.c"
#include "lex.c"
#include "scope.c"
#include "parse.c"
#include "type.c"
#include "ir.c"
#include "linux_system_v_x86_64.c"
#include "cmd_flags.c"

#else

#include "common.h"

#endif

#define STRVIEW_IMPLEMENTATION
#include "strview.h"
#include "cmd_flags.h"

KC_PRIVATE struct asm_module *
compile_file(const char *filename, struct asm_module *asm_mod, bool is_dll)
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
		}
	}
#endif
	/* IR => ASM */
	ir.is_dll = is_dll;
	for (size_t i = 0; i < ir.len; ++i) {
		switch (ir.elems[i].tag) {
		case IRO_PROC:
			asm_emit_procedure(&ir.elems[i].proc, &ir, da_allot(&asm_mod->procs));
			break;
		case IRO_DATA:
			asm_emit_datum(&ir.elems[i].data, da_allot(&asm_mod->data));
			break;
		case IRO_EXTERN_DATA: /* Do nothing */ break;
		case IRO_EXTERN_PROC: /* Do nothing */ break;
		default: FAILWITH("Unreachable"); break;
		}
	}
	return asm_mod;
}

KC_PRIVATE int
run_cmd(struct lines *args)
{
	char *cmd_str = concat_lines(args, " ");
	printf("[Info] %s\n", cmd_str);
	int c = system(cmd_str);
	free(cmd_str);
	return c;
}

// as --gdwarf-5 --64 syntax.s -o syntax.o
// ld -dynamic-linker /lib/ld-linux-x86-64.so.2 /usr/lib/crt1.o /usr/lib/crti.o -lc syntax.o /usr/lib/crtn.o
KC_PRIVATE int
assemble_module(struct asm_module *mod, char *target, bool debug)
{
	struct lines cmd = {0};
	da_append(&cmd, "as");
	if (debug) da_append(&cmd, "--gdwarf-5");
	da_append(&cmd, "--64");
	da_append(&cmd, "-o");
	da_append(&cmd, target);
	char *cmd_str = concat_lines(&cmd, " ");
	printf("[Info] %s\n", cmd_str);
	FILE *assembler = popen(cmd_str, "w");
	assert(assembler != NULL);
	asm_dump_module(mod, assembler);
	int c = pclose(assembler);
	free(cmd_str);
	da_free(&cmd);
	return c;
}

KC_PRIVATE int
assemble_file(char *filename, char *target, bool debug)
{
	struct lines cmd = {0};
	da_append(&cmd, "as");
	if (debug) da_append(&cmd, "--gdwarf-5");
	da_append(&cmd, "--64");
	da_append(&cmd, "-o");
	da_append(&cmd, target);
	da_append(&cmd, filename);
	char *cmd_str = concat_lines(&cmd, " ");
	int c = run_cmd(&cmd);
	free(cmd_str);
	da_free(&cmd);
	return c;
}

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
	da_copy(&cmd, obj_files);
	da_append(&cmd, "/usr/lib/crtn.o");
	da_append(&cmd, "-o");
	da_append(&cmd, target);
	int c = run_cmd(&cmd);
	da_free(&cmd);
	return c;
}

KC_PRIVATE int
run(char *source_file, const char *dll_file)
{
	void *handle = dlopen(dll_file, RTLD_NOW);
	if (handle == NULL) {
		fprintf(stderr, "[Error] %s\n", dlerror());
		return -1;
	}
	int(*fn)(int, char**) = dlsym(handle, "main");
	if (fn == NULL) {
		fprintf(stderr, "[Error] %s\n", dlerror());
		return -1;
	}
	char *args[] = {source_file, NULL};
	int c = fn(1, args);
	dlclose(handle);
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

int main(int argc, char **argv)
{
	static char cwd[PATH_MAX] = {0};
	struct lines input_files = {0};
	struct lines asm_files = {0};
	struct lines obj_files = {0};
	struct asm_modules asm_modules = {0};
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
				  .desc    = "Compile program to dll then call its `main` procedure.",
				  .data    = &run_p,
				  .handler = fh_set_bool_flag);
	cmd_flags_add(&flags,
				  .name    = "-S",
				  .desc    = "Produce assembly files as a by-product of compilation.",
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
		compile_file(input_files.elems[i], da_allot(&asm_modules), run_p);
		da_append(&asm_files, subst_file_suffix(input_files.elems[i], "s"));
	}
	if (output_asm_p) {
		for (size_t i = 0; i < asm_files.len; ++i) {
			FILE *file = fopen(asm_files.elems[i], "w");
			assert(file != NULL);
			asm_dump_module(&asm_modules.elems[i], file);
			fclose(file);
		}
	}
	for (size_t i = 0; i < input_files.len; ++i) {
		char *obj_file = subst_file_suffix(input_files.elems[i], "o");
		if (output_asm_p) {
			assemble_file(asm_files.elems[i], obj_file, true);
		} else {
			assemble_module(&asm_modules.elems[i], obj_file, true);
		}
		da_append(&obj_files, obj_file);
	}
	if (run_p) {
		char *dlname = strjoin(cwd, "dlrun.so", "/");
		link_object_files(&obj_files, dlname, true);
		printf("[Info] Running program...\n");
		printf("[Info] Program exited with error code: %d\n", run(input_files.elems[0], dlname));
	} else {
		link_object_files(&obj_files, target_file, false);
	}
	return 0;
}
