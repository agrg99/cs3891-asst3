
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

#include <addrspace.h>
#include <vm.h> 
#include <mips/vm.h>
#include <elf.h>

void *
sbrk(intptr_t amount) {

        struct addrspace *as;
        struct region *heap_region;

        as = proc_getas();
        heap_region = get_heap(as);

        if (heap_region) {
                /* heap region exists. extend it */

        } else {
                /* create a heap region, add it to the address space */

                uint32_t vaddr;     /* Virtual address */
                uint32_t memsz;     /* Size of data to be loaded into memory*/
                uint32_t flags;     /* Flags */

                vaddr = HEAP_ADDRESS;   // constant of find best addr?
                memsz = 0;
                flags = 0 & PF_R & PF_W;

		result = as_define_region(as, vaddr, memsz, flags);
		if (result) {
			return result;
		}
        }

        return (void *)heap_region;      // fail.
}

/* returns either NULL or an address of the heap region */
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

        return result;
}
