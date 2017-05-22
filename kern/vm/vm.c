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
        /* can get away with kmalloc here, as it still uses the basic
         * stealmem function */
        kprintf("we called bootstrap\n");
        frametable_init();
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
        (void) faulttype;
        (void) faultaddress;


        

        // if pte->valid == 1
        //   pte->flag has pt_r?
        //      return
        //   else
        //      pte->dirty?
        //         load from swap
        //      else
        //         load from elf
        //      update tlb, page table and frame table
        // else
        //   if segment is code
        //     load from elf
        //   else
        //     allocate a zeroed page
        //   update tlb, page table and frame table
        

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
int
search_hpt(struct addrspace *as, vaddr_t address)
{
        vaddr_t p_address;
        uint32_t pt_hash;
        int vpn, offset;

        vpn = address >> 12;        /* get the vpn from the address */
        offset = address & 0xFF;    /* get the offset */

        /* get the hash index from hte hpt */
        pt_hash = hpt_hash(as, vpn);

        /* get the page table entry */
        struct page_entry pe = hpt[pt_hash];
        
        (void)p_address;
        (void)pe;
        (void)offset;

        return 0;       
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

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
        (void)ts;
        panic("vm tried to do tlb shootdown?!\n");
}

