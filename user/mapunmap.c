#include <inc/assert.h>
#include <inc/lib.h>

#define TILE_SIZE	(4096)
#define SWEEP_SPACE	(6 * TILE_SIZE)
#define STRIPE_SPACE    (2 * TILE_SIZE)
#define TILE(I)		(I * TILE_SIZE)

void umain(int argc, char **argv)
{
    void *va = NULL;

    va = sys_vma_create(SWEEP_SPACE, PERM_W, MAP_POPULATE);

    *((uint32_t *)va) = 0x7777;

    assert(0 == sys_vma_destroy((void *)(va + TILE(2)), STRIPE_SPACE));

    *((uint32_t *)(va + TILE(0))) = 0x02020202;
    *((uint32_t *)(va + TILE(5))) = 0x04040404;
    *((uint32_t *)(va + TILE(4))) = 0x04040404;
    *((uint32_t *)(va + TILE(1))) = 0x08080808;

    cprintf("0+5 == 4+1\n");

    *((uint32_t *)(va + TILE(2))) = 0x0BADB10C;
    panic("SHOULD HAVE TRAPPED!!!");
}
