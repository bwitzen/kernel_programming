#ifndef JOS_KERN_VMA_H
#define JOS_KERN_VMA_H

#include <inc/types.h>
#include <inc/elf.h>
#include <kern/pmap.h>
#include <kern/env.h>

#define VMA_LENGTH 128
#define VMA_DEBUG 1
#define MAP_POPULATE 1

#define VMA_MERGE // disable to not merge
#define BONUS_LAB5 // disable to disable shmem
#define VMA_RETAIN_PHYS 0
#define VMA_DESTROY_PHYS 1

enum {
    VMA_UNUSED = 0,
    VMA_ANON,
    VMA_BINARY,
};

struct vma {
    int type;
    void *va;
    size_t len;
    int perm;
    struct elf_proghdr *ph;
    uint8_t *bin;
#ifdef BONUS_LAB5
    int shmem_key;
#endif
};

// interface
void vma_init(struct env *e);
void vma_rmv(struct env *e, void *va, size_t len, int destrucive);
struct vma *vma_new(struct env *e, void *va, size_t len, int perm,
             struct elf_proghdr *ph, uint8_t *bin);
int vma_seek(struct env *e, void *va);
void vma_print(struct env *e);
void *vma_find_mem(struct env *e, size_t len);

// private helpers
static int vma_find_free_slot(struct env *e);
static int vma_find_slot_by_va(struct env *e, void *va);
void vma_merge(struct env *e);

#endif // JOS_KERN_VMA_H
