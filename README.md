# COMP3891 Extended Operating Systems 2017/S1 - Assignment 3 (Memory Management)

> In this assignment you will implement the virtual memory sub-system of OS/161. The existing VM implementation in OS/161, dumbvm, is a minimal implementation with a number of shortcomings. In this assignment you will adapt OS/161 to take full advantage of the simulated hardware by implementing management of the MIPS software-managed Translation Lookaside Buffer (TLB). You will write the code to manage this TLB. You will also write code to manage system memory.

## Memory Management
This assignment requires you to keep track of physical memory. The current memory management implementation in dumbvm never frees memory; your implementation should handle both allocation and freeing of frames.

You will need a frametable containing information about the memory available in the system. In the basic assignment you will need to keep track of whether a frame is used or not. In the advanced part, you will need to keep track of reference statistics and other information about the frames.

The functions that deal with memory are described in kern/include/vm.h. You may assume that only one page will be allocated at a time—designing a page allocator that can allocate multiple pages at a time is surprisingly tricky. However, make sure that you never allocate memory (though kmalloc) that is larger than a page!

Note that alloc_kpages() should return the virtual address of the page, i.e., an address in kseg0.

Warning: alloc_kpages() can be called before vm_bootstrap(). The means that your implementation of alloc_kpages() must work before your frametable is initialised. You should just call ram_stealmem() if the frametable hasn't been initialised.

## Address Space Management
OS/161 has an address space abstraction, the struct addrspace. To enable OS/161 to interact with your VM implementation, you will need to fill in the functions in kern/vm/addrspace.c. The semantics of these functions is documented in kern/include/addrspace.h.

You may use a fixed-size stack region (say 16 pages) for each process.

## Address Translation
The main goal for this assignment is to provide virtual memory translation for user programs. To do this, you will need to implement a TLB refill handler. You will also need to implement a page table. For this assignment, you will implement a hashed page table (HPT).

Note that a hashed page table is a fixed sized data structure allocated at boot time and shared between all processes. We suggest sizing the table to have twice as many entries as there are frames of physical memory in RAM.

Given the HPT is a shared data structure, it will have to handle concurrent access by multiple processes. You'll need to synchronise access to avoid potential races. A single lock is satisfactory approach.

Each entry in the HPT typically has a process identifier, the page number, a link to handle collisions, and a frame number and permissions in EntryLo format for faster TLB loading. A HPT entry should not need to exceed 4 32-bit words.

See the wiki for a sample hash function (you're free to tune what we suggest). Hash collisions are always possible even with a good hash function, and you can use either internal or external chaining to resolve collisions. We suggest external chaining to avoid the HPT filling in the presence of sharing (in the advanced/bonus assignments), however internal chaining is sufficient for the basic assignment.

One can use the OS/161 address space pointer (of type struct addrspace) as the value of the current process ID. It is readily accessible where needed, and is unique to each address space.

# Design doc for Ass 3

## Identify an approach to allocate the memory that will become your frame table and implement it.

The frametable has the following fields:

```
struct frame_entry {
        int     fe_refcount; /* number of references to this frame */
        char    fe_used;  /* flag to indicate if this frame is free */
        int     fe_next;  /* if this frame is free, index of next free */
};
```


## Initialise the frame table (vm_bootstrap is a plausible location to call the code).

One of the first things that the kernel does upon booting is call `vm_bootstrap()`, this inturn calls `frametable_init()` (within vm/frametable.c). Frametable_init uses alloc_kpages indirectly (via kmalloc), which uses ram_stealmem at this point in time while we are setting up the frametable. Once the frametable has been initialised then alloc_kpages uses frames to alloc and dealloc memory, via `push_frame()` and `pop_frame()`.

Frametable_init performs the following steps:
        1. Calculate the number of pages available
        2. Calculate the size of the frame table
        3. Allocate memor for frame table and page table
        4. Get the top of the frame table
        5. Calculate number of pages already used (by the OS)
        6. Set the current free page index
        7. Set dirty/clean bits according to what pages are in use


## Write routines in kern/vm/frametable.c to manage free frames and allocate pages. Note: the frame table is a global resource, therefore you'll need to deal with concurrency. It is okay to use the spinlock already in place for stealmem, or you can use interrupt disabling/enabling for this table.


A spinlock is made use of in the following situations:
        * Acquiring memory within `alloc_kpages()`
        * Freeing a page within `free_kpages()`

Before the frame table is initialised the spinlock surrounds the `ram_stealmem()` call, and after the frame table is initiliased the two aforementioned situations call `pop_frame()` and `push_frame()` respectively. This is to prevent race conditions with the global frame table pointer.


## Understand how the page table works, and its relationship with the TLB.

The page table keeps track of a process id, physical page address, virtual page address, flags and a link to the next entry. Once memory has been allocated then a page entry is created and inserted into the TLB after a fault (when an address which lies within that frame has been requested to be accessed).

TLB keeps track of page table entries for a given address space. The TLB can be flushed, pages can be replaced, or pages can be found via `tlb_probe()`. Pages are found by hashing the virtual address and a page frame mask to look up an entry within the hashed page table.


## Work out a basic design for your page table implementation.

```
struct page_entry {
        uint32_t        pe_proc;    /* the process id */
        uint32_t        pe_ppn;     /* the frame table frame num */
        uint32_t        pe_vpn;     /* the frame table frame num */
        char            pe_flags;   /* page permissions and flags */
        struct page_entry *pe_next; /* pointer to collion next entry */
};
```


## Modify kern/vm/vm.c to insert , lookup, and update page table entries, and keep the TLB consistent with the page table.

## Implement the TLB exception handlers in vm.c using your page table.

## Implement the functions in kern/vm/addrspace.c that are required for basic functionality (e.g. as_create(), as_prepare_load(), etc.). Allocating user pages in as_define_region() may also simplify your assignment, however good solution allocate pages in vm_fault().

The following functions were implemented within kern/vm/addrspace.c:

        * struct addrspace *as_create(void)
                - creates an address space

        * int as_copy(struct addrspace *old, struct addrspace **ret)
                - allocates a new destination addr space
                - adds all the same regions as source
                - roughly, for each mapped page in source
                - allocate a frame in dest
                - copy contents from source frame to dest frame
                - add PT entry for dest

        * void as_destroy(struct addrspace *as)
                - purges the page and frame tables
                - frees all regions

        * void as_activate(void)
                - if there is an address space then flush the tlb

        * void as_deactivate(void)
                - if there is an address space then flush the tlb

        * int as_define_region(struct addrspace *as, vaddr_t vaddr, 
                        size_t memsize, int readable, int writeable, 
                        int executable)
                - create a new region within a given address space

        * int as_prepare_load(struct addrspace *as)
                - store current permissions and then change them to RW

        * int as_complete_load(struct addrspace *as)
                - restore saved permissions

        * int as_define_stack(struct addrspace *as, vaddr_t *stackptr)
                - define the stack region within the given address space

        * static int append_region(struct addrspace *as, 
                        int permissions, vaddr_t start, size_t size)
                - create a region
                - append it to the address space regions

        * int region_type(struct addrspace *as, vaddr_t addr)
                - return what type of region an address lies in

        * int region_perms(struct addrspace *as, vaddr_t addr)
                - return the permissions of a region
