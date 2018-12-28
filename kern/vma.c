#include <kern/vma.h>

/*
 * Initializes the VMA chain for an environment.
 * Panics on failure.
 */
void vma_init(struct env *e) {
    // allocate a page
    struct page_info *pp = page_alloc(ALLOC_ZERO);
    if (!pp) panic("Could not create VMA structure for env %x\n", e->env_id);
    pp->pp_ref += 1;

    // store reference
    e->env_vmas = page2kva(pp);
}

/*
 * Inserts a new VMA block into the chain.
 * Returns -1 on failure
 */
struct vma *vma_new(struct env *e, void *va, size_t len, int perm,
             struct elf_proghdr *ph, uint8_t *bin)
{
    assert(len > 0);

    // find VMA block
    int slot = vma_find_free_slot(e);
    if (slot < 0) return (void *)-1;
    struct vma *v = &e->env_vmas[slot];

    // fill it in
    v->type = (bin == NULL) ? VMA_ANON : VMA_BINARY;
    v->va   = va;
    v->len  = len;
    v->perm = perm | PTE_U;
    v->ph   = ph;
    v->bin  = bin;

    // perform one round of VMA merging
#ifdef VMA_MERGE
    vma_merge(e);
#endif

    return v;
}

/*
 * Remove a VMA block from the chain.
 * If destructive is set, also removes the physical memory. If its cleared, it
 * doesn't remove the physical memory. This is used by VMA merging and should
 * never be used for another purpose.
 */
void vma_rmv(struct env *e, void *va, size_t len, int destructive) {
    int slot = vma_find_slot_by_va(e, va);

    void *rmv_start = va, *rmv_end = va + len;

    // cannot find
    if (slot == -1) {
        cprintf("vma_rmv called on invalid slot??\n");
        return;
    }

    if (va + len > e->env_vmas[slot].va + e->env_vmas[slot].len)
        panic("vma_rmv len too large");

    if (e->env_vmas[slot].va == va) {
        // va+len collide (destroy)
        if (e->env_vmas[slot].len == len) {
            e->env_vmas[slot].type = VMA_UNUSED;
        }
        // va collides, but len doesn't (move left boundery =>)
        else {
            e->env_vmas[slot].va += len;
            e->env_vmas[slot].len -= len;
        }
    }
    else {
        // end collides, but va doesn't (move right boundary <=)
        if (va + len == e->env_vmas[slot].va + e->env_vmas[slot].len) {
            e->env_vmas[slot].len = va - e->env_vmas[slot].va;
        }
        // no collision, the freed region is in middle
        else {
            size_t left_len  = va - e->env_vmas[slot].va;
            void *right_vma  = (void *) ((size_t) va + len);
            size_t right_len = e->env_vmas[slot].len - left_len - len;

            // (left) shorten existing vma
            e->env_vmas[slot].len = left_len;

            // (right) spawn new vma
            vma_new(e, right_vma, right_len,
                    e->env_vmas[slot].perm,
                    e->env_vmas[slot].ph, // can you even split binaries?
                    e->env_vmas[slot].bin);
        }
    }

    // remove physpages
    if (destructive)
        for (void *i = rmv_start; i < rmv_end; i++)
            page_remove(e->env_pgdir, i);
}

/*
 * Calling this function will perform one round of VMA merging. Note that, due
 * to the merge, new merging oportunities might arise.
 */
void vma_merge(struct env *e) {

    for (size_t outer = 0; outer < VMA_LENGTH; outer++) {
        struct vma *o = &e->env_vmas[outer];
        if (o->type == VMA_UNUSED) continue;

        void *o_left = o->va;
        void *o_right = o->va + o->len;

        for (size_t inner = 0; inner < VMA_LENGTH; inner++) {
            struct vma *i = &e->env_vmas[inner];
            if (i->type == VMA_UNUSED || inner == outer) continue;
            void *i_left = i->va;
            void *i_right = i->va + i->len;

            if (o->type == i->type && o->perm == i->perm) {
                if (i_left == o_right) {
                    int len = i->len;
                    vma_rmv(e, i->va, i->len, VMA_RETAIN_PHYS);
                    o->len += len;
                    break;
                }
                else if (i_right == o_left) {
                    int len = o->len;
                    vma_rmv(e, o->va, o->len, VMA_RETAIN_PHYS);
                    i->len += len;
                    break;
                }
            }
        }
    }
}

/*
 * Helper function. Finds first free slot in VMA chain and returns index of that
 * slot. Returns -1 if no slots available.
 */
static int vma_find_free_slot(struct env *e) {
    for (size_t i = 0; i < VMA_LENGTH; i++)
        if (e->env_vmas[i].type == VMA_UNUSED)
            return i;
    return -1;
}

/**
 * Returns index to the slot in VMA that contains va
 * Returns VMA_NOSLOT if va not in VMA
 * addresses in VMA are assumed to not overlap
 */
static int vma_find_slot_by_va(struct env *e, void *va) {
    for (int i = 0; i < VMA_LENGTH; i++) {
        if (e->env_vmas[i].type != VMA_UNUSED
            && e->env_vmas[i].va <= va
            && e->env_vmas[i].va + e->env_vmas[i].len > va)
            return i;
    }
    return -1;
}

/*
 * Helper function. Returns the first free virtual memory address that could fit
 * the request. This function will reuse low memory addresses as they are freed.
 */
void *vma_find_mem(struct env *e, size_t len) {
    void *target = (void *) UTEXT;
    len = ROUNDUP(len, PGSIZE);
    int consec = 0;

    // search page by page...
    for (void *current = (void *) UTEXT; current < (void *) USTACKTOP; current += PGSIZE) {
        // ... check if this page occupied
        for (size_t i = 0; i < VMA_LENGTH; i++) {
            if (e->env_vmas[i].type != VMA_UNUSED) {
                struct vma *v = &e->env_vmas[i];
                if (v->va <= current && v->va + v->len > current) {
                    // page occupied
                    target = current + PGSIZE;
                    consec = 0;
                }
            }
        }

        // all vma's checked, this page is free
        consec += PGSIZE;

        if (consec > len)
            return target;
    }

    // no space found
    return NULL;
}

/*
 * Searches for a VA in the VMA chain. If found, attempts to page in.
 * Returns 0 on success, -1 on failure.
 */
int vma_seek(struct env *e, void *va) {
    for (size_t i = 0; i < VMA_LENGTH; i++) {
        if (e->env_vmas[i].type != VMA_UNUSED &&
            e->env_vmas[i].va <= va &&
            e->env_vmas[i].va + e->env_vmas[i].len > va)
        {
            return i;
        }
    }
    return -1;
}

/*
 * Debug function. Prints the VMA chain for an environment.
 * Note that empty entries are not printed.
 */
void vma_print(struct env *e) {
    size_t count_used = 0, count_vacant = 0;
    cprintf(">> ------------------------------------------------\n");
    for (size_t i = 0; i < VMA_LENGTH; i++) {
        struct vma v = e->env_vmas[i];
        if (v.type == VMA_UNUSED) {
            count_vacant += 1;
        }
        else {
            count_used += 1;
            cprintf(">> %x [%03u]: ", e->env_id, i);
            switch (v.type) {
                case VMA_ANON:   cprintf("ANON "); break;
                case VMA_BINARY: cprintf("BIN  "); break;
                default:         cprintf("???? "); break;
            }
            cprintf("| Perm: ");
            switch (v.perm) {
                case 1:  cprintf("  E x1 "); break;
                case 2:  cprintf(" W  x2 "); break;
                case 3:  cprintf(" WE x3 "); break;
                case 4:  cprintf("R   x4 "); break;
                case 5:  cprintf("R E x5 "); break;
                case 6:  cprintf("RW  x6 "); break;
                case 7:  cprintf("RWE x7 "); break;
                default: cprintf("?????? "); break;
            }
            cprintf("| %x - %x (%x)\n", v.va, v.va + v.len, v.len);
        }
    }
    cprintf(">> Summary: %u used, %u vacant.\n", count_used, count_vacant);
    cprintf(">> ------------------------------------------------\n");
}
