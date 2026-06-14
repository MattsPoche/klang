#pragma once

#ifndef NO_STD_HEADERS
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

#ifndef FAILWITH
#define FAILWITH(_fmt_msg, ...)										\
	do {															\
		fflush(stdout);												\
		fflush(stderr);												\
		fprintf(stderr, "%s: %d: [FAILWITH] ", __FILE__, __LINE__); \
		fprintf(stderr, _fmt_msg __VA_OPT__(,) __VA_ARGS__);		\
		fputc('\n', stderr);										\
		__asm__("int3");											\
		exit(1);													\
	} while (0)
#endif

typedef signed   char elf64_sbyte;
typedef unsigned char elf64_ubyte;
typedef uint64_t      elf64_addr;   //   8 8 Unsigned program address
typedef uint64_t      elf64_off;    //   8 8 Unsigned file offset
typedef uint16_t      elf64_half;   //   2 2 Unsigned medium integer
typedef uint32_t      elf64_word;   //   4 4 Unsigned integer
typedef int32_t       elf64_sword;  //   4 4 Signed integer
typedef uint64_t      elf64_xword;  //   8 8 Unsigned long integer
typedef int64_t       elf64_sxword; //   8 8 Signed long integer

/* e_ident fields */
enum elf_ei_fields {
	EI_MAG0,		/* File identification */
	EI_MAG1,
	EI_MAG2,
	EI_MAG3,
	EI_CLASS,		/* File class */
	EI_DATA,		/* Data encoding */
	EI_VERSION,		/* File version */
	EI_OSABI,		/* OS/ABI identification */
	EI_ABIVERSION,	/* ABI version */
	EI_PAD,			/* Start of padding bytes */
	EI_NIDENT = 16, /* length of e_ident */
};

enum elf_class {
	ELFCLASS32 = 1,
	ELFCLASS64 = 2,
};

enum elf_data {
	ELFDATA2LSB = 1, /* Object file data structures are little-endian */
	ELFDATA2MSB = 2, /* Object file data structures are big-endian */
};


enum elf_osabi {
	ELFOSABI_NONE		= 0,			/* UNIX System V ABI */
	ELFOSABI_SYSV		= 0,			/* Alias.  */
	ELFOSABI_HPUX		= 1,			/* HP-UX */
	ELFOSABI_NETBSD		= 2,			/* NetBSD.  */
	ELFOSABI_GNU		= 3,			/* Object uses GNU ELF extensions.  */
	ELFOSABI_LINUX		= ELFOSABI_GNU, /* Compatibility alias.  */
	ELFOSABI_SOLARIS	= 6,			/* Sun Solaris.  */
	ELFOSABI_AIX		= 7,			/* IBM AIX.  */
	ELFOSABI_IRIX		= 8,			/* SGI Irix.  */
	ELFOSABI_FREEBSD	= 9,			/* FreeBSD.  */
	ELFOSABI_TRU64		= 10,			/* Compaq TRU64 UNIX.  */
	ELFOSABI_MODESTO	= 11,			/* Novell Modesto.  */
	ELFOSABI_OPENBSD	= 12,			/* OpenBSD.  */
	ELFOSABI_ARM_AEABI	= 64,			/* ARM EABI */
	ELFOSABI_ARM		= 97,			/* ARM */
	ELFOSABI_STANDALONE = 255,			/* Standalone (embedded) application */
};

enum elf_file_type {
	ET_NONE		= 0,		/*  No file type */
	ET_REL		= 1,		/*  Relocatable object file */
	ET_EXEC		= 2,		/*  Executable file */
	ET_DYN		= 3,		/*  Shared object file */
	ET_CORE		= 4,		/*  Core file */
	ET_LOOS		= 0xFE00,   /*  Environment-specific use */
	ET_HIOS		= 0xFEFF,
	ET_LOPROC	= 0xFF00,   /*  Processor-specific use */
	ET_HIPROC	= 0xFFFF,
};

#define EM_NONE   0  /* No machine */
#define EM_X86_64 62 /* AMD x86-64 architecture */


enum elf_section_type {
	SHT_NULL		  = 0,			/* Marks an unused section header SHT_PROGBITS 1 Contains
									   information defined by the program */
	SHT_PROGBITS	  = 1,          /* Contains information defined by the program */
	SHT_SYMTAB		  = 2,			/* Contains a linker symbol table */
	SHT_STRTAB		  = 3,			/* Contains a string table */
	SHT_RELA		  = 4,			/* Contains “Rela” type relocation entries */
	SHT_HASH		  = 5,			/* Contains a symbol hash table */
	SHT_DYNAMIC		  = 6,			/* Contains dynamic linking tables */
	SHT_NOTE		  = 7,			/* Contains note information */
	SHT_NOBITS		  = 8,			/* Contains uninitialized space; does not occupy any space
									   in the file */
	SHT_REL			  = 9,			/* Contains “Rel” type relocation entries */
	SHT_SHLIB		  = 10,			/* Reserved */
	SHT_DYNSYM		  = 11,			/* Contains a dynamic loader symbol table */
	SHT_INIT_ARRAY	  = 14,  		/* Array of constructors */
	SHT_FINI_ARRAY	  = 15,  		/* Array of destructors */
	SHT_PREINIT_ARRAY = 16,  		/* Array of pre-constructors */
	SHT_GROUP		  = 17,  		/* Section group */
	SHT_SYMTAB_SHNDX  = 18,  		/* Extended section indices */
	SHT_RELR		  = 19,         /* RELR relative relocations */
	SHT_NUM			  = 20,         /* Number of defined types.  */
	SHT_LOOS		  = 0x60000000,	/* Environment-specific use */
	SHT_HIOS		  = 0x6FFFFFFF,
	SHT_LOPROC		  = 0x70000000,	/* Processor-specific use */
	SHT_HIPROC		  = 0x7FFFFFFF,
};

enum elf_section_attribute {
	SHF_WRITE	  = 0x1,		/* Section contains writable data */
	SHF_ALLOC	  = 0x2,		/* Section is allocated in memory image of program */
	SHF_EXECINSTR = 0x4,		/* Section contains executable instructions */
	SHF_MASKOS	  = 0x0F000000, /* Environment-specific use */
	SHF_MASKPROC  = 0xF0000000, /* Processor-specific use */
};

enum elf_symbol_binding {
	STB_LOCAL  = 0,		/* Not visible outside the object file */
	STB_GLOBAL = 1,		/* Global symbol, visible to all object files */
	STB_WEAK   = 2,		/* Global scope, but with lower precedence than global symbols */
	STB_LOOS   = 10,	/* Environment-specific use */
	STB_HIOS   = 12,
	STB_LOPROC = 13,	/* Processor-specific use */
	STB_HIPROC = 15,
};

enum elf_symbol_type {
	STT_NOTYPE	= 0,	/*  No type specified (e.g., an absolute symbol) */
	STT_OBJECT	= 1,	/*  Data object */
	STT_FUNC	= 2,	/*  Function entry point */
	STT_SECTION = 3,	/*  Symbol is associated with a section */
	STT_FILE	= 4,	/*  Source file associated with the object file */
	STT_LOOS	= 10,	/*  Environment-specific use */
	STT_HIOS	= 12,
	STT_LOPROC	= 13,	/*  Processor-specific use */
	STT_HIPROC	= 15,
};

enum elf_segment_type {
	PT_NULL	   = 0,				/* Unused entry */
	PT_LOAD	   = 1,				/* Loadable segment */
	PT_DYNAMIC = 2,				/* Dynamic linking tables */
	PT_INTERP  = 3,				/* Program interpreter path name */
	PT_NOTE	   = 4,				/* Note sections */
	PT_SHLIB   = 5,				/* Reserved */
	PT_PHDR	   = 6,				/* Program header table */
	PT_LOOS	   = 0x60000000,	/* Environment-specific use */
	PT_HIOS	   = 0x6FFFFFFF,	/* */
	PT_LOPROC  = 0x70000000,	/* Processor-specific use */
	PT_HIPROC  = 0x7FFFFFFF,	/* */
};

enum elf_segment_attribute {
	PF_X		= 0x1,			/* Execute permission */
	PF_W		= 0x2,			/* Write permission */
	PF_R		= 0x4,			/* Read permission */
	PF_MASKOS	= 0x00FF0000,	/* These flag bits are reserved for environment-specific use */
	PF_MASKPROC = 0xFF000000,	/* These flag bits are reserved for processor-specific use */
};

enum elf_dynamic_table_entry {
	DT_NULL			= 0,			/* ignored | Marks the end of the dynamic array */
	DT_NEEDED		= 1,			/* d_val | The string table offset of the name of a needed library. */
	DT_PLTRELSZ		= 2,			/* d_val | Total size, in bytes, of the relocation entries associated
									   with the procedure linkage table. */
	DT_PLTGOT		= 3,			/* d_ptr | Contains an address associated with the linkage table. The
									   specific meaning of this field is processor-dependent. */
	DT_HASH			= 4,			/* d_ptr | Address of the symbol hash table, described below. */
	DT_STRTAB		= 5,			/* d_ptr | Address of the dynamic string table. */
	DT_SYMTAB		= 6,			/* d_ptr | Address of the dynamic symbol table. */
	DT_RELA			= 7,			/* d_ptr | Address of a relocation table with Elf64_Rela entries. */
	DT_RELASZ		= 8,			/* d_val | Total size, in bytes, of the DT_RELA relocation table. */
	DT_RELAENT		= 9,			/* d_val | Size, in bytes, of each DT_RELA relocation entry. */
	DT_STRSZ		= 10,			/* d_val | Total size, in bytes, of the string table. */
	DT_SYMENT		= 11,			/* d_val | Size, in bytes, of each symbol table entry. */
	DT_INIT			= 12,			/* d_ptr | Address of the initialization function. */
	DT_FINI			= 13,			/* d_ptr | Address of the termination function. */
	DT_SONAME		= 14,			/* d_val | The string table offset of the name of this shared object. */
	DT_RPATH		= 15,			/* d_val | The string table offset of a shared library search path string. */
	DT_SYMBOLIC		= 16,			/* ignored | The presence of this dynamic table entry modifies the
									   symbol resolution algorithm for references within the
									   library. Symbols defined within the library are used to
									   resolve references before the dynamic linker searches the
									   usual search path. */
	DT_REL			= 17,			/* d_ptr | Address of a relocation table with Elf64_Rel entries. */
	DT_RELSZ		= 18,			/* d_val | Total size, in bytes, of the DT_REL relocation table. */
	DT_RELENT		= 19,			/* d_val | Size, in bytes, of each DT_REL relocation entry. */
	DT_PLTREL		= 20,			/* d_val | Type of relocation entry used for the procedure linkage table.
									   The d_val member contains either DT_REL or DT_RELA. */
	DT_DEBUG		= 21,			/* d_ptr | Reserved for debugger use. */
	DT_TEXTREL		= 22,			/* ignored | The presence of this dynamic table entry signals that the
									   relocation table contains relocations for a non-writable segment. */
	DT_JMPREL		= 23,			/* d_ptr |  Address of the relocations associated with the
									   procedure linkage table. */
	DT_BIND_NOW		= 24,			/* ignored The presence of this dynamic table entry signals that the
									   dynamic loader should process all relocations for this object
									   before transferring control to the program. */
	DT_INIT_ARRAY	= 25,			/* d_ptr | Pointer to an array of pointers to initialization functions. */
	DT_FINI_ARRAY	= 26,			/* d_ptr | Pointer to an array of pointers to termination functions. */
	DT_INIT_ARRAYSZ = 27,			/* d_val | Size, in bytes, of the array of initialization functions. */
	DT_FINI_ARRAYSZ = 28,			/* d_val | Size, in bytes, of the array of termination functions. */
	DT_LOOS			= 0x60000000,	/* Defines a range of dynamic table tags that are reserved
									   for environment-specific use. */
	DT_HIOS			= 0x6FFFFFFF,
	DT_LOPROC		= 0x70000000,	/* Defines a range of dynamic table tags that are
									   reserved for processor-specific use. */
	DT_HIPROC		= 0x7FFFFFFF,
};

enum elf_amd64_system_v_rela_type {
	R_X86_64_NONE			 = 0,   /* none none */
	R_X86_64_64				 = 1,   /* word64 S + A */
	R_X86_64_PC32			 = 2,   /* word32 S + A - P */
	R_X86_64_GOT32			 = 3,   /* word32 G + A */
	R_X86_64_PLT32			 = 4,   /* word32 L + A - P */
	R_X86_64_COPY			 = 5,   /* none none */
	R_X86_64_GLOB_DAT		 = 6,   /* word64 S */
	R_X86_64_JUMP_SLOT		 = 7,   /* word64 S */
	R_X86_64_RELATIVE		 = 8,   /* word64 B + A */
	R_X86_64_GOTPCREL		 = 9,   /* word32 G + GOT + A - P */
	R_X86_64_32				 = 10,  /* word32 S + A */
	R_X86_64_32S			 = 11,  /* word32 S + A */
	R_X86_64_16				 = 12,  /* word16 S + A */
	R_X86_64_PC16			 = 13,  /* word16 S + A - P */
	R_X86_64_8				 = 14,  /* word8 S + A */
	R_X86_64_PC8			 = 15,  /* word8 S + A - P */
	R_X86_64_DTPMOD64		 = 16,  /* word64 */
	R_X86_64_DTPOFF64		 = 17,  /* word64 */
	R_X86_64_TPOFF64		 = 18,  /* word64 */
	R_X86_64_TLSGD			 = 19,  /* word32 */
	R_X86_64_TLSLD			 = 20,  /* word32 */
	R_X86_64_DTPOFF32		 = 21,  /* word32 */
	R_X86_64_GOTTPOFF		 = 22,  /* word32 */
	R_X86_64_TPOFF32		 = 23,  /* word32 */
	R_X86_64_PC64			 = 24,  /* word64 S + A - P */
	R_X86_64_GOTOFF64		 = 25,  /* word64 S + A - GOT */
	R_X86_64_GOTPC32		 = 26,  /* word32 GOT + A - P */
	R_X86_64_SIZE32			 = 32,  /* word32 Z + A */
	R_X86_64_SIZE64			 = 33,  /* word64 Z + A */
	R_X86_64_GOTPC32_TLSDESC = 34,  /* word32 */
	R_X86_64_TLSDESC_CALL	 = 35,  /* none */
	R_X86_64_TLSDESC		 = 36,  /* word64×2 */
	R_X86_64_IRELATIVE		 = 37,  /* word64 | indirect (B + A) */
};

typedef struct elf64_header {
	elf64_ubyte e_ident[EI_NIDENT]; /* ELF identification */
	elf64_half  e_type;			    /* Object file type */
	elf64_half  e_machine;		    /* Machine type */
	elf64_word  e_version;		    /* Object file version */
	elf64_addr  e_entry;			/* Entry point address */
	elf64_off   e_phoff;			/* Program header offset */
	elf64_off   e_shoff;			/* Section header offset */
	elf64_word  e_flags;			/* Processor-specific flags */
	elf64_half  e_ehsize;		    /* ELF header size */
	elf64_half  e_phentsize;		/* Size of program header entry */
	elf64_half  e_phnum;			/* Number of program header entries */
	elf64_half  e_shentsize;		/* Size of section header entry */
	elf64_half  e_shnum;			/* Number of section header entries */
	elf64_half  e_shstrndx;		    /* Section name string table index */
} Elf64_header;

typedef struct elf64_section_header {
	elf64_word	sh_name;		/* Section name */
	elf64_word	sh_type;		/* Section type */
	elf64_xword sh_flags;		/* Section attributes */
	elf64_addr	sh_addr;		/* Virtual address in memory */
	elf64_off	sh_offset;		/* Offset in file */
	elf64_xword sh_size;		/* Size of section */
	elf64_word	sh_link;		/* Link to other section */
	elf64_word	sh_info;		/* Miscellaneous information */
	elf64_xword sh_addralign;	/* Address alignment boundary */
	elf64_xword sh_entsize;		/* Size of entries, if section has table */
} Elf64_section_header;

typedef struct elf64_program_header {
	elf64_word  p_type;		/* Type of segment */
	elf64_word  p_flags;	/* Segment attributes */
	elf64_off   p_offset;	/* Offset in file */
	elf64_addr  p_vaddr;	/* Virtual address in memory */
	elf64_addr  p_paddr;	/* Reserved */
	elf64_xword p_filesz;	/* Size of segment in file */
	elf64_xword p_memsz;	/* Size of segment in memory */
	elf64_xword p_align;	/* Alignment of segment */
} Elf64_program_header;

typedef struct elf64_symbol {
	elf64_word  st_name;	/* Symbol name */
	elf64_ubyte st_info;	/* Type and Binding attributes */
	elf64_ubyte st_other;	/* Reserved */
	elf64_half  st_shndx;	/* Section table index */
	elf64_addr  st_value;	/* Symbol value */
	elf64_xword st_size;	/* Size of object (e.g., common) */
} Elf64_symbol;

typedef struct elf64_rel {
	elf64_addr  r_offset;	/* Address of reference */
	elf64_xword r_info;		/* Symbol index and type of relocation */
} Elf64_rel;

typedef struct elf64_rela {
	elf64_addr   r_offset;	/* Address of reference */
	elf64_xword  r_info;	/* Symbol index and type of relocation */
	elf64_sxword r_addend;	/* Constant part of expression */
} Elf64_rela;

typedef struct elf64_dyn {
	elf64_sxword d_tag;
	union {
		elf64_xword d_val;
		elf64_addr  d_ptr;
	} d_un;
} Elf64_dyn;

typedef struct elf64_file_contents {
	struct elf64_header header;
	elf64_ubyte rest[];
} Elf64_file_contents;

typedef struct elf64_file {
	Elf64_file_contents *contents;
	Elf64_section_header *shtbl;
	Elf64_program_header *phtbl;
	const char *strtbl;
	int  shnum;
	int  phnum;
	bool is_mmaped;
} Elf64_file;

/* Special section indecies */
#define SHN_UNDEF  0        /* Used to mark an undefined or meaningless section reference */
#define SHN_LOPROC 0xFF00   /* Processor-specific use */
#define SHN_HIPROC 0xFF1F
#define SHN_LOOS   0xFF20   /* Environment-specific use */
#define SHN_HIOS   0xFF3F
#define SHN_ABS    0xFFF1   /* Indicates that the corresponding reference is an absolute value */
#define SHN_COMMON 0xFFF2   /* Indicates a symbol that has been declared as a
							   common block (Fortran COMMON or C tentative declaration) */

#define ELF64_R_SYM(i)     ((i) >> 32)
#define ELF64_R_TYPE(i)    ((i) & 0xffffffffL)
#define ELF64_R_INFO(s, t) (((elf64_xword)(s) << 32) + ((elf64_xword)(t) & 0xffffffffL))

/*
 *    ELF Header
 *    Program header table
 *    Section 1
 *    Section 2
 *    . . .
 *    Section n
 *    Section header table
 */

static const elf64_ubyte ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};

#if 0
static inline unsigned long elf64_hash(const unsigned char *name);
static inline void *elf64_file_offset(Elf64_file *f, elf64_off offset);
static inline Elf64_header *elf64_file_header(Elf64_file *f);
static inline Elf64_program_header *elf64_file_program_header_table(Elf64_file *f);
static inline int elf64_file_program_header_count(Elf64_file *f);
static inline Elf64_section_header *elf64_file_section_header_table(Elf64_file *f);
static inline int elf64_file_section_header_count(Elf64_file *f);
static inline Elf64_section_header *elf64_file_string_table_section(Elf64_file *f);
static inline const char *elf64_file_string_table(Elf64_file *f);
static inline const char *elf64_file_section_contents(Elf64_file *f, Elf64_section_header *sh);
static inline const char *elf64_section_name(Elf64_file *f, Elf64_section_header *sh);

bool
elf64_validate_e_ident(Elf64_header *hdr)
{
	if ((*(uint32_t *)hdr->e_ident ^ *(uint32_t *)ELF_MAGIC) != 0) return false;
	switch (hdr->e_ident[EI_CLASS]) {
	case ELFCLASS32:          FAILWITH("[ERROR] ELFCLASS32 is unsupported.");
	case ELFCLASS64:          break;
	default:                  FAILWITH("[ERROR] Invalid ELFCLASS.");
	}
	switch (hdr->e_ident[EI_DATA]) {
	case ELFDATA2LSB:         break;
	case ELFDATA2MSB:         FAILWITH("[ERROR] ELFDATA2MSB is unsupported.");
	default:                  FAILWITH("[ERROR] Invalid EI_DATA.");
	}
	switch ((enum elf_osabi)hdr->e_ident[EI_OSABI]) {
	case ELFOSABI_SYSV:       break;
	case ELFOSABI_GNU:        break;
	case ELFOSABI_SOLARIS:	  FAILWITH("[ERROR] ELFOSABI_SOLARIS is unsupported.");
	case ELFOSABI_AIX:		  FAILWITH("[ERROR] ELFOSABI_AIX is unsupported.");
	case ELFOSABI_IRIX:		  FAILWITH("[ERROR] ELFOSABI_IRIX is unsupported.");
	case ELFOSABI_NETBSD:	  FAILWITH("[ERROR] ELFOSABI_NETBSD is unsupported.");
	case ELFOSABI_FREEBSD:	  FAILWITH("[ERROR] ELFOSABI_FREEBSD is unsupported.");
	case ELFOSABI_TRU64:	  FAILWITH("[ERROR] ELFOSABI_TRU64 is unsupported.");
	case ELFOSABI_MODESTO:	  FAILWITH("[ERROR] ELFOSABI_MODESTO is unsupported.");
	case ELFOSABI_OPENBSD:	  FAILWITH("[ERROR] ELFOSABI_OPENBSD is unsupported.");
	case ELFOSABI_ARM_AEABI:  FAILWITH("[ERROR] ELFOSABI_ARM_AEABI is unsupported.");
	case ELFOSABI_ARM:		  FAILWITH("[ERROR] ELFOSABI_ARM is unsupported.");
	case ELFOSABI_HPUX:       FAILWITH("[ERROR] ELFOSABI_HPUX is unsupported.");
	case ELFOSABI_STANDALONE: FAILWITH("[ERROR] ELFOSABI_STANDALONE is unsupported.");
	default:                  FAILWITH("[ERROR] Invalid EI_OSABI: %x.", hdr->e_ident[EI_OSABI]);
	}
	return true;
}

bool
elf64_fread_header(FILE *file, Elf64_header *hdr)
{
	if (fread(hdr, 1, sizeof(*hdr), file) != sizeof(*hdr)) return false;
	if (!elf64_validate_e_ident(hdr)) return false;
	printf("e_type = %x\n", hdr->e_type);
	printf("e_machine = %x\n", hdr->e_machine);
	printf("e_version = %x\n", hdr->e_version);
	FAILWITH("TODO");
	return true;
}

bool
elf64_map_file(const char *filename, Elf64_file *e)
{
	int fd = -1;
	struct stat stat_buf = {0};
	void *data = NULL;
	if ((fd = open(filename, O_RDONLY)) == -1)
		goto map_fail1;
	if (fstat(fd, &stat_buf) == -1)
		goto map_fail1;
	if ((data = mmap(NULL, stat_buf.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
		goto map_fail0;
	close(fd);
	e->contents = data;
	e->is_mmaped = true;
	e->shnum  = elf64_file_section_header_count(e);
	e->shtbl  = elf64_file_section_header_table(e);
	e->phnum  = elf64_file_program_header_count(e);
	e->phtbl  = elf64_file_program_header_table(e);
	e->strtbl = elf64_file_string_table(e);
	return true;
map_fail0:
	close(fd);
map_fail1:
	return false;
}

static inline unsigned long
elf64_hash(const unsigned char *name)
{
	unsigned long h = 0, g;
	while (*name) {
		h = (h << 4) + *name++;
		if ((g = h & 0xf0000000))
			h ^= g >> 24;
		h &= 0x0fffffff;
	}
	return h;
}

static inline void *
elf64_file_offset(Elf64_file *f, elf64_off offset)
{
	return (elf64_ubyte *)f->contents + offset;
}

static inline Elf64_header *
elf64_file_header(Elf64_file *f)
{
	return &f->contents->header;
}

static inline Elf64_program_header *
elf64_file_program_header_table(Elf64_file *f)
{
	return elf64_file_offset(f, elf64_file_header(f)->e_phoff);
}

static inline int
elf64_file_program_header_count(Elf64_file *f)
{
	return elf64_file_header(f)->e_phnum;
}

static inline Elf64_section_header *
elf64_file_section_header_table(Elf64_file *f)
{
	return elf64_file_offset(f, elf64_file_header(f)->e_shoff);
}

static inline int
elf64_file_section_header_count(Elf64_file *f)
{
	return elf64_file_header(f)->e_shnum;
}

static inline Elf64_section_header *
elf64_file_string_table_section(Elf64_file *f)
{
	return &elf64_file_section_header_table(f)[elf64_file_header(f)->e_shstrndx];
}

static inline const char *
elf64_file_section_contents(Elf64_file *f, Elf64_section_header *sh)
{
	return elf64_file_offset(f, sh->sh_offset);
}

static inline const char *
elf64_file_string_table(Elf64_file *f)
{
	return elf64_file_section_contents(f, elf64_file_string_table_section(f));
}

static inline const char *
elf64_section_name(Elf64_file *f, Elf64_section_header *sh)
{
	return elf64_file_string_table(f) + sh->sh_name;
}

#endif
