
/* asst3 extended: sbrk */

/* sbrk - set process break (allocate memory)
 * http://cgi.cse.unsw.edu.au/~cs3231/16s1/os161/man/syscall/sbrk.html
 */

/*
 * Errors
 * ENOMEM :     Sufficient virtual memory to satisfy the request was not 
 *              available, or the process has reached the limit of the 
 *              memory it is allowed to allocate.
 * EINVAL :     The request would move the "break" below its initial 
 *              value.
 */

#include <types.h>
#include <addrspace.h>
#include <sbrk.h>
#include <vm.h> 
#include <mips/vm.h>
#include <elf.h>
#include <proc.h>
#include <syscall.h>
#include <kern/errno.h>

int sys_sbrk(intptr_t amount, int32_t *retval) {

        *retval = (int) sbrk(amount);

        if (retval) {
                return 0;
        }

        return -1;
}

void *
sbrk(intptr_t amount) {

        struct addrspace *as;
        struct region *heap_region;

        as = proc_getas();
        heap_region = get_heap(as);

        /* if heap region doesn't exist, create it */
        if (!heap_region) {
                return create_heap(as);
        }

        /* heap region exists. extend it */

        /* page align the given amount (only if amount != 0) */
        if (amount && amount % PAGE_SIZE != 0) {
                amount += (PAGE_SIZE - amount % PAGE_SIZE);
        }

        /* check that it's not extending into the stack region */
        vaddr_t end_of_heap = (vaddr_t) heap_region + heap_region->size + amount;
        if (region_type(as, end_of_heap) == SEG_STACK) {
                return (void *)ENOMEM;
        }
        heap_region->size += amount;

        return (void *)heap_region;
}

void *
create_heap(struct addrspace *as) {
        /* get addr of where to create heap
         * make sure it won't impact on other regions
         * call define_region
         */

        uint32_t vaddr;     /* vaddr of new heap */
        uint32_t memsz;     /* size of heap */
        int result;

        vaddr = (uint32_t) get_heap_address(as);
        memsz = PAGE_SIZE;

        /* check that it's not extending into the stack region */
        vaddr_t end_of_heap = vaddr + memsz;
        if (region_type(as, end_of_heap)) {
                return (void *)ENOMEM;
        }

        result = as_define_region(as, vaddr, memsz, PF_R, PF_W, 0);
        if (result) {
                return (void *) -1;
        }

        return (void *) vaddr;
}

/* returns either NULL or the address of the heap region */
struct region *
get_heap(struct addrspace *as) {

        struct region *r;
        r = as->regions;

        while (r) {
                if (r->is_heap) {
                        break;
                }
                r = r->next;
        }

        return r;
}

/* return an address for which the heap can start at. this address will
 * be where the code/data regions end
 */
void *
get_heap_address(struct addrspace *as) {

        void *new_heap_addr;
        struct region *r;
        struct region *prev;

        r = as->regions;
        prev = as->regions;

        /* find the stack region */
        while (!r->is_stack) {
                prev = r;
                r = r->next;
        }

        /* prev is the code region */
        new_heap_addr = prev + prev->size;

        /* make sure it is aligned */
        if ((vaddr_t)new_heap_addr % 4 != 0) {
                new_heap_addr += (4 - (vaddr_t)new_heap_addr %4);
        }

        return new_heap_addr;
}
