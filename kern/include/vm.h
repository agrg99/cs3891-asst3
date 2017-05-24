/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VM_H_
#define _VM_H_

#include <machine/vm.h>
#include <addrspace.h>

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

#define VM_INVALID_INDEX	 -1	  /* invalid pointer index */

#define PAGE_SIZE 4096	/* page size for hpt */
#define PAGE_BITS 20	/* number of bits in vpn */

/* ------------------------------------------------------------------------- */

/* layout of a frame table entry */
struct frame_entry {
	int		fe_refcount;		/* number of references to this frame */
	char	fe_used;			/* flag to indicate if this frame is free */
	int		fe_next;			/* if this frame is free, index of next free */
};

/* pointer to the frame table */
struct frame_entry *ft;					

/* layout of a page table entry */
struct page_entry {
	uint32_t	pe_vpn;						/* the vpn of the entry */
	uint32_t	pe_proc_id;					/* the process id */
	uint32_t	pe_ppn;						/* the frame table frame num */
	char		pe_flags;					/* page permissions and flags */
	uint32_t	pe_next;					/* pointer to collion next entry */
};

/* pointer to the hashed page table */
struct page_entry *hpt;

/* number of entries in the page table */
unsigned int hpt_size;	

/* the index for the top level free frame in the frame table */
int cur_free;

/* ------------------------------------------------------------------------- */

/* Initialization function */
void vm_bootstrap(void);

/* init the frametable */
void frametable_init(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* purge hpt and ft for frames belonging to an as */
void purge_hpt(struct addrspace *as);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *);


#endif /* _VM_H_ */
