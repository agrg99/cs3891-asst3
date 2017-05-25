#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <tlb.h>
#include <mips/tlb.h>
#include <proc.h>
#include <current.h>

/* define static methods */
static uint32_t hpt_hash(struct addrspace *as, vaddr_t faultaddr);
static struct page_entry * search_hpt(struct addrspace *as, vaddr_t addr);
static struct page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr);

/* The following hash function will combine the address of the struct
 * addrspace and faultaddr address to reduce hash collisions between processes
 * (processes using similar address ranges). */
        static uint32_t
hpt_hash(struct addrspace *as, vaddr_t faultaddr)
{
        uint32_t index;
        //kprintf("faultaddr >> PAGEBITS = %d\n", faultaddr >> PAGE_BITS);
        index = (((uint32_t )as) ^ (faultaddr >> PAGE_BITS)) % hpt_size;
        return index;
}

/* bootstrap the frame and page table here. We can use kmalloc because 
 * alloc_kpages still uses the stealmem function. */
        void 
vm_bootstrap(void)
{
        frametable_init();

        /* create the page table */
        kprintf("time to setup page table\n");
        kprintf("[*] vm_boostrap: hpt_size is %d\n", hpt_size);

        unsigned int i;
        /* init the page table */
        for(i = 0; i < hpt_size; i++) {
                hpt[i] = NULL;
        }
}

/* vm_fault
 * we update the tlb with the ppn
 */
        int
vm_fault(int faulttype, vaddr_t faultaddress)
{
        struct page_entry *pe;
        struct addrspace *as;

        switch (faulttype) {
                case VM_FAULT_READ:
                case VM_FAULT_WRITE:
                        break;
                case VM_FAULT_READONLY:
                        return EFAULT;
                default:
                        return EINVAL;
        }

        /* get as then do sanity check */
        as = proc_getas();
        if (!curproc || !hpt || !as) {
                return EFAULT;
        }

        /* check if request was to a valid region */
        if (!region_type(as, faultaddress)){
                return EFAULT;
        }

        /* get page entry */
        pe = search_hpt(as, faultaddress);

        //kprintf("did we find pe? %d\n", (pe!=NULL) ? 1 : 0 );

        if (pe){ // && GET_PAGE_PRES(pe->pe_flags)) { /* if in frame table */
                //   pte->flag has pt_r?   |
                //      return EFAULT;     |
                //   else
                /*  if GET_PAGE_MOD(pe->pe_flags) {
                //         load from swap
                } else { 
                //         load from elf
                } */

        } else { /* not in frame table */
                //    if (region_type(as, faultaddress) == SEG_CODE) {
                //  load from elf
                //return -1;
                //    } else {
                /* create and insert the page entry */
                pe = insert_hpt(as, faultaddress);
                //   }
        }

        /* mask the ppn ready for tlb store */
        uint32_t ppn = (uint32_t) PN_TO_ADDR(pe->pe_ppn);
        //kprintf("vpn checked: %x\n", ADDR_TO_PN(faultaddress));
        //kprintf("ppn inserted under: %x\n\n", pe->pe_ppn);
        ppn = ppn | TLBLO_VALID | TLBLO_DIRTY; /* set valid bit */ 
        pe->pe_flags = SET_PAGE_REF(pe->pe_flags);  /* set referenced */
        insert_tlb(faultaddress, ppn);       /* load tlb */

        return 0;
}

/* insert_hpt
 * insert a page entry into the hpt, if the record exists then linear probing
 * is used to find an empty slot.
 */
        static struct
page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr)
{
        vaddr_t n_frame = alloc_kpages(1);
        struct page_entry *n_pe = kmalloc(sizeof(struct page_entry));
        uint32_t ppn = ADDR_TO_PN(KVADDR_TO_PADDR(n_frame));
        uint32_t pt_hash = hpt_hash(as, vaddr);
        struct page_entry *pe = hpt[pt_hash];
        
        /* set up new page */
        n_pe->pe_proc = (uint32_t) as;
        n_pe->pe_ppn = ppn;
        n_pe->pe_flags = SET_PAGE_PROT(0, PROT_RW);
        n_pe->pe_flags = SET_PAGE_PRES(n_pe->pe_flags);
        n_pe->pe_next = NULL;
        /* check if this record is taken */
        if (pe != NULL) {
                /* loop until we find the last of the chain */
                while (pe->pe_next != NULL) {
                        /* get next page */
                        pe = pe->pe_next;
                }
                pe->pe_next = n_pe;
        } else {
                hpt[pt_hash] = n_pe;
        }
        return n_pe;
}

/* search_hbt 
 * search the page table for a vpn number, and return the page
 */
        static struct
page_entry * search_hpt(struct addrspace *as, vaddr_t addr)
{ 
        uint32_t pt_hash, proc;

        /* get the hash index for hpt */
        pt_hash = hpt_hash(as, addr);
        
        //kprintf("hashing addr: %x\n", addr);
        //kprintf("hash used is: %d\n", pt_hash);

        /* get addrspace id (secretly the pointer to the addrspace) */
        proc = (uint32_t) as;

        /* get the page table entry */
        struct page_entry *pe = hpt[pt_hash];

        /* loop the chain while we don't have an entry for this proc */
        while(pe != NULL && pe->pe_proc != proc) {
                pe = pe->pe_next;
        }

        /* we didn't find the desired entry - page fault! */
        if (pe == NULL) {
                return NULL;
        }

        /* return the page if the page is valid */
        return pe;
}

/*
 * purge_hpt
 * purges all records in the hpt and then the records they refer to in the ft
 * for the current addresspace - this is called when the process is ending
 */
        void
purge_hpt(struct addrspace *as)
{
        unsigned int i;
        uint32_t proc = (uint32_t) as;
        struct page_entry *c_pe, *p_pe;
        for (i=0; i < hpt_size; i++) {
                p_pe = c_pe = hpt[i];
                if (c_pe != NULL) {
                        while (c_pe != NULL && c_pe->pe_proc != proc) {
                                p_pe = c_pe;
                                c_pe = c_pe->pe_next;
                        }
                        if (c_pe != NULL) {
                                p_pe->pe_next = c_pe->pe_next;
                                free_kpages(FINDEX_TO_KVADDR(c_pe->pe_ppn));
                                kfree(c_pe);
                        }
                }
        }
}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
        (void)ts;
        panic("vm tried to do tlb shootdown?!\n");
}

