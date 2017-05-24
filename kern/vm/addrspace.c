/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *        The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <current.h>
#include <addrspace.h>
#include <tlb.h>
#include <vm.h>
#include <mips/vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */

static int
append_region(struct addrspace *as, int permissions, vaddr_t start, size_t size);


    struct addrspace *
as_create(void)
{
    struct addrspace *as;

    as = kmalloc(sizeof(struct addrspace));
    if (as == NULL) {
        return NULL;
    }

    as->regions = NULL;

    return as;
}

    int
as_copy(struct addrspace *old, struct addrspace **ret)
{
    /*
       - allocates a new destination addr space
       - adds all the same regions as source
       - roughly, for each mapped page in source
       - allocate a frame in dest
       - copy contents from source frame to dest frame
       - add PT entry for dest
       */
panic("tried to as_copy\n");
    /*
     * it is my understanding we need to loop through the page table and
     * look at each entry that corresponds to the old addresspace (lucky 
     * we used the pointer as the process Identifier) and copy all the 
     * entries to use for the new address space (using the new addrspace 
     * pointer as the identifier lol) but have them point to the same
     * physical frame - changing both address spaces to have all pages
     * as read only. on write to a page that is NOW read only then the frame 
     * is duplicated for the process that tried to write to it 
     *
     * this is why we need to keep a refcount with the frame - if it is
     * more than one then we need to duplicate the frame and change the 
     * frame number for the page entry of the address space doing the write
     */

    struct addrspace *newas;

    newas = as_create();
    if (newas==NULL) {
        return ENOMEM;
    }

    /* set the pages and bases the same? copied from dumvm */
    //new->as_vbase1 = old->as_vbase1;
    //new->as_npages1 = old->as_npages1;
    //new->as_vbase2 = old->as_vbase2;
    //new->as_npages2 = old->as_npages2;

    (void)old;

    *ret = newas;
    return 0;
}

/* as_destroy
 * destroys the addrspace
 */
    void
as_destroy(struct addrspace *as)
{
    /* purge the hpt and ft of all records for this AS */
    purge_hpt(as);

    /* free all regions */
    struct region *c_region = as->regions;
    struct region *n_region;
    while (c_region) {
        n_region = c_region->next;
        kfree(c_region);
        c_region = n_region;
    }

    kfree(as);
}


/* as_activate
 * flushes tlb
 */
    void
as_activate(void)
{
    struct addrspace *as = proc_getas();
    if (as == NULL) {
        /*
         * Kernel thread without an address space; leave the
         * prior address space in place.
         */
        return;
    }

    /* Disable interrupts and flush TLB */
    flush_tlb();
}

/* as_deactivate
 * flushes the tlb
 */
    void
as_deactivate(void)
{
    struct addrspace *as = proc_getas();
    if (as == NULL) {
        /*
         * Kernel thread without an address space; leave the
         * prior address space in place.
         */
        return;
    }

    flush_tlb();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
    int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
        int readable, int writeable, int executable)
{
    int result;

    /* Align the region. First, the base... */
    memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
    vaddr &= PAGE_FRAME;
    /* ...and now the length. */
    memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

    int permissions = (readable << 2 | writeable << 1 | executable); 

    /* append a region to the address space */
    result = append_region(as, permissions, vaddr, memsize);
    if (result) {
        return result;
    }

    return 0;
}

/* as_prepare_load
 * store the old permissions and set the region permissions to be RW
 */
    int
as_prepare_load(struct addrspace *as)
{
    struct region *regions = as->regions;
    while(regions != NULL) {
        regions->old_writebit = regions->cur_writebit;
        regions->cur_writebit = 1;
        regions = regions->next;
    }
    return 0;
}

/* as_complete_load
 * set the region permissions back to the old ones
 */
    int
as_complete_load(struct addrspace *as)
{
    struct region *regions = as->regions;
    while(regions != NULL) {
        regions->cur_writebit = regions->old_writebit;
        regions = regions->next;
    }
    /* flush read only sections out */
    flush_tlb();
    return 0;
}

/* as_define_stack
 * define the stack region within the addr space */
    int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
    int result;
    /* allocate the stack region */
    result = as_define_region(as, USERSTACK - USERSTACK_SIZE, USERSTACK_SIZE, 1, 1, 1);
    if (result) {
        return result;
    }

    /* Initial user-level stack pointer */
    *stackptr = USERSTACK;

    return 0;
}


/* append_region
 * creates and appends a region to the current address space region list */
    static int
append_region(struct addrspace *as, int permissions, vaddr_t start, size_t size)
{
    struct region *n_region, *c_region, *t_region;

    n_region = kmalloc(sizeof(struct region));
    if (!n_region)
        return EFAULT;

    n_region->old_writebit = (permissions & 0x2) >> 1;
    n_region->cur_writebit = (permissions & 0x2) >> 1;
    n_region->start = start;
    n_region->size = size;
    n_region->next = NULL;

    /* append the new region to where it fits in the region list */
    t_region = c_region = as->regions;
    if (c_region) {
        while(c_region && c_region->start < n_region->start){
            t_region = c_region;
            c_region = c_region->next;
        }
        t_region->next = n_region;
        n_region->next = c_region;
    } else {
        as->regions = n_region;
    }

    return 0;
}

/* region_type
 * find what type of region a virtual address is from. returns -1 if the
 * address isn't within any region.
 */
int region_type(struct addrspace *as, vaddr_t addr)
{
    struct region *c_region = as->regions;
    int index = 1;
    /* loop through all regions in addrspace */
    while (c_region != NULL)
    {
        if (addr <= (c_region->start + c_region->size))
            return index;
        c_region = c_region->next;
    }
    return -1;
}

