#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <spl.h>

/* define static internal functions */
static vaddr_t pop_frame(void);
static void push_frame(vaddr_t vaddr);
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

/*
static void fill_deadbeef(void *vptr, size_t len);

static void fill_deadbeef(void *vptr, size_t len)
{
	uint32_t *ptr = vptr;
	size_t i;

	for (i=0; i<len/sizeof(uint32_t); i++) {
		ptr[i] = 0xdeadbeef;
	}
}

*/
        void
frametable_init()
{   
        paddr_t ram_sz, ft_top;
        size_t ft_size; 
        int n_pages, used_pages, i;

        /* size of ram and num pages in the system */
        ram_sz = ram_getsize();
        n_pages = ram_sz / PAGE_SIZE;

        /* set the size to init the hpt when we need to */
        hpt_size = n_pages * 2;

        kprintf("[*] Virtual Memory: %d frame fit in memory\n", n_pages);
        kprintf("[*] Virtual Memory: ram size is %p\n", (void *)ram_sz);

        /* calc size of the frame table */
        ft_size = n_pages * sizeof(struct frame_entry);
        kprintf("[*] Virtual Memory: size of ft is: 0x%x\n", (int)ft_size);

        /* allocate some space for the hpt when we need it */
        hpt = (struct page_entry **)kmalloc(hpt_size * sizeof(struct page_entry *));

        /* allocate the frame table above the os */
        ft = (struct frame_entry *)kmalloc(ft_size);

        /* calc the top of the frame table */
        ft_top = ram_getfirstfree();
        kprintf("[*] Virtual Memory: first free is %p\n", (void *)ft_top);

        /* calculate the number of pages used so far */
        used_pages = ft_top / PAGE_SIZE;
        kprintf("[*] Virtual Memory: num pages used in total: %d\n", used_pages);
        kprintf("[*] Virtual Memory: therefore size of used: 0x%x\n", used_pages*PAGE_SIZE);

        /* set the current free page index */
        cur_free = used_pages;

        /* then init all the dirty pages */
        for(i = 0; i < used_pages; i++)
        {
                ft[i].fe_refcount = 1;
                ft[i].fe_used = 1;
                ft[i].fe_next = VM_INVALID_INDEX;
        }
        /* init the clean pages */
        for(i = used_pages; i < n_pages; i++)
        {
                ft[i].fe_refcount = 0;
                ft[i].fe_used = 0;
                if(i != n_pages-1) {
                        ft[i].fe_next = i+1;
                }
        }
        /* fix the corner case */
        ft[n_pages-1].fe_next = VM_INVALID_INDEX;
}


/* Note that this function returns a VIRTUAL address, not a physical 
 * address
 * WARNING: this function gets called very early, before
 * vm_bootstrap().  You may wish to modify main.c to call your
 * frame table initialisation function, or check to see if the
 * frame table has been initialised and call ram_stealmem() otherwise.
 */
        vaddr_t
alloc_kpages(unsigned int npages)
{



        /* check if the page table or frame table has not been allocated yet */
        if (!ft) {
                /* vm system not alive - use stealmem */
                paddr_t addr;
                spinlock_acquire(&stealmem_lock);
                addr = ram_stealmem(npages);
                spinlock_release(&stealmem_lock);
                if(addr == 0) {
                        return 0;
                }
                return PADDR_TO_KVADDR(addr);
        } else {
                /* use my allocator as frame table is now initialised */
                if (npages > 1){
                        /* can't alloc more than one page */
                        return 0;
                }
                spinlock_acquire(&stealmem_lock);
                /* ensure we have enough memory to alloc */
                if (cur_free == VM_INVALID_INDEX) {
                        spinlock_release(&stealmem_lock);
                        return 0;
                }
                /* pop the next free frame */         
                vaddr_t addr = pop_frame();
                spinlock_release(&stealmem_lock);
                return addr;               
        }
}

/* pop_frame()
 * pops the next available frame from the freelist and returns the kvaddr
 */
        static vaddr_t
pop_frame(void)
{
        int c_index = cur_free;
        /* if we are getting the last frame, deem the cur_free invalid */
        if (ft[cur_free].fe_next == VM_INVALID_INDEX) {
                cur_free = VM_INVALID_INDEX;
        } else {
                /* otherwise just set the next free */
                cur_free = ft[cur_free].fe_next;
        }



        /* alter meta data */
        ft[c_index].fe_used = 1;
        ft[c_index].fe_refcount = 1;
        ft[c_index].fe_next = VM_INVALID_INDEX;

        vaddr_t addr = FINDEX_TO_KVADDR(c_index);       /* find the kvaddr */
 //       bzero((void *)addr, PAGE_SIZE);                 /* zero the frame */
//fill_deadbeef((void *)addr, PAGE_SIZE);


     /*   int spl = splhigh();
        kprintf("allocating %x\n", addr);
        splx(spl);
  */
        return addr;
}

/* push_frame()
 * push a frame onto the freelist
 */
        static void
push_frame(vaddr_t vaddr)
{
        int c_index;
        c_index = KVADDR_TO_FINDEX(vaddr);
 //         ADDR_TO_PN(vaddr);

        /* append fe to the start of the freelist */
        if (ft[c_index].fe_refcount == 1) {
                ft[c_index].fe_used = 0;
                ft[c_index].fe_refcount = 0;
                ft[c_index].fe_next = cur_free;
                cur_free = c_index;
        } else if (ft[c_index].fe_refcount == 0) {
                panic("reached 0 refcount\n");
        } else {
                ft[c_index].fe_refcount--;
                kprintf("vaddr %x has new refcount %d\n", vaddr, ft[c_index].fe_refcount); 
        }
}

        void
free_kpages(vaddr_t addr)
{
        spinlock_acquire(&stealmem_lock);
        /* call static function to free */
        push_frame(addr);
        spinlock_release(&stealmem_lock);
}

