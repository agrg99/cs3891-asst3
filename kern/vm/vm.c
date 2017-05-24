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
static int hash_inc(uint32_t *pt_hash);

/* The following hash function will combine the address of the struct
 * addrspace and faultaddr address to reduce hash collisions between processes
 * (processes using similar address ranges). */
        static uint32_t
hpt_hash(struct addrspace *as, vaddr_t faultaddr)
{
        uint32_t index;
        index = (((uint32_t )as) ^ (faultaddr >> PAGE_BITS)) % hpt_size;
        return (!index) ? 1 : index;
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
        kprintf("[*] vm_boostrap: hpt_entry size is %d\n", sizeof(struct page_entry));
        kprintf("[*] vm_boostrap: total hpt size  %d pages\n", (hpt_size * sizeof(struct page_entry)) / PAGE_SIZE);

        unsigned int i;
        /* init the page table */
        for(i = 0; i < hpt_size; i++) {
                hpt[i].pe_proc_id = 0;
                hpt[i].pe_ppn   = 0;
                hpt[i].pe_flags = 0;
                hpt[i].pe_next  = 0;
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

        /* fault if we tried to write to a readonly page */
        //       if (faulttype == VM_FAULT_READONLY)
        //               return EFAULT;

        if (curproc == NULL) {
                return EFAULT;
        }

        (void) faulttype;

        as = proc_getas();
        if (as == NULL) {
                return EFAULT;
        }

        if (hpt == NULL) {
                return EFAULT;
        }

        /* get page entry */
        pe = search_hpt(as, faultaddress);

        if (pe && GET_PAGE_PRES(pe->pe_flags)) { /* if in frame table */
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
                vaddr_t n_page = alloc_kpages(1);
                pe = insert_hpt(as, n_page);
                //   }
        }

        /* mask the ppn ready for tlb store */
        uint32_t ppn = (pe->pe_ppn << 12) & PAGE_FRAME;
        ppn = ppn | TLBLO_VALID | TLBLO_DIRTY; /* set valid bit */ 
        pe->pe_flags = SET_PAGE_REF(pe->pe_flags);  /* set referenced */
        insert_tlb(faultaddress, ppn);       /* load tlb */

        return 0;
}

/* hash_inc
 * increment the hash table hash index safely
 */
        static int
hash_inc(uint32_t *pt_hash)
{
        /* wrap around end of list, and ensure empty spot 0 */
        *pt_hash = (*pt_hash + 1) % hpt_size;
        if (!(*pt_hash)) {
                *pt_hash = 1;
        }
        return *pt_hash;
}

/* insert_hpt
 * insert a page entry into the hpt, if the record exists then linear probing
 * is used to find an empty slot.
 */
        static struct
page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr)
{
        struct page_entry n_pe;
        uint32_t pt_hash;
        uint32_t vpn = ADDR_TO_PN(vaddr);
        uint32_t ppn = ADDR_TO_PN(KVADDR_TO_PADDR(vaddr));
        n_pe.pe_vpn = vpn;
        n_pe.pe_proc_id = (uint32_t) as;
        n_pe.pe_ppn = ppn;
        n_pe.pe_flags = SET_PAGE_PROT(0, PROT_RW);
        n_pe.pe_flags = SET_PAGE_PRES(n_pe.pe_flags);
        n_pe.pe_next = 0;

        pt_hash = hpt_hash(as, vpn);
        struct page_entry *pe = &hpt[pt_hash];
        struct page_entry *t_pe;

        /* check if this record is taken */
        if (pe->pe_vpn) {
                /* record IS taken */
                /* loop until we find the last of the chain */
                while (pe->pe_next) {
                        /* get next row */
                        pe = &hpt[pe->pe_next];
                }
                /* pe contains the last element of the chain */
                pt_hash = hpt_hash(as, pe->pe_vpn);
                /* safely increment */
                pt_hash = hash_inc(&pt_hash);
                /* assign t_pe to the next entry after the last in the chain */
                t_pe = &hpt[pt_hash];
                /* go down the table to find an empty entry */
                while (t_pe->pe_vpn) {
                        pt_hash = hash_inc(&pt_hash);
                        t_pe = &hpt[pt_hash];
                }
                /* found an empty entry */
                pe->pe_next = pt_hash;
                /* assign the empty entry back */
                pe = t_pe;
        }
        /* store new page entry */
        hpt[pt_hash] = n_pe;
        return &hpt[pt_hash];
}

/* search_hbt 
 * search the page table for a vpn number, and return the physical page num
 * if found, otherwise return -1
 */
        static struct
page_entry * search_hpt(struct addrspace *as, vaddr_t addr)
{ 
        uint32_t pt_hash, pid, vpn;

        /* get the vpn from the address */
        vpn = ADDR_TO_PN(addr); 

        /* get the hash index from hte hpt */
        pt_hash = hpt_hash(as, vpn);

        /* get addrspace id (secretly the pointer to the addrspace) */
        pid = (uint32_t) as;

        /* get the page table entry */
        struct page_entry *pe = &hpt[pt_hash];

        /* loop the chain while we don't have an entry for this proc */
        while((pe->pe_proc_id != pid && pe->pe_vpn != vpn) && pe->pe_proc_id != 0) {
                pe = &hpt[pe->pe_next];
        }

        /* we didn't find the desired entry - page fault! */
        if (!pe->pe_proc_id) {
                return NULL;
        }

        /* return the PPN if the page is valid, otherwise -1 */
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
        unsigned int i,j;
        for (i=1; i<hpt_size; i++) {
                /* remove all entries to current as from frame table */
                if (hpt[i].pe_proc_id == (uint32_t)as) {
                        /* find the entry that references this one */
                        for (j=1; j<hpt_size; j++){
                                if(hpt[j].pe_next == i){
                                        /* set its next to the real next */
                                        hpt[j].pe_next = hpt[i].pe_next;
                                        break;
                                }
                        }
                        hpt[i].pe_proc_id = 0;
                        hpt[i].pe_vpn = 0;
                        hpt[i].pe_flags = 0;
                        hpt[i].pe_next = 0;
                        free_kpages(FINDEX_TO_KVADDR(hpt[i].pe_ppn));
                        hpt[i].pe_ppn = 0;
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

