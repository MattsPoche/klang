/**
TODOS:
- [ ] Proper modules and namespaces
- [ ] Implement stubs for basic operators
- [ ] Floating point types
- [ ] Vector types
- [ ] For loops
- [ ] Loop break
- [ ] Loop continue
- [ ] Loop labels
- [ ] Early return
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

KC_PUBLIC  Mem_pool *KC_HEAP = NULL;

KC_PRIVATE int set_sig_handler(void);
KC_PRIVATE bool recover_from_handler = false;
KC_PRIVATE jmp_buf recover_exec;

KC_PRIVATE void
kc_init(void)
{
	if (set_sig_handler() != 0)
		FAILWITH("[Error] %s\n", strerror(errno));
}

void *
kc_malloc(size_t size)
{
	return mem_pool_alloc(KC_HEAP, size);
}

void
kc_free(UNUSED void *ptr) {}

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
			while (fgets(scratch_buffer[0], sizeof(scratch_buffer[0]), f)) {
				printf("[Info] %s", scratch_buffer[0]);
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

KC_PRIVATE void
handler(int sig, siginfo_t *info, UNUSED void *ucontext)
{
	if (sig == SIGSEGV) {
		fprintf(stderr, "[ERROR] %s (0x%lx)\n", strsignal(sig), (uintptr_t)info->si_addr);
	}
	if (recover_from_handler) longjmp(recover_exec, sig);
	EXIT(sig);
}

KC_PRIVATE int
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

KC_PRIVATE int
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

KC_PRIVATE void
compilation_phase1(KC_session *session)
{
	kc_parse(session);
	type_check(session);
}

KC_PRIVATE void
compilation_phase2(KC_session *session)
{
	ast_desugar(&session->tl_exps);
	session->ir = ast_compile(session->scope);
}

KC_PRIVATE void *
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

struct kc_session_args {
	const char *input_file;
	const char *target;
	bool run_p  : 1;
	bool link_p : 1;
};

KC_PRIVATE bool
kc_session_set_input_file(KC_session *s, const char *input_file)
{
	if (is_valid_input_file_name(input_file)) {
		if (realpath(input_file, scratch_buffer[0]) == NULL) {
			fprintf(stderr, "[Error] realpath: %s", strerror(errno));
			return false;
		}
		s->input_file = str_dup(scratch_buffer[0], strlen(scratch_buffer[0]));
	} else {
		fprintf(stderr, "[Error] Invalid input file name `%s`\n", input_file);
		return false;
	}
	return true;
}

KC_PRIVATE bool
kc_session_set_target_file(KC_session *s, const char *target_file)
{
	if (realpath(target_file, scratch_buffer[0]) == NULL) {
		s->target = fmt_str("%s/%s", s->cwd, target_file);
	} else {
		s->target = str_dup(scratch_buffer[0], strlen(scratch_buffer[0]));
	}
	return true;
}

KC_PRIVATE bool
kc_session_set_cwd(KC_session *s)
{
	if (getcwd(scratch_buffer[0], sizeof(scratch_buffer[0])) == NULL) {
		fprintf(stderr, "[Error] %s\n", strerror(errno));
		return false;
	}
	s->cwd = str_dup(scratch_buffer[0], strlen(scratch_buffer[0]));
	return true;
}

#define kc_session_init(s, ...)											\
	kc_session_init_impl(s, ((struct kc_session_args){__VA_ARGS__}))

#define kc_session_create(...)										\
	kc_session_create_impl((struct kc_session_args){__VA_ARGS__})

KC_PRIVATE bool
kc_session_init_impl(KC_session *s, struct kc_session_args args)
{
	memset(s, 0, sizeof(*s));
	s->heap = mem_pool_create();
	KC_HEAP = &s->heap;
	s->run_p = args.run_p;
	s->link_p = args.link_p;
	bool c = true;
	s->symbols = syminfo_create();
	c &= kc_session_set_cwd(s);
	if (args.input_file)
		c &= kc_session_set_input_file(s, args.input_file);
	if (args.target)
		c &= kc_session_set_target_file(s, args.target);
	return c;
}

KC_PRIVATE KC_session *
kc_session_create_impl(struct kc_session_args args)
{
	KC_session *s = malloc(sizeof(*s));
	if (s && kc_session_init_impl(s, args))
		return s;
	free(s);
	return NULL;
}

KC_PRIVATE void
kc_session_end(KC_session *s)
{
	mem_pool_destroy(&s->heap);
}

KC_PRIVATE int
dispatch_query(CL_args args)
{
	printf("[Info] %s\n", __func__);
	KC_session *session;
	if ((session = kc_session_create()) == NULL) return 1;
	printf("[Info] cwd: %s\n", session->cwd);
	while (shift_args(&args)) {
		if (!kc_session_set_input_file(session, get_arg(&args)))
			return 1;
	}
	if (session->input_file[0] == 0) {
		fprintf(stderr, "[Error] No input file was provided\n");
		return 1;
	}
	compilation_phase1(session);
	Syminfo_iter iter = syminfo_iter(session->symbols);
	Syminfo_pair pair = {0};
	while (syminfo_iter_next(&iter, &pair)) {
		for (Syminfo_list list = pair.info_list; list; list = list->next) {
			const char *filename;
			uint32_t line, column, offset;
			if (list->info && list->info->tag == SYMTBL_VARIABL) {
				filename = list->info->name->filename;
				offset   = list->info->name->offset;
				line     = list->info->name->line;
				column   = list->info->name->column;
				struct definition *def = list->info->variable.def;
				fprintf(stdout,
						"@\t%s\t%u\t%u\t%u\tv\t\""SV_FMT"\"\tt\t\"",
						filename, line, column+1, offset, SV_ARGS(pair.name));
				ast_type_fprint(def->type, stdout);
				printf("\"\n");
			} else if (list->info && list->info->tag == SYMTBL_VALCONS) {
				filename = list->info->name->filename;
				offset   = list->info->name->offset;
				line     = list->info->name->line;
				column   = list->info->name->column;
				KCType *memb = list->info->valcons.type;
				KCType type  = get_temp_app_type_from_definition(list->info->valcons.td);
				fprintf(stdout,
						"@\t%s\t%u\t%u\t%u\tc\t\""SV_FMT"\"\tt\t\"",
						filename, line, column+1, offset, SV_ARGS(pair.name));
				ast_type_fprint(memb, stdout);
				fprintf(stdout, "\"\tt\t\"");
				ast_type_fprint(&type, stdout);
				fprintf(stdout, "\"\n");
			} else if (list->info && list->info->tag == SYMTBL_TYPE) {
				filename = list->info->name->filename;
				offset   = list->info->name->offset;
				line     = list->info->name->line;
				column   = list->info->name->column;
				KCType type = get_temp_app_type_from_definition(&list->info->type);
				fprintf(stdout,
						"@\t%s\t%u\t%u\t%u\tt\t\"",
						filename, line, column+1, offset);
				ast_type_fprint(&type, stdout);
				fprintf(stdout, "\"\tt\t\"");
				ast_type_fprint(list->info->type.type, stdout);
				fprintf(stdout, "\"\n");
			}
		}
	}
	return 0;
}

KC_PRIVATE int
dispatch_build(CL_args args)
{
	printf("[Info] %s\n", __func__);
	KC_session *session;
	if ((session = kc_session_create(.target = "a.out", .link_p = true)) == NULL) return 1;
	printf("[Info] cwd: %s\n", session->cwd);
	while (shift_args(&args) > 0) {
		const char *arg = get_arg(&args);
		if (strcmp("-o", arg) == 0) {
			if (shift_args(&args) <= 0) {
				fprintf(stderr, "[Error] No output file provided after flag `-o`\n");
				return 1;
			}
			if (!kc_session_set_target_file(session, get_arg(&args)))
				return 1;
		} else if (strcmp("-c", arg) == 0) {
			session->link_p = false;
		} else if (strcmp("--dump-ir", arg) == 0) {
			session->dump_ir_p = true;
		} else if (!kc_session_set_input_file(session, arg)) {
			return 1;
		}
	}
	if (session->input_file[0] == 0) {
		fprintf(stderr, "[Error] No input file was provided\n");
		return 1;
	}
	compilation_phase1(session);
	compilation_phase2(session);
	if (session->dump_ir_p) {
		printf("[Info] ir dump:\n");
		for (size_t i = 0; i < session->ir.len; ++i) {
			if (session->ir.elems[i].hddr.tag == IRO_PROC) {
				ir_proc_fprint(&session->ir.elems[i].proc, stdout);
				printf("\n");
			}
		}
	}
	compilation_phase3(session);
	const char *ofile = subst_file_suffix(session->input_file, "o");
	asm_output_object_file(&session->cg_module.as, ofile);
	if (session->link_p) {
		struct lines obj_files = {
			.cap = 1,
			.len = 1,
			.elems = (void *)&ofile,
		};
		return link_object_files(&obj_files, session->target, false);
	}
	return 0;
}

KC_PRIVATE int
dispatch_run(CL_args args)
{
	printf("[Info] %s\n", __func__);
	KC_session *session;
	if ((session = kc_session_create(.run_p = true)) == NULL) return 1;
	printf("[Info] cwd: %s\n", session->cwd);
	while (shift_args(&args)) {
		const char *arg = get_arg(&args);
		if (strcmp("--dump-ir", arg) == 0) {
			session->dump_ir_p = true;
		} else if (!kc_session_set_input_file(session, arg)) {
			return 1;
		}
	}
	if (session->input_file[0] == 0) {
		fprintf(stderr, "[Error] No input file was provided\n");
		return 1;
	}
	compilation_phase1(session);
	compilation_phase2(session);
	if (session->dump_ir_p) {
		printf("[Info] ir dump:\n");
		for (size_t i = 0; i < session->ir.len; ++i) {
			if (session->ir.elems[i].hddr.tag == IRO_PROC) {
				ir_proc_fprint(&session->ir.elems[i].proc, stdout);
				printf("\n");
			}
		}
	}
	int(*entry_point)(int, const char**) = compilation_phase3(session);
	if (entry_point) {
		const char *argv[] = {session->input_file, NULL};
		run_code_from_entry_point(&session->cg_module, entry_point, 1, argv);
	} else {
		fprintf(stderr, "[Error] No entry point found in compilation unit\n");
		return 1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	kc_init();
	CL_args args = {argv, argc};
	if (shift_args(&args) <= 0) FAILWITH("TODO: print default help message.");
	if (strcmp("build", get_arg(&args)) == 0)
		return dispatch_build(args);
	if (strcmp("run", get_arg(&args)) == 0)
		return dispatch_run(args);
	if (strcmp("query", get_arg(&args)) == 0)
		return dispatch_query(args);
	fprintf(stderr, "[Error] Unrecognized command: `%s`\n", get_arg(&args));
	FAILWITH("TODO: print default help message.");
	return 0;
}
