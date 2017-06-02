vaddr_t         sbrk(intptr_t amount);
vaddr_t         create_heap(struct addrspace *as, intptr_t amount);
struct region   *get_heap(struct addrspace *as);
vaddr_t         get_heap_address(struct addrspace *as);
void            set_heap(struct addrspace *as);
