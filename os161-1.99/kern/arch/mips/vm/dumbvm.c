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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

#include "opt-A3.h"

// No longer need we are looking for any n pages instead
// of n contiguous pages
// #if OPT_A3
// struct CoreMap {
// 	unsigned int avail;
// 	paddr_t paddr;
// };
// #endif

#if OPT_A3
	static unsigned long num_of_frames = 0;
	static paddr_t physmap_begin = 0;
	static paddr_t physmap_end = 0;
	static bool physmap_ready = false;
	static struct spinlock physmap_lock = SPINLOCK_INITIALIZER;
#endif

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

void vm_bootstrap(void) {
	#if OPT_A3
		// No longer need we are looking for any n pages instead
		// of n contiguous pages
		// paddr_t lo, hi;
		// ram_getsize(&lo, &hi);
		// num_of_frames = (hi - lo)/PAGE_SIZE;
		// phys_map = (struct CoreMap*)PADDR_TO_KVADDR(lo);
		// lo = lo + (sizeof(struct CoreMap) * num_of_frames);
		// lo = ROUNDUP(lo, PAGE_SIZE);
		// num_of_frames = (hi - lo)/PAGE_SIZE;
		// unsigned long ind = 0;
		// while (ind < num_of_frames) {
		// 	(*(phys_map + ind)).avail = 0;
		// 	(*(phys_map + ind)).paddr = lo;
		// 	ind = ind + 1;
		// 	lo = lo + PAGE_SIZE;
		// }
		// physmap_ready = true;
		ram_getsize(&physmap_begin, &physmap_end);
		num_of_frames = physmap_end - physmap_begin;
		num_of_frames = num_of_frames / PAGE_SIZE;
		for (unsigned long ind = 0; ind < num_of_frames; ++ind) {
			((int*)PADDR_TO_KVADDR(physmap_begin))[ind] = 0;
		}
		physmap_ready = true;
	#endif
}

static paddr_t getppages(unsigned long npages) {
	paddr_t paddr = 0;
	#if OPT_A3
		// No longer need we are looking for any n pages instead
		// of n contiguous pages
		// if (physmap_ready == true) {
		// 	unsigned long pagelast = 0;
		// 	unsigned long npagesleftover = npages;
		// 	for (unsigned long npage = 0; npage < num_of_frames; npage++) {
		// 		if (phys_map[npage].avail > 0) {
		// 			npage = npage + phys_map[npage].avail;
		// 			npage = npage - 1;
		// 			npagesleftover = npages;
		// 		} else { npagesleftover = npagesleftover - 1; }

		// 		if (npagesleftover == 0) {
		// 			pagelast = npage;
		// 			break;
		// 		}
		// 	}
		// 	pagelast = pagelast + 1;
		// 	for (npagesleftover = 1; npagesleftover < npages+1; npagesleftover++) {
		// 		pagelast = pagelast - 1;
		// 		phys_map[pagelast].avail = npagesleftover;
		// 	}
		// 	paddr = phys_map[pagelast].paddr;
		// } else { paddr = ram_stealmem(npages); }
		if (physmap_ready == true) {
			spinlock_acquire(&physmap_lock);
			bool found = false;
			unsigned long ind_begin = 0;
			while (ind_begin < num_of_frames) {
				unsigned long ind = ind_begin;
				unsigned long num_of_pages = 0;
				int pos = ((int*)PADDR_TO_KVADDR(physmap_begin))[ind_begin];
				while ((pos == 0) && ((num_of_pages + ind) < num_of_frames)) {
					if (num_of_pages + 1 == npages) {
						ind_begin = ind;
						unsigned long j = 0;
						j = j + 1;
						while (j < npages + 1) {
							((int*)PADDR_TO_KVADDR(physmap_begin))[ind_begin] = (int)j;
							j = j + 1;
							ind_begin = ind_begin + 1;
						}
						paddr = (ind + 1)*PAGE_SIZE;
						paddr = paddr + physmap_begin;
						found = true;
						break;
					}
					num_of_pages = num_of_pages + 1;
					pos = ((int*)PADDR_TO_KVADDR(physmap_begin))[ind_begin+1];
					ind_begin = ind_begin + 1;
				}
				if (found == true) {
					break;
				}
				ind_begin = ind_begin + 1;
			}
			spinlock_release(&physmap_lock);

		} else {
			spinlock_acquire(&stealmem_lock);
			paddr = ram_stealmem(npages);
			spinlock_release(&stealmem_lock);
		}
	
	#else
		spinlock_acquire(&stealmem_lock);
		paddr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);

	#endif

	return paddr;
}

// #if OPT_A3
// void putppages(paddr_t paddr) {
// 	// Opposite of getppages
// 	int tot = 0;
// 	int pos = -1;
// 	if (phys_ready == true) {
// 		spinlock_acquire(&stealmem_lock);
// 		unsigned long ind = 0;
// 		while (ind < num_of_frames) {
// 			if (phys_map[ind].paddr == paddr) {
// 				pos = ind;
// 				tot = phys_map[pos].avail;
// 				break;
// 			}
// 			ind = ind + 1;
// 		}
// 		if (pos != -1) {
// 			tot = tot + pos;
// 			while (pos < tot) {
// 				phys_map[pos].avail = 0;
// 				pos = pos + 1;
// 			}
// 		} else { kprintf("Erro: given addr not found"); }
// 		spinlock_release(&stealmem_lock);
// 	}
// }
// #endif

/* Allocate/free some kernel-space virtual pages */
vaddr_t alloc_kpages(int npages) {
	paddr_t pa;
	pa = getppages(npages);
	if (pa == 0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr) {
	#if OPT_A3
		// No longer need we are looking for any n pages instead
		// of n contiguous pages
		// paddr_t paddr = KVADDR_TO_PADDR(addr);
		// int tot = 0;
		// int pos = -1;
		// if (physmap_ready == true) {
		// 	spinlock_acquire(&stealmem_lock);
		// 	unsigned long ind = 0;
		// 	while (ind < num_of_frames) {
		// 		if (phys_map[ind].paddr == paddr) {
		// 			pos = ind;
		// 			tot = phys_map[ind].avail;
		// 			break;
		// 		}
		// 		ind = ind + 1;
		// 	}

		// 	if (pos >= 0) {
		// 		tot = tot + pos;
		// 		while (pos < tot) {
		// 			phys_map[pos].avail = 0;
		// 			pos = pos + 1;
		// 		}
		// 	}
		// 	spinlock_release(&stealmem_lock);
		// }

		paddr_t paddr = KVADDR_TO_PADDR(addr);
		spinlock_acquire(&physmap_lock);
		unsigned long ind_begin = paddr - physmap_begin;
		ind_begin = ind_begin / PAGE_SIZE - 1;
		unsigned long ind = ind_begin;
		int pos = -1;
		while ((pos != 1) && (pos != 0)) {
			((int*)PADDR_TO_KVADDR(physmap_begin))[ind] = 0;
			pos = ((int*)PADDR_TO_KVADDR(physmap_begin))[ind+1];
			ind = ind + 1;
		}
		spinlock_release(&physmap_lock);

	#else
		/* nothing - leak the memory. */

		(void)addr;

	#endif
	return;
}

void vm_tlbshootdown_all(void) {
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void vm_tlbshootdown(const struct tlbshootdown *ts) {
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int vm_fault(int faulttype, vaddr_t faultaddress) {
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		#if OPT_A3
			return EROFS;
		#else
			/* We always create pages read-write, so we can't get this */
			panic("dumbvm: got VM_FAULT_READONLY\n");
		#endif
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	#if OPT_A3
		KASSERT(as->as_vbase1 != 0);
		KASSERT(as->as_pbase1 != NULL);
		KASSERT(as->as_npages1 != 0);
		KASSERT(as->as_vbase2 != 0);
		KASSERT(as->as_pbase2 != NULL);
		KASSERT(as->as_npages2 != 0);
		KASSERT(as->as_stackpbase != NULL);
		KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
		KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	#else
		KASSERT(as->as_vbase1 != 0);
		KASSERT(as->as_pbase1 != 0);
		KASSERT(as->as_npages1 != 0);
		KASSERT(as->as_vbase2 != 0);
		KASSERT(as->as_pbase2 != 0);
		KASSERT(as->as_npages2 != 0);
		KASSERT(as->as_stackpbase != 0);
		KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
		KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
		KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
		KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
		KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
	#endif

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	#if OPT_A3
		bool is_code;
		if (faultaddress >= vbase1 && faultaddress < vtop1) {
			paddr = (faultaddress - vbase1) + as->as_pbase1[0];
			is_code = true;
		}
		else if (faultaddress >= vbase2 && faultaddress < vtop2) {
			paddr = (faultaddress - vbase2) + as->as_pbase2[0];
			is_code = false;
		}
		else if (faultaddress >= stackbase && faultaddress < stacktop) {
			paddr = (faultaddress - stackbase) + as->as_stackpbase[0];
			is_code = false;
		}
		else {
			return EFAULT;
		}
	#else
		if (faultaddress >= vbase1 && faultaddress < vtop1) {
			paddr = (faultaddress - vbase1) + as->as_pbase1;
		}
		else if (faultaddress >= vbase2 && faultaddress < vtop2) {
			paddr = (faultaddress - vbase2) + as->as_pbase2;
		}
		else if (faultaddress >= stackbase && faultaddress < stacktop) {
			paddr = (faultaddress - stackbase) + as->as_stackpbase;
		}
		else {
			return EFAULT;
		}
	#endif

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		#if OPT_A3
			if (as->as_loaded == true && is_code == true) {
				// Load TLB entries for text seg with TLBLO_DIRTY off
				elo &= ~TLBLO_DIRTY;
			}
		#endif
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	#if OPT_A3
		// Write ehi and elo values to random TLB slot instead of error
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		ehi = faultaddress;
		// Check if text segment is read-only
		if (as->as_loaded == true && is_code == true) {
			// Load TLB entries for text seg with TLBLO_DIRTY off
			elo &= ~TLBLO_DIRTY;
		}
		// Write ehi and elo values to random TLB slot
		tlb_random(ehi, elo);
		splx(spl);
		return 0;
	#else
		kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
		splx(spl);
		return EFAULT;
	#endif
}

struct addrspace *as_create(void) {
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	#if OPT_A3
		as->as_vbase1 = 0;
		as->as_pbase1 = NULL;
		as->as_npages1 = 0;
		as->as_vbase2 = 0;
		as->as_pbase2 = NULL;
		as->as_npages2 = 0;
		as->as_stackpbase = NULL;
		as->as_loaded = false;
	#else
		as->as_vbase1 = 0;
		as->as_pbase1 = 0;
		as->as_npages1 = 0;
		as->as_vbase2 = 0;
		as->as_pbase2 = 0;
		as->as_npages2 = 0;
		as->as_stackpbase = 0;
		// #if OPT_A3
		// 	as->as_loaded = true;
		// #endif
	#endif

	return as;
}

void as_destroy(struct addrspace *as) {
	#if OPT_A3
		// free_kpages(PADDR_TO_KVADDR(as->as_pbase1));
		// free_kpages(PADDR_TO_KVADDR(as->as_pbase2));
		// free_kpages(PADDR_TO_KVADDR(as->as_stackpbase));
		
		unsigned long ind = 0;
		while (ind < as->as_npages1) {
			free_kpages(PADDR_TO_KVADDR(as->as_pbase1[ind]));
			ind = ind + 1;
		}
		as->as_pbase1 = NULL;
		ind = 0;
		while (ind < as->as_npages2) {
			free_kpages(PADDR_TO_KVADDR(as->as_pbase2[ind]));
			ind = ind + 1;
		}
		as->as_pbase2 = NULL;
		ind = 0;
		while (ind < DUMBVM_STACKPAGES) {
			free_kpages(PADDR_TO_KVADDR(as->as_stackpbase[ind]));
			ind = ind + 1;
		}
		as->as_stackpbase = NULL;
		kfree(as);
		as = NULL;

	#else
		kfree(as);
		as = NULL;

	#endif
}

void as_activate(void) {
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void as_deactivate(void) {
	/* nothing */
}

int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
					 int readable, int writeable, int executable) {
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		#if OPT_A3
			as->as_pbase1 = kmalloc((sizeof(paddr_t))*(as->as_npages1));
		#endif
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		#if OPT_A3
			as->as_pbase2 = kmalloc((sizeof(paddr_t))*(as->as_npages2));
		#endif
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static void as_zero_region(paddr_t paddr, unsigned npages) {
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int as_prepare_load(struct addrspace *as) {
	#if OPT_A3
		// KASSERT(as->as_pbase1 == NULL);
		// KASSERT(as->as_pbase2 == NULL);
		// KASSERT(as->as_stackpbase == NULL);

		unsigned long ind = 0;
		while (ind < as->as_npages1) {
			paddr_t paddr = getppages(1);
			if (paddr == 0) {
				return ENOMEM;
			}
			as->as_pbase1[ind] = paddr;
			as_zero_region(as->as_pbase1[ind], 1);
			ind = ind + 1;
		}

		ind = 0;
  		while (ind < as->as_npages2) {
			paddr_t paddr = getppages(1);
			if (paddr == 0) {
				return ENOMEM;
			}
			as->as_pbase2[ind] = paddr;
			as_zero_region(as->as_pbase2[ind], 1);
			ind = ind + 1;
  		}

		ind = 0;
		as->as_stackpbase = kmalloc((sizeof(paddr_t))*DUMBVM_STACKPAGES);
		if (as->as_stackpbase == NULL) {
			return ENOMEM;
		}
		while (ind < DUMBVM_STACKPAGES) {
			paddr_t paddr = getppages(1);
			if (paddr == 0) {
				return ENOMEM;
			}
			as->as_stackpbase[ind] = paddr;
			as_zero_region(as->as_stackpbase[ind], 1);
			ind = ind + 1;
		}

	#else
		KASSERT(as->as_pbase1 == 0);
		KASSERT(as->as_pbase2 == 0);
		KASSERT(as->as_stackpbase == 0);

		as->as_pbase1 = getppages(as->as_npages1);
		if (as->as_pbase1 == 0) {
			return ENOMEM;
		}

		as->as_pbase2 = getppages(as->as_npages2);
		if (as->as_pbase2 == 0) {
			return ENOMEM;
		}

		as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
		if (as->as_stackpbase == 0) {
			return ENOMEM;
		}
		
		as_zero_region(as->as_pbase1, as->as_npages1);
		as_zero_region(as->as_pbase2, as->as_npages2);
		as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	#endif

	return 0;
}

int as_complete_load(struct addrspace *as) {
	// #if OPT_A3
	// 	as->as_loaded = false;
	// #else
	// 	(void)as;
	// #endif
	(void)as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr) {
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int as_copy(struct addrspace *old, struct addrspace **ret) {
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	#if OPT_A3
		new->as_pbase1 = kmalloc((sizeof(paddr_t))*(old->as_npages1));
		new->as_vbase1 = old->as_vbase1;
		new->as_npages1 = old->as_npages1;
		new->as_pbase2 = kmalloc((sizeof(paddr_t))*(old->as_npages2));
		new->as_vbase2 = old->as_vbase2;
		new->as_npages2 = old->as_npages2;
		new->as_stackpbase = kmalloc((sizeof(paddr_t))*DUMBVM_STACKPAGES);
	#else
		new->as_vbase1 = old->as_vbase1;
		new->as_npages1 = old->as_npages1;
		new->as_vbase2 = old->as_vbase2;
		new->as_npages2 = old->as_npages2;
	#endif

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	#if OPT_A3
		KASSERT(new->as_pbase1 != NULL);
		KASSERT(new->as_pbase2 != NULL);
		KASSERT(new->as_stackpbase != NULL);
		unsigned long ind = 0;
		while (ind < old->as_npages1) {
			memmove((void*)PADDR_TO_KVADDR(new->as_pbase1[ind]),
					(const void*)PADDR_TO_KVADDR(old->as_pbase1[ind]),
					PAGE_SIZE);
			ind = ind + 1;
		}
		ind = 0;
		while (ind < old->as_npages2) {
			memmove((void*)PADDR_TO_KVADDR(new->as_pbase2[ind]),
					(const void*)PADDR_TO_KVADDR(old->as_pbase2[ind]),
					PAGE_SIZE);
			ind = ind + 1;
		}
		ind = 0;
		while (ind < DUMBVM_STACKPAGES) {
			memmove((void*)PADDR_TO_KVADDR(new->as_stackpbase[ind]),
					(const void*)PADDR_TO_KVADDR(old->as_stackpbase[ind]),
					PAGE_SIZE);
			ind = ind + 1;
		}
	#else
		KASSERT(new->as_pbase1 != 0);
		KASSERT(new->as_pbase2 != 0);
		KASSERT(new->as_stackpbase != 0);
		
		memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
				(const void *)PADDR_TO_KVADDR(old->as_pbase1),
				old->as_npages1*PAGE_SIZE);

		memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
				(const void *)PADDR_TO_KVADDR(old->as_pbase2),
				old->as_npages2*PAGE_SIZE);

		memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
				(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
				DUMBVM_STACKPAGES*PAGE_SIZE);
	#endif
	
	*ret = new;
	return 0;
}
