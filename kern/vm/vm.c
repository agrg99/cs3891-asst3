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
static struct page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr, vaddr_t n_frame);

//static struct spinlock spinny_lock = SPINLOCK_INITIALIZER;

/* The following hash function will combine the address of the struct
 * addrspace and faultaddr address to reduce hash collisions between processes
 * (processes using similar address ranges). */
    static uint32_t
hpt_hash(struct addrspace *as, vaddr_t faultaddr)
{
    uint32_t index;
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
    int spl, perms, region;
    uint32_t ppn;
    struct page_entry *pe;
    struct addrspace *as;

    as = proc_getas();
    /* sanity check */
    if (!curproc || !hpt || !as) {
        return EFAULT;
    }

    /* check if request was to a valid region */
    if (!(region = region_type(as, faultaddress))){
        return EFAULT;
    }

    /* get page entry */
    spl = splhigh();
    pe = search_hpt(as, faultaddress);
    splx(spl);

    /* get perms for current region */
    perms = region_perms(as, faultaddress);
    
    switch (faulttype) {
            case VM_FAULT_READ:
            case VM_FAULT_WRITE:
                goto normal_handle;
            case VM_FAULT_READONLY:
                
                if (!pe)
                    panic("this shouldn't happen");
               
                /* region isn't writable anyway so EFAULT */
                if (!(GET_WRITABLE(perms)))
                    return EFAULT;

                /* region is writable but page isn't - COW! */
                if (!(GET_WRITABLE(pe->pe_flags >> 1)))
                    goto do_cow;
        
                /* region is writable and page is writeable - fix TLB */
                ppn = (uint32_t) PN_TO_ADDR(pe->pe_ppn) | TLBLO_DIRTY;
                replace_tlb(faultaddress, ppn);
                return 0;

            default:
                   return EINVAL;
        }

do_cow:
        spl = splhigh();
        if (pe) {
            struct frame_entry fe = ft[pe->pe_ppn]; /* get the frame entry */
            if (fe.fe_refcount > 1) {
                fe.fe_refcount--;
                vaddr_t new_frame = alloc_kpages(1);
                memcpy((void *)new_frame, (void *)FINDEX_TO_KVADDR(pe->pe_ppn), PAGE_SIZE);
                pe->pe_ppn = KVADDR_TO_FINDEX(new_frame);
            }
            /* if refcount is 1, then the other process in the fork has
             * already done their deed and copied the frame so we can just
             * become writeable for this frame */
            pe->pe_flags = SET_PAGE_WRITE(pe->pe_flags);
        } else {
            panic("COW but we don't have a page entry??");
        }
        splx(spl);
        flush_tlb();
        goto fill_tlb;

normal_handle:
        if (pe) { // && GET_PAGE_PRES(pe->pe_flags)) { /* if in frame table */
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
            spl = splhigh();
            vaddr_t n_frame = alloc_kpages(1);
            pe = insert_hpt(as, faultaddress, n_frame);
            splx(spl);
        }

fill_tlb:            
        ppn = (uint32_t) PN_TO_ADDR(pe->pe_ppn);
        int dirty = (GET_WRITABLE(pe->pe_flags >> 1)) ? TLBLO_DIRTY : 0;
        ppn = ppn | TLBLO_VALID | dirty; /* set valid bit */ 
        //ddpe->pe_flags = SET_PAGE_REF(pe->pe_flags);  /* set referenced */
        insert_tlb(faultaddress, ppn);       /* load tlb */

        return 0;
}

/* insert_hpt
 * insert a page entry into the hpt, if the record exists then linear probing
 * is used to find an empty slot.
 */
static struct
page_entry * insert_hpt(struct addrspace *as, vaddr_t vaddr, vaddr_t n_frame)
{
        struct page_entry *n_pe = kmalloc(sizeof(struct page_entry));
        uint32_t vpn = ADDR_TO_PN(vaddr);
        uint32_t ppn = KVADDR_TO_FINDEX(n_frame);
        uint32_t pt_hash = hpt_hash(as, vaddr);
        struct page_entry *pe = hpt[pt_hash];

        int perms = region_perms(as, vaddr);

        /* set up new page */
        n_pe->pe_proc = (uint32_t) as;
        n_pe->pe_ppn = ppn;
        n_pe->pe_vpn = vpn;
        n_pe->pe_flags = SET_PAGE_PROT(0, perms);
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
        uint32_t pt_hash, proc, vpn;
                
        /* get the hash index for hpt */
        pt_hash = hpt_hash(as, addr);
        
        /* get addrspace id (secretly the pointer to the addrspace) */
        proc = (uint32_t) as;

        /* declare the vpn */
        vpn = ADDR_TO_PN(addr);

        /* get the page table entry */
        struct page_entry *pe = hpt[pt_hash];

        /* loop the chain while we don't have an entry for this proc */
        while(pe != NULL) {
                if (pe->pe_proc == proc && pe->pe_vpn == vpn) {
                    break;
                }
                pe = pe->pe_next;
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
       // kprintf("purging hpt: %x\n", proc);
        struct page_entry *c_pe, *n_pe, *t_pe;;
        
        int spl = splhigh();
        for (i=0; i < hpt_size; i++) {
                n_pe = c_pe = hpt[i];
                while (c_pe != NULL) {
                        if (c_pe->pe_proc == proc) {
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
                }
        }
        splx(spl);
}


/* duplicate_hpt
 * duplicates all entries for the old addrspace for the new one and sets all
 * pages to read only
 */
void
duplicate_hpt(struct addrspace *new, struct addrspace *old)
{
        uint32_t o_proc = (uint32_t) old;
        struct page_entry *pe, *n_pe;
        unsigned int i;
        
        int spl = splhigh();
        for (i=0; i < hpt_size; i++) {
                pe = hpt[i];
                while (pe != NULL) {
                        /* if we find a matching record, duplicate & insert */
                        n_pe = pe;
                        if (pe->pe_proc == o_proc) {
                                /* insert the new entry with the old frame */
                                vaddr_t old_frame = FINDEX_TO_KVADDR(pe->pe_ppn);
                                n_pe = insert_hpt(new, PN_TO_ADDR(pe->pe_vpn), old_frame); 
                                
                                /* disable write bit */
                                n_pe->pe_flags = SET_PAGE_NOWRITE(n_pe->pe_flags);
                                pe->pe_flags = SET_PAGE_NOWRITE(pe->pe_flags);
                                
                                /* increment refcount on frame */
                                ft[pe->pe_ppn].fe_refcount++;
                        }
                        pe = n_pe->pe_next;
                }
        }
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

