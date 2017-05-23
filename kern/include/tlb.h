#ifndef _TLB_H_
#define _TLB_H_

void insert_tlb(int vpn, int ppn);

/* flush the tlb */
void flush_tlb(void);

#endif /* _TLB_H_ */

