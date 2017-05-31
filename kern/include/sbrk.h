vaddr_t            sbrk(intptr_t amount);
vaddr_t            create_heap(struct addrspace *as);
struct region   *get_heap(struct addrspace *as);
vaddr_t            get_heap_address(struct addrspace *as);
