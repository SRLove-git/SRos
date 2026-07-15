#ifndef ELF_H
#define ELF_H

#include "types.h"

/* ELF magic numbers */
#define ELF_MAGIC0      0x7F
#define ELF_MAGIC1      'E'
#define ELF_MAGIC2      'L'
#define ELF_MAGIC3      'F'

#define EI_NIDENT       16

/* e_ident indices */
#define EI_MAG0         0
#define EI_MAG1         1
#define EI_MAG2         2
#define EI_MAG3         3
#define EI_CLASS        4
#define EI_DATA         5
#define EI_VERSION      6
#define EI_OSABI        7
#define EI_ABIVERSION   8

/* e_ident[EI_CLASS] */
#define ELFCLASS32      1

/* e_ident[EI_DATA] */
#define ELFDATA2LSB     1

/* e_type */
#define ET_EXEC         2

/* e_machine */
#define EM_386          3

/* p_type */
#define PT_NULL         0
#define PT_LOAD         1

/* p_flags */
#define PF_X            1
#define PF_W            2
#define PF_R            4

/* 32-bit ELF header */
typedef struct {
    u8      e_ident[EI_NIDENT];
    u16     e_type;
    u16     e_machine;
    u32     e_version;
    u32     e_entry;
    u32     e_phoff;
    u32     e_shoff;
    u32     e_flags;
    u16     e_ehsize;
    u16     e_phentsize;
    u16     e_phnum;
    u16     e_shentsize;
    u16     e_shnum;
    u16     e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

/* 32-bit program header */
typedef struct {
    u32     p_type;
    u32     p_offset;   /* offset in file */
    u32     p_vaddr;    /* virtual address to load at */
    u32     p_paddr;    /* physical address (unused) */
    u32     p_filesz;   /* size in file */
    u32     p_memsz;    /* size in memory */
    u32     p_flags;    /* permissions */
    u32     p_align;    /* alignment */
} __attribute__((packed)) Elf32_Phdr;

/* Validate if the given data is a valid 32-bit ELF executable */
static inline int elf_check_file(const Elf32_Ehdr *ehdr)
{
    if (!ehdr) return 0;
    if (ehdr->e_ident[EI_MAG0] != ELF_MAGIC0) return 0;
    if (ehdr->e_ident[EI_MAG1] != ELF_MAGIC1) return 0;
    if (ehdr->e_ident[EI_MAG2] != ELF_MAGIC2) return 0;
    if (ehdr->e_ident[EI_MAG3] != ELF_MAGIC3) return 0;
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) return 0;
    if (ehdr->e_ident[EI_DATA]  != ELFDATA2LSB) return 0;
    if (ehdr->e_type    != ET_EXEC)   return 0;
    if (ehdr->e_machine != EM_386)    return 0;
    if (ehdr->e_phnum   == 0)         return 0;
    return 1;
}

#endif /* ELF_H */
