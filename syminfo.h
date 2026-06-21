#pragma once

/* opaque handle to hashmap */
typedef uintptr_t Syminfo;

typedef struct symbol_info {
	struct symbol_info     *next;
	struct symtbl_entry    *si;
	struct type_definition *ti;
} *Syminfo_list;

typedef struct {
	uint32_t count, index;
	Syminfo si;
} Syminfo_iter;

typedef struct {
	struct strview name;
	Syminfo_list info_list;
} Syminfo_pair;

#define syminfo_add(si, key, val)								\
	_Generic((val),												\
			 struct symtbl_entry *: syminfo_add_symtbl_entry,	\
			 struct type_definition *: syminfo_add_type_entry)	\
	(si, key, val)

#define syminfo_iter(S) ((Syminfo_iter){.si=(S)})

KC_PUBLIC Syminfo syminfo_create(void);
KC_PUBLIC void syminfo_add_symtbl_entry(Syminfo *si, struct strview key, struct symtbl_entry *val);
KC_PUBLIC void syminfo_add_type_entry(Syminfo *si, struct strview key, struct type_definition *val);
KC_PUBLIC Syminfo_list syminfo_lookup(Syminfo si, struct strview key);
KC_PUBLIC bool syminfo_iter_next(Syminfo_iter *iter, Syminfo_pair *pair);
