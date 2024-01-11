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

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>

#include "opt-A2.h"
#if OPT_A2
	// ASST2b
	#include <copyinout.h>
#endif

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
#if OPT_A2
// ASST2b
int runprogram(char *progname, unsigned long nargs, char **args) {
#else
int runprogram(char *progname) {
#endif
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}


	#if OPT_A2
		// ASST2b
		// arg copy out
		int len_of_arg_pos = 0;
		vaddr_t argv[nargs + 1]; // array of addr
		stackptr = ROUNDUP(stackptr, 8); // stackptr must be 8-byte aligned
		int pos = nargs;
		argv[pos] = stackptr;
		pos--;
		while (pos >= 0) {
			len_of_arg_pos = ROUNDUP((strlen(args[pos]) + 1), 4); // + 1 for NULL term
			stackptr -= len_of_arg_pos;
			copyoutstr(args[pos], (userptr_t)stackptr, (size_t)len_of_arg_pos, NULL);
			argv[pos] = stackptr;
			pos--;
		}
		// first copy out NULL termination (which will be at end)
		stackptr -= sizeof(vaddr_t);
		copyout((void*)NULL, (userptr_t)stackptr, (size_t)4);

		// copy backwards from NULL
		pos = nargs - 1;
		while (pos >= 0) {
			stackptr -= sizeof(vaddr_t);
			copyout(&argv[pos], (userptr_t)stackptr, sizeof(vaddr_t));
			pos--;
		}
	#endif

	#if OPT_A2
		// ASST2b
		/* Warp to user mode (modified with args now) */
		enter_new_process(nargs, (userptr_t)stackptr,
						  stackptr, entrypoint);
	#else
		/* Warp to user mode. */
		enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
						  stackptr, entrypoint);
	#endif
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

