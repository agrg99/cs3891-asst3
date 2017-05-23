#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* define the hpt size to be used across different functions */
int hpt_size;

/* The following hash function will combine the address of the struct
 * addrspace and faultaddr address to reduce hash collisions between processes
 * (processes using similar address ranges). */
uint32_t hpt_hash(struct addrspace *as, vaddr_t faultaddr)
{
        uint32_t index;

        index = (((uint32_t )as) ^ (faultaddr >> PAGE_BITS)) % hpt_size;
        return index;
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

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
        int offset;
        struct page_entry *pe;

        /* fault if we tried to write to a readonly page */
        if (faulttype == VM_FAULT_READONLY)
                return EFAULT;

        /* get page entry */
        pe = search_hpt(faultaddress);
        if (pe == NULL) {
                /* page fault */
                (void)pe;
        }

        if GET_PAGE_PRES(pe->pe_flags) {
        //   pte->flag has pt_r?
        //      return EFAULT;
        //   else
        //      pte->dirty?
        //         load from swap
        //      else
        //         load from elf
        //      update tlb, page table and frame table
        }
        else {
        //   if segment is code
        //     load from elf
        //   else
        //     allocate a zeroed page
        }
        //   update tlb, page table and frame table
        
        offset = faultaddress & ~PAGE_FRAME;    /* get the offset */
        (void)offset;
        /* get frame physical address */
        //p_address = getpage();

        /* write result of getpage() to TLB */
        //tlb_random(uint32_t entryhi, uint32_t entrylo);

        return EFAULT;
}

/* search_hbt 
 * search the page table for a vpn number, and return the physical page num
 * if found, otherwise return -1
 */
struct *page_entry search_hpt(vaddr_t addr)
{
        uint32_t pt_hash, pid;
        struct addrspace *as;
        int vpn;

        /* get the current as */
        as = proc_getas();

        /* get the vpn from the address */
        vpn = addr & PAGE_FRAME; 

        /* get the hash index from hte hpt */
        pt_hash = hpt_hash(as, vpn);

        /* get addrspace id (secretly the pointer to the addrspace) */
        pid = (uint32_t) as;

        /* get the page table entry */
        struct page_entry *pe = &hpt[pt_hash];

        /* loop the chain while we don't have an entry for this proc */
        while(pe->pe_proc_id != pid && pe->pe_proc_id != VM_INVALID_INDEX) {
                pe = &hpt[pe->pe_next];
        }

        /* we didn't find the desired entry - page fault! */
        if (pe->pe_proc_id == VM_INVALID_INDEX) {
                return NULL;
        }

        /* return the PPN if the page is valid, otherwise -1 */
        return pe;
}

//int
//getpage(int ppn)
//{
//
//
//}

/*
 *
 * SMP-specific functions.  Unused in our configuration.
 */

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
        (void)ts;
        panic("vm tried to do tlb shootdown?!\n");
}

