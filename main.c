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

KC_PRIVATE Mem_pool kc_heap;
KC_PUBLIC  Mem_pool *KC_HEAP = &kc_heap;

int set_sig_handler(void);

KC_PRIVATE void
kc_init(void)
{
	if (set_sig_handler() != 0)
		FAILWITH("[Error] %s\n", strerror(errno));
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
exec_cmd(struct lines *cmd)
{
	pid_t pid;
	int wstatus = 0;
	int io_pipes[2];
	assert(cmd->len > 0);
	if (pipe(io_pipes) < 0)
		FAILWITH("[Error] pipe(io_pipes): %s\n", strerror(errno));
	if ((pid = fork()) == 0) {
		/* child */
		printf("[Info]");
		for (size_t i = 0; i < cmd->len && cmd->elems[i]; ++i)
			printf(" %s", cmd->elems[i]);
		printf("\n");
		if (close(io_pipes[0]) < 0)
			FAILWITH("[Error] close(io_pipes[0]): %s\n", strerror(errno));
		if (dup2(io_pipes[1], STDOUT_FILENO) < 0)
			FAILWITH("[Error] dup2(io_pipes[1], STDOUT_FILENO): %s\n", strerror(errno));
		if (dup2(io_pipes[1], STDERR_FILENO) < 0)
			FAILWITH("[Error] dup2(io_pipes[1], STDERR_FILENO): %s\n", strerror(errno));
		if (execvp(cmd->elems[0], cmd->elems) < 0)
			FAILWITH("[Error] %s\n", strerror(errno));
	} else if (pid < 0) {
		FAILWITH("[Error] %s\n", strerror(errno));
	} else {
		/* parent */
		if (close(io_pipes[1]) < 0)
			FAILWITH("[Error] close(io_pipes[1]): %s\n", strerror(errno));
		wait(&wstatus);
		wstatus = WEXITSTATUS(wstatus);
		if (wstatus) {
			FILE *f = fdopen(io_pipes[0], "r");
			if (f == NULL)
				FAILWITH("[Error] fdopen(io_pipes[0], \"r\"): %s\n", strerror(errno));
			while (fgets(scratch_buffer, sizeof(scratch_buffer), f)) {
				printf("[Info] %s", scratch_buffer);
			}
			fclose(f);
		} else if (close(io_pipes[0]) < 0) {
			FAILWITH("[Error] close(io_pipes[0]): %s\n", strerror(errno));
		}
	}
	return wstatus;
}

// ld -dynamic-linker /lib/ld-linux-x86-64.so.2 /usr/lib/crt1.o /usr/lib/crti.o -lc syntax.o /usr/lib/crtn.o
KC_PRIVATE int
link_object_files(struct lines *obj_files, const char *target, bool is_dll)
{
	struct lines cmd = {0};
	da_append(&cmd, "ld");
	if (is_dll) da_append(&cmd, "-shared");
	da_append(&cmd, "-dynamic-linker");
	da_append(&cmd, "/lib64/ld-linux-x86-64.so.2");
	da_append(&cmd, "/usr/lib/crt1.o");
	da_append(&cmd, "/usr/lib/crti.o");
	da_append(&cmd, "-lc");
	da_concat(&cmd, obj_files);
	da_append(&cmd, "/usr/lib/crtn.o");
	da_append(&cmd, "-o");
	da_append(&cmd, (char *)target);
	da_append(&cmd, (char *)NULL);
	int c = exec_cmd(&cmd);
	da_free(&cmd);
	return c;
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

typedef struct {
	char **argv;
	int  argc;
} CL_args;

KC_PRIVATE int
shift_args(CL_args *args)
{
	args->argc--;
	args->argv++;
	return args->argc;
}

KC_PRIVATE const char *
get_arg(CL_args *args)
{
	return *args->argv;
}

KC_PRIVATE bool
is_valid_input_file_name(const char *file)
{
	int n1 = 0, n2 = 0;
	int len = strlen(file);
	if (sscanf(file, "%*[a-zA-Z_/]%n.k", &n1) == EOF || n1 == 0)
		return false;
	switch (len - n1) {
	case 0: return true;
	case 1: return false;
	case 2:
		if (sscanf(file+n1, ".k%n", &n2) == EOF)
			return false;
		break;
	default:
		if (sscanf(file+n1, "%*[a-zA-Z0-9_/].k%n", &n2) == EOF)
			return false;
		break;
	}
	return len == n1 + n2;
}

KC_PRIVATE int
dispatch_build(CL_args args)
{
	KC_session session = {.target = "a.out", .link_p = true};
	printf("[Info] dispatch_build\n");
	if (getcwd(session.cwd, sizeof(session.cwd)) == NULL) {
		fprintf(stderr, "[Error] %s\n", strerror(errno));
		return 1;
	}
	while (shift_args(&args) > 0) {
		const char *arg = get_arg(&args);
		if (strcmp("-o", arg) == 0) {
			if (shift_args(&args) <= 0) {
				fprintf(stderr, "[Error] No output file provided after flag `-o`\n");
				return 1;
			}
			session.target = get_arg(&args);
		} else if (strcmp("-c", arg) == 0) {
			session.link_p = false;
		} else if (is_valid_input_file_name(arg)) {
			session.input_file = arg;
		} else {
			fprintf(stderr, "[Error] Invalid input file `%s`\n", arg);
			return 1;
		}
	}
	if (session.input_file == NULL) {
		fprintf(stderr, "[Error] No input file was provided\n");
		return 1;
	}
	compilation_phase1(&session);
	compilation_phase2(&session);
	compilation_phase3(&session);
	const char *ofile = subst_file_suffix(session.input_file, "o");
	asm_output_object_file(&session.cg_module.as, ofile);
	if (session.link_p) {
		struct lines obj_files = {
			.cap = 1,
			.len = 1,
			.elems = (void *)&ofile,
		};
		return link_object_files(&obj_files, session.target, false);
	}
	return 0;
}

KC_PRIVATE int
dispatch_run(CL_args args)
{
	KC_session session = {.run_p = true};
	printf("[Info] dispatch_run\n");
	if (getcwd(session.cwd, sizeof(session.cwd)) == NULL) {
		fprintf(stderr, "[Error] %s\n", strerror(errno));
		return 1;
	}
	while (shift_args(&args)) {
		const char *arg = get_arg(&args);
		if (is_valid_input_file_name(arg)) {
			session.input_file = arg;
		} else {
			fprintf(stderr, "[Error] Invalid input file `%s`\n", arg);
			return 1;
		}
	}
	if (session.input_file == NULL) {
		fprintf(stderr, "[Error] No input file was provided\n");
		return 1;
	}
	compilation_phase1(&session);
	compilation_phase2(&session);
	int(*entry_point)(int, const char**) = compilation_phase3(&session);
	if (entry_point) {
		const char *argv[] = {session.input_file, NULL};
		run_code_from_entry_point(&session.cg_module, entry_point, 1, argv);
	} else {
		fprintf(stderr, "[Error] No entry point found in compilation unit\n");
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	kc_init();
	CL_args args = {argv, argc};
	printf("%s\n", get_arg(&args));
	if (shift_args(&args) <= 0) FAILWITH("TODO: print default help message.");
	if (strcmp("build", get_arg(&args)) == 0)
		return dispatch_build(args);
	if (strcmp("run", get_arg(&args)) == 0)
		return dispatch_run(args);
	fprintf(stderr, "[Error] Unrecognized command: `%s`\n", get_arg(&args));
	FAILWITH("TODO: print default help message.");
	return 0;
}
