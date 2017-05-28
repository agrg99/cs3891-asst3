#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <tlb.h>
#include <spl.h>
#include <mips/tlb.h>
#include <proc.h>
#include <current.h>

/* define static methods */
static uint32_t hpt_hash(struct addrspace *as, vaddr_t faultaddr);
static struct page_entry * search_hpt(struct addrspace *as, vaddr_t addr);
static struct page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr);

//static struct spinlock spinny_lock = SPINLOCK_INITIALIZER;

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
        int spl;
        struct page_entry *pe;
        struct addrspace *as;
/*
        switch (faulttype) {
                case VM_FAULT_READ:
                case VM_FAULT_WRITE:
                        break;
                case VM_FAULT_READONLY:
                        panic("readonpmn;y\n");
                default:
                       return EINVAL;
        }
*/
        (void)faulttype;
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
        spl = splhigh();
        pe = search_hpt(as, faultaddress);
        splx(spl);
        
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
        //        spinlock_acquire(&spinny_lock);
        spl = splhigh();
                pe = insert_hpt(as, faultaddress);
          //      spinlock_release(&spinny_lock);
        splx(spl);
                //   }
        }

        /* mask the ppn ready for tlb store */
        uint32_t ppn = (uint32_t) PN_TO_ADDR(pe->pe_ppn);
        //kprintf("vpn checked: %x\n", ADDR_TO_PN(faultaddress));
        //kprintf("ppn inserted under: %x\n\n", pe->pe_ppn);
        ppn = ppn | TLBLO_VALID | TLBLO_DIRTY; /* set valid bit */ 
        //pe->pe_flags = SET_PAGE_REF(pe->pe_flags);  /* set referenced */
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
        uint32_t vpn = ADDR_TO_PN(vaddr);
        uint32_t ppn = ADDR_TO_PN(KVADDR_TO_PADDR(n_frame));
        uint32_t pt_hash = hpt_hash(as, vaddr);
        struct page_entry *pe = hpt[pt_hash];
        
        /* set up new page */
        n_pe->pe_proc = (uint32_t) as;
        n_pe->pe_ppn = ppn;
        n_pe->pe_vpn = vpn;
        n_pe->pe_flags = SET_PAGE_PROT(0, region_perms(as, vaddr));
        n_pe->pe_flags = SET_PAGE_PRES(n_pe->pe_flags);
        n_pe->pe_next = NULL;

        //kprintf("inserting record with proc:%x, vpn:%x, hash:%x, addr:%x\n", (uint32_t)as, vpn, pt_hash, vaddr);

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
        uint32_t pt_hash, proc, vpn;
                
        /* get the hash index for hpt */
        pt_hash = hpt_hash(as, addr);
        
        //kprintf("hashing addr: %x\n", addr);
        //kprintf("hash used is: %d\n", pt_hash);

        /* get addrspace id (secretly the pointer to the addrspace) */
        proc = (uint32_t) as;

        /* declare the vpn */
        vpn = ADDR_TO_PN(addr);

        //kprintf("searching for addr 0x%x\n", addr);
        //kprintf("searching for hash 0x%x\n", pt_hash);
        //kprintf("searching for vpn 0x%x\n", vpn);
        //kprintf("searching for proc %x\n", proc);

        /* get the page table entry */
        struct page_entry *pe = hpt[pt_hash];

        /* loop the chain while we don't have an entry for this proc */
        while(pe != NULL) {
                if (pe->pe_proc == proc && pe->pe_vpn == vpn) {
                        break;
                }
                pe = pe->pe_next;
        }

        /* we didn't find the desired entry - page fault! */
        //if (pe == NULL) {
          //      kprintf("not found!!!\n");
        //kprintf("-------------------\n");
        //        return NULL;
        //}

        //kprintf("found!!!\n");
        //kprintf("-------------------\n");

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
       // kprintf("purging hpt: %x\n", proc);
        struct page_entry *c_pe, *n_pe, *t_pe;;
        
        int spl = splhigh();

        ///spinlock_acquire(&spinny_lock);
        for (i=0; i < hpt_size; i++) {
                n_pe = c_pe = hpt[i];
                while (c_pe != NULL) {
                        if (c_pe->pe_proc == proc) {
                               
                               
    //                           kprintf("purging %x for %x (row %d)\n", (unsigned int)c_pe->pe_vpn, proc, i);
                               

                               if (hpt[i] == c_pe)
                                        t_pe = hpt[i] = n_pe = c_pe->pe_next;
                                else
                                        t_pe = n_pe->pe_next = c_pe->pe_next;
                                free_kpages(FINDEX_TO_KVADDR(c_pe->pe_ppn));
                                kfree(c_pe);
                                c_pe = t_pe;
                        } else {
                                n_pe = c_pe;
                                c_pe = c_pe->pe_next;
                        }
                        /*c_pe = n_pe->pe_next;
                        if (c_pe->pe_proc == proc) {
                                kprintf("purging %x for %x\n", (unsigned int)c_pe->pe_vpn, proc);
                                n_pe->pe_next = c_pe->pe_next;
                                free_kpages(FINDEX_TO_KVADDR(c_pe->pe_ppn));
                                kfree(c_pe);
                                c_pe = n_pe->pe_next;
                        } else {
                                n_pe = c_pe;
                        }
                        if (!c_pe)
                                break;
                                */
                }
        }
        //spinlock_release(&spinny_lock);
        splx(spl);
}


/* duplicate_hpt
 * duplicates all entries for the old addrspace for the new one and sets all
 * pages to read only
 */
void
duplicate_hpt(struct addrspace *new, struct addrspace *old)
{
        kprintf("duplicating hpt\n");
        uint32_t n_proc = (uint32_t) new;
        uint32_t o_proc = (uint32_t) old;
        struct page_entry *pe;
        struct page_entry *n_pe;
        unsigned int i;
        
        //spinlock_acquire(&spinny_lock);
        int spl = splhigh();
        for (i=0; i < hpt_size; i++) {
                pe = hpt[i];
                while (pe != NULL) {
                        /* if we find a matching record, duplicate & insert */
                        n_pe = pe;
                        if (pe->pe_proc == o_proc) {
                                n_pe = kmalloc(sizeof(struct page_entry));   
                                *n_pe = *pe;
                                n_pe->pe_proc = n_proc;
                                n_pe->pe_next = pe->pe_next;
                                pe->pe_next = n_pe;
                                /* disable write bit */
                               // n_pe->pe_flags = SET_PAGE_NOWRITE(n_pe->pe_flags);
                               // pe->pe_flags = SET_PAGE_NOWRITE(pe->pe_flags);
                                /* increment refcount on frame */
 


                                kprintf("-------------------------------\n");
                                vaddr_t nep = alloc_kpages(1);
                                n_pe->pe_ppn = ADDR_TO_PN(KVADDR_TO_PADDR(nep));
                                
                                kprintf("memcpy(%x, %x, %d);\n", (uint32_t)nep, PADDR_TO_KVADDR(PN_TO_ADDR(pe->pe_ppn)), PAGE_SIZE);
                                memcpy((void *)nep, (void *)FINDEX_TO_KVADDR(pe->pe_ppn), PAGE_SIZE);
                                kprintf("memcpy(%x, %x, %d);\n", (uint32_t)nep, PADDR_TO_KVADDR(PN_TO_ADDR(pe->pe_ppn)), PAGE_SIZE);
                                kprintf("-------------------------------\n");
                               // kprintf("-------------------------------\n");
                               // kprintf("** pe: %x\n\tproc: %x\n\tvpn: %x\n\tppn: %x\n\tnext: %x\n", (uint32_t)pe, pe->pe_proc, pe->pe_vpn, pe->pe_ppn, (uint32_t)pe->pe_next); 
                               // kprintf("** n_pe: %x\n\tproc: %x\n\tvpn: %x\n\tppn: %x\n\tnext: %x\n", (uint32_t)n_pe, n_pe->pe_proc, n_pe->pe_vpn, n_pe->pe_ppn, (uint32_t)n_pe->pe_next); 
                                //ft[pe->pe_ppn].fe_refcount++;
                        }
                        pe = n_pe->pe_next;
                }
        }
       // spinlock_release(&spinny_lock);
    splx(spl);
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

