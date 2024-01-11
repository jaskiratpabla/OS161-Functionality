#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include "opt-A2.h"
#if OPT_A2
  // ASST2a
  #include <mips/trapframe.h>
#endif
#if OPT_A2
  // ASST2b
  #include <vfs.h>
  #include <kern/fcntl.h>
#endif

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  #if OPT_A2
    // ASST2a
    while (array_num(p->p_children) >= 1) {
      struct proc *temp_child_proc = array_get(p->p_children, 0);
      array_remove(p->p_children, 0);
      spinlock_acquire(&temp_child_proc->p_lock);
      if (temp_child_proc->p_exitstatus != 0) {
        spinlock_release(&temp_child_proc->p_lock);
        proc_destroy(temp_child_proc);
      } else {
        temp_child_proc->p_parent = NULL;
        spinlock_release(&temp_child_proc->p_lock);
      }
    }
  #else
    /* for now, just include this to keep the compiler from complaining about
       an unused variable */
    (void)exitcode;
  #endif

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  #if OPT_A2
    // ASST2a
    spinlock_acquire(&p->p_lock);
    if (p->p_parent == NULL) {
      spinlock_release(&p->p_lock);
      proc_destroy(p);
    } else {
      p->p_exitstatus = 1;
      p->p_exitcode = _MKWAIT_EXIT(exitcode);
      spinlock_release(&p->p_lock);
      lock_acquire(p->p_lck);
      cv_broadcast(p->p_cv, p->p_lck);
      lock_release(p->p_lck);
    }
  #else
    /* if this is the last user process in the system, proc_destroy()
       will wake up the kernel menu thread */
    proc_destroy(p);
  #endif
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int sys_getpid(pid_t *retval) {
  #if OPT_A2
    // ASST2a
    *retval = curproc->p_pid;
  #else
    /* for now, this is just a stub that always returns a PID of 1 */
    /* you need to fix this to make it work properly */
    *retval = 1;
  #endif
  return 0;
}

/* stub handler for waitpid() system call                */

int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval) {
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

  #if OPT_A2
    // ASST2a
    struct proc *temp_child = NULL;
    int len = array_num(curproc->p_children);
    for (int i = 0; i < len; ++i) {
      temp_child = array_get(curproc->p_children, i);
      if (temp_child->p_pid == pid) {
        array_remove(curproc->p_children, i);
        break;
      }
      temp_child = NULL;
    }
    if (temp_child == NULL) {
      panic("waitpid child not found\n");
      return (ECHILD);
    }

    lock_acquire(temp_child->p_lck);
    while (temp_child->p_exitstatus == 0) {
      cv_wait(temp_child->p_cv, temp_child->p_lck);
    }
    exitstatus = temp_child->p_exitcode;
    lock_release(temp_child->p_lck);
    proc_destroy(temp_child);

  #else
    /* for now, just pretend the exitstatus is 0 */
    exitstatus = 0;
    
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return result;
  }
  *retval = pid;
  return 0;
}


#if OPT_A2
  // ASST2a
  int sys_fork(pid_t *retval, struct trapframe *tf) {
    struct proc *child_proc = proc_create_runprogram("child proc");
    KASSERT(child_proc != NULL);
    child_proc->p_parent = curproc;
    array_init(child_proc->p_children);
    spinlock_acquire(&curproc->p_lock);
    int err = array_add(curproc->p_children, child_proc, NULL);
    spinlock_release(&curproc->p_lock);
    if (err != 0) {
      panic("array add child process unsuccessful\n");
    }
    struct addrspace *new_addr;
    err = as_copy(curproc_getas(), &new_addr);
    if (err != 0) {
      panic("addr copy to child process unsuccessful\n");
    }
    spinlock_acquire(&child_proc->p_lock);
    child_proc->p_addrspace = new_addr;
    spinlock_release(&child_proc->p_lock);
    struct trapframe *child_trapframe = kmalloc(sizeof(struct trapframe));
    KASSERT(child_trapframe != NULL);
    memcpy(child_trapframe, tf, sizeof(struct trapframe));
    err = thread_fork("child thread", child_proc, &enter_forked_process, child_trapframe, 0);
    if (err != 0) {
      panic("child thread fork unsuccessful\n");
    }
    *retval = child_proc->p_pid;
    return 0;
  }
#endif

#if OPT_A2
  // ASST2b
  int sys_execv(char *progname, char **argv) {
    int nargs = 0; // arg count
    struct addrspace *old_as; // old addr space that will be destroyed
    struct addrspace *as;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    // Find arg count
    char **argp = argv; // can't iterate through argv or we lose start
    while (*argp != NULL) {
      nargs++;
      argp++;
    }

    // Copy args before detroying old addr space
    char *newargv[nargs + 1];
    argp = argv;
    int pos = 0;
    char *arg;
    while (*argp != NULL) {
      arg = kmalloc(strlen(*argp) + 1);
      memcpy(arg, *argp, (strlen(*argp) + 1));
      newargv[pos] = arg;
      argp++;
      pos++;
    }

    /* Open the file. */
    result = vfs_open(progname, O_RDONLY, 0, &v);
    if (result) {
      return result;
    }

    as_deactivate(); // unload curproc's address space
    old_as = curproc_getas();

    /* We should be a new process. */
    // KASSERT(curproc_getas() == NULL);

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


    // arg copy out
    int len_of_arg_pos = 0;
    vaddr_t argvp[nargs + 1]; // array of addr
    stackptr = ROUNDUP(stackptr, 8); // stackptr must be 8-byte aligned
    pos = nargs;
    argvp[pos] = stackptr;
    pos--;
    while (pos >= 0) {
      len_of_arg_pos = ROUNDUP((strlen(newargv[pos]) + 1), 4); // + 1 for NULL term
      stackptr -= len_of_arg_pos;
      copyoutstr(newargv[pos], (userptr_t)stackptr, (size_t)len_of_arg_pos, NULL);
      argvp[pos] = stackptr;
      pos--;
    }
    // first copy out NULL termination (which will be at end)
    stackptr -= sizeof(vaddr_t);
    copyout((void*)NULL, (userptr_t)stackptr, (size_t)4);

    // copy backwards from NULL
    pos = nargs - 1;
    while (pos >= 0) {
      stackptr -= sizeof(vaddr_t);
      copyout(&argvp[pos], (userptr_t)stackptr, sizeof(vaddr_t));
      pos--;
    }

    as_destroy(old_as); // destroy old addr space

    /* Warp to user mode (modified with args now) */
    enter_new_process(nargs, (userptr_t)stackptr,
              stackptr, entrypoint);
    
    /* enter_new_process does not return. */
    panic("enter_new_process returned\n");
    return EINVAL;
  }
#endif

