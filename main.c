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
- [x] Global variables
- [ ] Dynamic memory allocation syntax
- [ ] Static memory storage syntax?
- [ ] Local functions
- [ ] Function literals
- [ ] Proper recursive let semantics
- [ ] Expanded pattern syntax: structs, arrays, nested patterns
- [ ] Patterns in let bindings? in function definitions?
- [ ] Type classes
- [ ] Query mode (LSP-like command interpreter)
  - Read stdin for commands
  - Output result to stdout
  - For example:
    - query_symbol(foo) > kc > list of symbol definitions
	- recompile(source.k) > kc > compiler error or ok
- [ ] Enum types? Some sort of auto-increment integer constants?
- [ ] Macros?
- [ ] Closures?
**/

#include <setjmp.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "mempool.h"
#include "common.h"
#define STRVIEW_IMPLEMENTATION
#include "strview.h"
#include "cmd_flags.h"

Mem_pool kc_heap;

__attribute__((constructor))
static void
init_heap(void)
{
	kc_heap = mem_pool_create();
}

void *
kc_malloc(size_t size)
{
	return mem_pool_alloc(&kc_heap, size);
}

void kc_free(UNUSED void *ptr) {}

void *
kc_calloc(size_t n, size_t size)
{
	size *= n;
	return memset(kc_malloc(size), 0, size);
}

void *
kc_realloc(void *ptr, size_t size)
{
	void *new_ptr = kc_malloc(size);
	return memcpy(new_ptr, ptr, MEM_POOL_PTR_SZ(ptr));
}

KC_PRIVATE int
exec_cmd(struct lines *args)
{
	char *cmd_str = concat_lines(args, " ");
	printf("[Info] %s\n", cmd_str);
	int c = system(cmd_str);
	FREE(cmd_str);
	return c;
}

// ld -dynamic-linker /lib/ld-linux-x86-64.so.2 /usr/lib/crt1.o /usr/lib/crti.o -lc syntax.o /usr/lib/crtn.o
KC_PRIVATE int
link_object_files(struct lines *obj_files, const char *target, bool is_dll)
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
	da_append(&cmd, (void *)target);
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
	if (sscanf(args.argv[0], "%s.k", input) != 1)
		FAILWITH("[Error] Invalid input file %s\n", args.argv[0]);
	*(const char **)data = str_dup(input, strlen(input));
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
	EXIT(sig);
}

int
set_sig_handler(void)
{
	struct sigaction act = {0};
	act.sa_sigaction = handler;
	act.sa_flags = SA_SIGINFO;
	return sigaction(SIGSEGV, &act, NULL);
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

int
run_code_from_entry_point(CG_module *m, int(*entry_point)(int argc, const char *argv[]),
						  int argc, const char *argv[])
{
	printf("[Info] Running initializers ...\n");
	recover_from_handler = true;
	for (size_t i = 0; i < m->thunks.len; ++i) {
		void (*thunk)(void) = asm_get_label_address(&m->as, m->thunks.elems[i]);
		thunk();
	}
	printf("[Info] Running program...\n");
	int ec;
	if ((ec = setjmp(recover_exec)) == 0) {
		ec = entry_point(argc, argv);
		printf("[Info] Program exited with error code: %d\n", ec);
	} else {
		printf("[Info] Program encountered fatal error and crashed\n");
		printf("[Info] Disassembly of compiled code:\n");
		fprint_disassembly(&m->as, stderr);
	}
	recover_from_handler = false;
	return ec;
}

void
compilation_phase1(KC_session *session)
{
	struct expression_stack tl_exps = {0};
	struct typing_context ctx = {0};
	ctx.scope = parse_file(session->input_file, &tl_exps);
	type_check(&ctx, &tl_exps);
	ast_desugar(&tl_exps, ctx.scope);
	session->scope = ctx.scope;
}

void
compilation_phase2(KC_session *session)
{
	session->ir = ast_compile(session->scope);
}

void *
compilation_phase3(KC_session *session)
{
	cg_emit_module_code(&session->cg_module, &session->ir, session->run_p);
	asm_assemble(&session->cg_module.as);
	void *entry_point;
	if (session->run_p && asm_lookup_label_address(&session->cg_module.as, "main", &entry_point)) {
		asm_set_executable(&session->cg_module.as);
		return entry_point;
	} else {
		return NULL;
	}
}

int main(int argc, char **argv)
{
	KC_session session = {.target = "a.out", .run_p = false};
	bool output_asm_p = false;
	if (set_sig_handler() != 0) {
		fprintf(stderr, "[Error] %s\n", strerror(errno));
		return 1;
	}
	if (getcwd(session.cwd, sizeof(session.cwd)) == NULL) {
		fprintf(stderr, "[Error] %s\n", strerror(errno));
		return 1;
	}
	/* process args and compile input files */
	Cmd_flags flags = cmd_flags_make(&session.input_file, fh_default_handler, "Usage kc [options] file...");
	cmd_flags_add(&flags,
				  .name    = "-run",
				  .desc    = "Run compiled program from memory. Do not produce an output file.",
				  .data    = &session.run_p,
				  .handler = fh_set_bool_flag);
	cmd_flags_add(&flags,
				  .name    = "-S",
				  .desc    = "Print out disassembly.",
				  .data    = &output_asm_p,
				  .handler = fh_set_bool_flag);
	cmd_flags_add(&flags,
				  .name    = "-o",
				  .desc    = "Specify output file name.",
				  .data    = &session.target,
				  .handler = fh_set_output_file);
	cmd_parse_flags(&flags, argc, argv);
	if (session.input_file == NULL) return 0;
	compilation_phase1(&session);
	compilation_phase2(&session);
	int(*entry_point)(int, const char**) = compilation_phase3(&session);
	if (output_asm_p) {
		fprint_disassembly(&session.cg_module.as, stdout);
	}
	if (entry_point) {
		const char *argv[] = {session.input_file, NULL};
		run_code_from_entry_point(&session.cg_module, entry_point, 1, argv);
	} else {
		const char *ofile = subst_file_suffix(session.input_file, "o");
		asm_output_object_file(&session.cg_module.as, ofile);
		struct lines obj_files = {0};
		da_append(&obj_files, (void *)ofile);
		link_object_files(&obj_files, session.target, false);
	}
	size_t total, bufc;
	mem_pool_total_allocated(&kc_heap, &total, &bufc);
	printf("[Info] Total memory allocated: %zu in %zu buffers.\n", total, bufc);
	return 0;
}
