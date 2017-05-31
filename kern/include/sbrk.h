void            *sbrk(intptr_t amount);
void            *create_heap(struct addrspace *as);
struct region   *get_heap(struct addrspace *as);
void            *get_heap_address(struct addrspace *as);
