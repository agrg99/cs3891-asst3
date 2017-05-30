#ifndef _TLB_H_
#define _TLB_H_

/* insert a tlb entry */
void insert_tlb(int vaddr, int ppn);

/* flush the tlb */
void flush_tlb(void);

/* replace a tlb entry */
void replace_tlb(int vaddr, int ppn);

#endif /* _TLB_H_ */

