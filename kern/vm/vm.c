#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <tlb.h>
#include <proc.h>

/* define static methods */
static uint32_t hpt_hash(struct addrspace *as, vaddr_t faultaddr);
static struct page_entry * search_hpt(struct addrspace *as, vaddr_t addr);
static struct page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr);
static int hash_inc(uint32_t *pt_hash);

/* define the hpt size to be used across different functions */
int hpt_size;

/* The following hash function will combine the address of the struct
 * addrspace and faultaddr address to reduce hash collisions between processes
 * (processes using similar address ranges). */
static uint32_t hpt_hash(struct addrspace *as, vaddr_t faultaddr)
{
        uint32_t index;
        index = (((uint32_t )as) ^ (faultaddr >> PAGE_BITS)) % hpt_size;
        return (!index) ? 1 : index;
}

/* bootstrap the frame and page table here. We can use kmalloc because 
 * alloc_kpages still uses the stealmem function. */
void vm_bootstrap(void)
{
        frametable_init();

        /* create the page table */
        kprintf("time to setup page table\n");
        hpt = (struct page_entry *)kmalloc(hpt_size * sizeof(struct page_entry));
        int i;
        /* init the page table */
        for(i = 0; i < hpt_size; i++) {
                hpt[i].pe_proc_id = VM_INVALID_INDEX;
                hpt[i].pe_ppn   = VM_INVALID_INDEX;
                hpt[i].pe_flags = VM_INVALID_INDEX;
                hpt[i].pe_next  = VM_INVALID_INDEX;
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
        if (faulttype == VM_FAULT_READONLY)
                return EFAULT;

        as = proc_getas();

        /* get page entry */
        pe = search_hpt(as, faultaddress);

        if (GET_PAGE_PRES(pe->pe_flags)) { /* if in frame table */
                //   pte->flag has pt_r?   |
                //      return EFAULT;     |
                //   else
              /*  if GET_PAGE_MOD(pe->pe_flags) {
                        //         load from swap
                } else { 
                        //         load from elf
                } */
                
        }
        else { /* not in frame table */
                if (region_type(as, faultaddress) == SEG_CODE) {
                        //  load from elf
                } else {
                        //  allocate a zeroed page
                        vaddr_t n_page = alloc_kpages(1);
                        /* create and insert the page entry */
                        pe = insert_hpt(as, n_page);
                }
        }

        pe->pe_flags = SET_PAGE_REF(pe->pe_flags);  /* set referenced */
        insert_tlb(faultaddress, pe->pe_ppn);       /* load tlb */

        return 0;
}

/* hash_inc
 * increment the hash table hash index safely
 */
static int hash_inc(uint32_t *pt_hash)
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
static struct page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr)
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
static struct page_entry * search_hpt(struct addrspace *as, vaddr_t addr)
{ 
        uint32_t pt_hash, pid;
        int vpn;

        /* get the vpn from the address */
        vpn = ADDR_TO_PN(addr); 

        /* get the hash index from hte hpt */
        pt_hash = hpt_hash(as, vpn);

        /* get addrspace id (secretly the pointer to the addrspace) */
        pid = (uint32_t) as;

        /* get the page table entry */
        struct page_entry *pe = &hpt[pt_hash];

        /* loop the chain while we don't have an entry for this proc */
        while(pe->pe_proc_id != pid && pe->pe_proc_id != 0) {
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
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
        (void)ts;
        panic("vm tried to do tlb shootdown?!\n");
}

