#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <mips/tlb.h>
#include <tlb.h>
#include <vm.h>
#include <spl.h>

/* insert a record into the tlb */
/* insert_tlb
 * massages the vpn and vpn such that we can insert them in the tlb
 */
void insert_tlb(int vaddr, int ppn)
{
        int spl = splhigh();
        vaddr &= PAGE_FRAME;  /* mask the vpn */
        tlb_random(vaddr, ppn);
        splx(spl);
}

/* flush_tlb
 * flushes the tlb for context switching 
 */
void flush_tlb()
{
        int i, spl;
        spl = splhigh();
        for (i=0; i<NUM_TLB; i++) {
                tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
        }
        splx(spl);
}

/* replace a ppn entry in the tlb with something else */
void replace_tlb(int vaddr, int ppn)
{
        vaddr &= PAGE_FRAME;
 
        /* get index where the vaddr is */
        int index = tlb_probe(vaddr, 0);
        if (index < 0) {
                panic("lol");
        }

        /* replace the ppn with something else */
        tlb_write(vaddr, ppn, index);
}
