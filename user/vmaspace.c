#include <inc/lib.h>
#include <inc/mmu.h>
#include <inc/assert.h>

#define CODE_SIZE	(7*PGSIZE)
#define STACK_SIZE	(3*PGSIZE)
#define ALLOC_FIRST	(UTOP - 0x00800000 - (CODE_SIZE + STACK_SIZE))
#define ALLOC_SECOND	(0x00a00000)
#define ALLOC_THIRD	(0x00900000)
#define MAP_FAILED	((void *)-1)

void umain(int argc, char **argv)
{
    void *va = NULL, *vb = NULL;

    /* A huge vma allocation */
    va = sys_vma_create(ALLOC_FIRST, PERM_W, 0);
    assert(MAP_FAILED != va);
    cprintf("First alloc succeeded. Size:%08x va:%08x\n",
            ALLOC_FIRST, (uint32_t)va);

    *((uint32_t *)va) = 0x01010101;
    *((uint32_t *)(((uint32_t)va + ALLOC_FIRST)/2))             = 0x02020202;
    *((uint32_t *)((uint32_t)va + ALLOC_FIRST - PGSIZE + 64))   = 0x03030303;
 
    /* Out of virtual memory space? */
    vb = sys_vma_create(ALLOC_SECOND, PERM_W, 0);
    assert(MAP_FAILED == vb);
    cprintf("Second alloc failed. Size:%08x\n", ALLOC_SECOND);

    /* Deallocate */
    assert(0 == sys_vma_destroy((void *)((uint32_t)va + (ALLOC_FIRST/2)),
                                ALLOC_THIRD));
    cprintf("[va: %08x : %08x]\n", (uint32_t)va, *((uint32_t *)va));;
    cprintf("[va: %08x : %08x]\n",
             (uint32_t)(va + ALLOC_FIRST - PGSIZE + 64), 
             *((uint32_t*)((uint32_t)va + ALLOC_FIRST - PGSIZE + 64)));;

    *((uint32_t *)((uint32_t)va + ALLOC_FIRST - PGSIZE + 64)) = 0x04040404;

    vb = sys_vma_create(ALLOC_THIRD, PERM_W, 0);
    assert(MAP_FAILED != va);
    cprintf("Third alloc succeeded. Size:%08x\n", ALLOC_THIRD);

    cprintf("VMA space check succeeded.\n"); 
    return;    
}
