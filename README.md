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

