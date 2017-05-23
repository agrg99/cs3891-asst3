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
void insert_tlb(int vpn, int ppn)
{
        vpn = (uint32_t) ADDR_TO_PN(vpn);  /* mask the vpn */
        ppn = (uint32_t) ppn & 0xFFFFF;    /* clean the ppn just incase */
        tlb_random(vpn, ppn);
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
