#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#ifdef CS333_P2
#include "uproc.h"
#endif

#ifdef CS333_P3P4
struct StateLists {
  struct proc* ready;
  struct proc* readyTail;
  struct proc* free;
  struct proc* freeTail;
  struct proc* sleep;
  struct proc* sleepTail;
  struct proc* zombie;
  struct proc* zombieTail;
  struct proc* running;
  struct proc* runningTail;
  struct proc* embryo;
  struct proc* embryoTail;
};
#endif

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  #ifdef CS333_P3P4
  struct StateLists pLists;
  #endif 
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);


#ifdef CS333_P3P4
static void initProcessLists(void);
static void initFreeList(void);
static int stateListAdd(struct proc** head, struct proc** tail, struct proc* p);
static int stateListRemove(struct proc** head, struct proc** tail, struct proc* p);
#endif


static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};


#ifdef CS333_P3P4
static void
assertState(struct proc *p, enum procstate state)
{	
  if (p == 0)
  {
    panic("The given process is NULL");
  }
  else if (p->state != state)
  {
    cprintf("Expected State: %s\t State Received: %s\n", states[state], states[p->state]);
    panic("States do not match!");
  }
}
#endif

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
  acquire(&ptable.lock);
#ifndef CS333_P3P4
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
#else
  int rc = 0;
  p = ptable.pLists.free;
  if (p)
    goto found;
#endif
  release(&ptable.lock);
  return 0;

found:
#ifndef CS333_P3P4  
  p->state = EMBRYO;
  p->pid = nextpid++;
#else  
  rc = stateListRemove(&ptable.pLists.free, &ptable.pLists.freeTail,p);
  if (rc < 0)
  {
    panic("Embryo List Remove failed in allocproc");
  }
  p->state = EMBRYO;
  assertState(p,EMBRYO);
  p->pid = nextpid++;
  stateListAdd(&ptable.pLists.embryo, &ptable.pLists.embryoTail, p);
#endif 
  
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0)
  {

#ifndef CS333_P3P4
    p->state = UNUSED;
    return 0;
#else 
  //if kalloc fails, remove the node from embryo list and put it back on free list 
  acquire(&ptable.lock);
  rc = stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryoTail,p);
  if (rc < 0)
  {
    panic("Embryo List Remove failed in allocproc");
  }
  p->state = UNUSED;
  assertState(p, UNUSED);
  stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail, p);
  release(&ptable.lock); 
  return 0;
#endif
  
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  #ifdef CS333_P1
  p->start_ticks = ticks;
  #endif
  
  #ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  #endif
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
#ifdef CS333_P3P4
  
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif 
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  // Initialise values at the boot time
#ifdef CS333_P2
  p->uid = UID;
  p->gid = GID;
#endif

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
#ifndef CS333_P3P4
  p->state = RUNNABLE;
#else
  acquire(&ptable.lock);
  int rc = stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryoTail,p); 
  if (rc < 0)
  {
    panic("Embryo List Remove failed in userinit");
  }
  p->state = RUNNABLE;
  assertState(p, RUNNABLE);
  stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail,p);
  release(&ptable.lock);
#endif 
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
#ifndef CS333_P3P4
    np->state = UNUSED;
#else
    acquire(&ptable.lock);
    int rc = stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryoTail,np); 
    if (rc < 0)
    {
      panic("Embryo List Remove failed in fork");
    }
    np->state = UNUSED;
    assertState(np, UNUSED);
    stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail,np);
    release(&ptable.lock);
#endif
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;
 // Copy the uid and gid of the current process to the  new child process
#ifdef CS333_P2
  np->uid = proc->uid;
  np->gid = proc->gid;
#endif
  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
#ifndef CS333_P3P4
  np->state = RUNNABLE;
#else
  int rc = stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryoTail,np); 
  if (rc < 0)
  {
    panic("Embryo List Remove failed in fork");
  }
  np->state = RUNNABLE;
  assertState(np, RUNNABLE);
  stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail,np);
#endif
  release(&ptable.lock);
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifndef CS333_P3P4
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  p = ptable.pLists.zombie;
  while (p)
  {
    if(p->parent == proc){
      p->parent = initproc;
      wakeup1(initproc);
    }
    p = p->next;
  }

  p = ptable.pLists.ready;
  while(p)
  {
    if(p->parent == proc)
    {
      p->parent = initproc;
    }
    p = p->next;
  }


  p = ptable.pLists.sleep;
  while(p)
  {
    if(p->parent == proc)
    {
      p->parent = initproc;
    }
    p = p->next;
  }

  p = ptable.pLists.running;
  while(p)
  {
    if(p->parent == proc)
    {
      p->parent = initproc;
    }
    p = p->next;
  }
  int rc = stateListRemove(&ptable.pLists.running, &ptable.pLists.runningTail, proc);
  if (rc < 0)
  {
    panic("Running List Remove failed in exit");
  }
  proc->state = ZOMBIE;
  assertState(proc, ZOMBIE);
  stateListAdd(&ptable.pLists.zombie, &ptable.pLists.zombieTail,proc);
  
  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");

}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifndef CS333_P3P4
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }


    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    p = ptable.pLists.zombie;
    while(p)
    {
      if(p->parent != proc)
      {
        p = p->next;
        continue;
      }
      else
      {
        havekids = 1;
      
        if(p->state == ZOMBIE)
	{
          // Found one.
          pid = p->pid;
          kfree(p->kstack);
          p->kstack = 0;
          freevm(p->pgdir);
          int rc = stateListRemove(&ptable.pLists.zombie, &ptable.pLists.zombieTail,p); 
          if (rc < 0)
          {
            panic("Zombie List Remove failed in wait");
          }
          p->state = UNUSED;
          assertState(p, UNUSED);
	  stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail,p); 
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          release(&ptable.lock);
          return pid;
        }
      }
      p = p->next;
    }

    p = ptable.pLists.ready;
    while(p)
    {
      if(p->parent != proc)
      {
         p = p->next;
         continue;
      }
      else
      {
        havekids = 1; 
      }
      p = p->next;
    }
    
    p = ptable.pLists.running;
    while(p)
    {
      if(p->parent != proc)
      {
        p = p->next;
        continue;
      }
      else
      {
        havekids = 1; 
      }
      p = p->next;
    }
    
    p = ptable.pLists.sleep;
    while(p)
    {
      if(p->parent != proc)
      {
        p = p->next;
        continue;
      }
      else
      {
        havekids = 1; 
      }
      p = p->next;
    }
    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifndef CS333_P3P4
// original xv6 scheduler. Use if CS333_P3P4 NOT defined.
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks; // value set to ticks when process enters cpu
      #endif
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}

#else
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    p = ptable.pLists.ready;
    if (p)
    //while(p)
    {

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);

      //struct proc * temp = p->next;
      int rc = stateListRemove(&ptable.pLists.ready, &ptable.pLists.readyTail,p); 
      if (rc < 0)
      {
        panic("Ready List Remove failed in scheduler");
      }
      p->state = RUNNING;
      assertState(p, RUNNING);
      stateListAdd(&ptable.pLists.running, &ptable.pLists.runningTail,p);
      //p = temp;
      #ifdef CS333_P2
      p->cpu_ticks_in = ticks; // value set to ticks when process enters cpu
      #endif
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }

}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  #ifdef CS333_P2
  proc->cpu_ticks_total += ticks - proc->cpu_ticks_in;
  #endif
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
#ifndef CS333_P3P4
void 
yield(void)
{
  acquire(&ptable.lock);
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#else
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  
  int rc = stateListRemove(&ptable.pLists.running, &ptable.pLists.runningTail, proc); 
  if (rc < 0)
  {
    panic("Running List Remove failed in yield");
  }
  proc->state = RUNNABLE;
  assertState(proc, RUNNABLE);
  stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, proc); 
  sched();
  release(&ptable.lock);
}
#endif
// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 2016/12/28: ticklock removed from xv6. sleep() changed to
// accept a NULL lock to accommodate.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    if (lk) release(lk);
  }

  // Go to sleep.
#ifndef CS333_P3P4
  proc->chan = chan;
  proc->state = SLEEPING;
#else
  int rc = stateListRemove(&ptable.pLists.running, &ptable.pLists.runningTail, proc);
  if (rc < 0)
  {
    panic("Running List Remove failed in sleep");
  }
  //stateListAdd(&ptable.pLists.sleep, &ptable.pLists.sleepTail, proc);
  proc->chan = chan;
  proc->state = SLEEPING;
  assertState(proc, SLEEPING);
  stateListAdd(&ptable.pLists.sleep, &ptable.pLists.sleepTail, proc);
#endif
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//PAGEBREAK!
#ifndef CS333_P3P4
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  p = ptable.pLists.sleep;
  while(p)
  {
    if(p->chan == chan)
    {
      struct proc * temp = p->next;
      int rc = stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleepTail, p);
      if (rc < 0)
      {
        panic("Sleep List Remove failed in wakeup1");
      }
      p->state = RUNNABLE;
      assertState(p, RUNNABLE);
      stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, p);
      p = temp;
    }
    else
    {
      p = p->next;
    }
  }
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifndef CS333_P3P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock); 
  
  p = ptable.pLists.embryo;
  while(p)
  {
    if(p->pid == pid)
    {
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    else
    {
      p = p->next;
  
    }
  }
  p = ptable.pLists.running;
  while(p)
  {
    if(p->pid == pid)
    {
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    else
    {
      p = p->next;
  
    }
  }
  
  p = ptable.pLists.zombie;
  while(p)
  {
    if(p->pid == pid)
    {
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    else
    {
      p = p->next;
  
    }
  }
  
  p = ptable.pLists.ready;
  while(p)
  {
    if(p->pid == pid)
    {
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    else
    {
      p = p->next;
  
    }
  }
  
  p = ptable.pLists.sleep;
  while(p)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      int rc = stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleepTail, p);
      if (rc < 0)
      {
        panic("Sleep List Remove failed in kill");
      }
      p->state = RUNNABLE;
      assertState(p, RUNNABLE);
      stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, p);
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }

  release(&ptable.lock);

  return 0;  // placeholder
}
#endif




//NOTE: I moved the states array towards the top of 
//this file so that I can use it in assertState.


//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
//#ifdef CS333_P1
//  uint finish_ticks;
//  uint elapsed_ticks;
//  uint time_sec;
//  uint time_rem;
//  cprintf("%s\t%s\t%s\t%s\t %s\n", "PID", "State", "Name","Elapsed","PCs");
//#endif
  

#ifdef CS333_P2
  cprintf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t %s\n", "PID", "NAME", "UID","GID","PPID", "Elapsed", "CPU","State", "Size","PCs");
#endif


  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

#ifdef CS333_P2 
    cprintf("%d\t%s\t%d\t%d", p->pid, p->name, p->uid, p->gid);
    if (p->parent)
      cprintf("\t%d\t", p->parent->pid);
    else
      cprintf("\t%d\t", p->pid); 
    
    print_time(ticks - p->start_ticks);
    print_time(p->cpu_ticks_total);
    cprintf("%s\t%d\t", state, p->sz);
#else

    cprintf("\n%s\n",state);
#endif

/*
#ifdef CS333_P1
    cprintf("%d\t%s\t%s", p->pid, state, p->name);
    finish_ticks = ticks;
    elapsed_ticks = finish_ticks - p->start_ticks;
    print_time(elapsed_ticks);
     time_rem = elapsed_ticks % 1000;
    time_sec = elapsed_ticks / 1000;
    if (time_rem <10)
    {
    	cprintf("\t%d.00%d\t", time_sec, time_rem);
    }
    else if ( time_rem > 10 && time_rem <100)
    {
	cprintf("\t%d.0%d\t", time_sec, time_rem);
    }
    else
    {
    	cprintf("\t%d.%d\t", time_sec, time_rem);
    }
#endif
  */  
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
	      cprintf(" %p", pc[i]);
    }
    
    cprintf("\n");
  }
}

#if defined(CS333_P2) || defined(CS333_P1)
void
print_time(int elapsed_ticks)
{
    int time_rem = elapsed_ticks % 1000;
    int time_sec = elapsed_ticks / 1000;
    if (time_rem <10)
    {
    	cprintf("%d.00%d\t", time_sec, time_rem);
    }
    else if ( time_rem > 10 && time_rem <100)
    {
	cprintf("%d.0%d\t", time_sec, time_rem);
    }
    else
    {
    	cprintf("%d.%d\t", time_sec, time_rem);
    }
}
#endif


#ifdef CS333_P3P4
void
readydump(void)
{
  struct proc * p;
  p = ptable.pLists.ready;
  if(p)
  {
    cprintf("Ready List Processes: \n");
    cprintf("%d", p->pid);
    p = p->next;
  }
  else
  {
    cprintf("Ready List is empty");
  }
  while(p)
  {
    cprintf("->%d",p->pid);
    p = p->next;
  }
  cprintf("\n");
}

	
void
freedump(void)
{
  struct proc * p;
  p = ptable.pLists.free;
  int count = 0;
  while(p)
  {
    ++count;
    p = p->next;
  }
  cprintf("Free List Size: %d processes\n", count);
}


void
sleepdump(void)
{
  struct proc * p;
  p = ptable.pLists.sleep;
  if(p)
  {
    cprintf("Sleep List Processes: \n");
    cprintf("%d", p->pid);
    p = p->next;
  }
  else
  {
    cprintf("Sleep list is empty");
  }
  while(p)
  {
    cprintf("->%d",p->pid);
    p = p->next;
  }
  cprintf("\n");
}


void
zombiedump(void)
{
  struct proc * p;
  p = ptable.pLists.zombie;
  if(p)
  {
    cprintf("Zombie List Processes: \n");
    cprintf("(%d", p->pid);
    cprintf(")");
    p = p->next;
  }
  else
  {
    cprintf("Zombie List is Empty");
  }
  while(p)
  {
    cprintf("->(%d",p->pid);
    if (p->parent)
    {
      cprintf("%d)", p->parent->pid);
    }
    else
    {
      cprintf("%d)", p->pid);
    }
    p = p->next;
  }
  cprintf("\n");
}
#endif


#ifdef CS333_P2
// Runs when user types ps on console.
int 
getprocs_helper(uint max, struct uproc * up)
{
  int i = 0;
  struct proc *p;
  
 //char *state;

  acquire(&ptable.lock);
  for(i=0, p = ptable.proc; p < &ptable.proc[NPROC] && i < max; p++)
  {
    if(p->state == UNUSED || p->state == EMBRYO)
      continue;
   
    up[i].pid = p->pid;
    up[i].uid = p->uid;
    up[i].gid = p->gid;
    if (p->parent)
    {
	    up[i].ppid = p->parent->pid;
    }
    else
    {
	    up[i].ppid = p->pid;
    }

    up[i].elapsed_ticks = ticks- p->start_ticks;
    up[i].CPU_total_ticks = p->cpu_ticks_total;
    safestrcpy(up[i].state, states[p->state], sizeof(up[i].state));
    up[i].size = p->sz;
    safestrcpy(up[i].name, p->name, sizeof(up[i].name));    
    ++i;

  
  }
  release(&ptable.lock);
  return i;
}
#endif



#ifdef CS333_P3P4
static int
stateListAdd(struct proc** head, struct proc** tail, struct proc* p)
{
  if (*head == 0) {
    *head = p;
    *tail = p;
    p->next = 0;
  } else {
    (*tail)->next = p;
    *tail = (*tail)->next;
    (*tail)->next = 0;
  }

  return 0;
}

static int
stateListRemove(struct proc** head, struct proc** tail, struct proc* p)
{
  if (*head == 0 || *tail == 0 || p == 0) {
    return -1;
  }

  struct proc* current = *head;
  struct proc* previous = 0;

  if (current == p) {
    *head = (*head)->next;
    return 0;
  }

  while(current) {
    if (current == p) {
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if (current == 0) {
    return -1;
  }

  // Process found. Set the appropriate next pointer.
  if (current == *tail) {
    *tail = previous;
    (*tail)->next = 0;
  } else {
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = 0;

  return 0;
}

static void
initProcessLists(void) {
  ptable.pLists.ready = 0;
  ptable.pLists.readyTail = 0;
  ptable.pLists.free = 0;
  ptable.pLists.freeTail = 0;
  ptable.pLists.sleep = 0;
  ptable.pLists.sleepTail = 0;
  ptable.pLists.zombie = 0;
  ptable.pLists.zombieTail = 0;
  ptable.pLists.running = 0;
  ptable.pLists.runningTail = 0;
  ptable.pLists.embryo = 0;
  ptable.pLists.embryoTail = 0;
}

static void
initFreeList(void) {
  if (!holding(&ptable.lock)) {
    panic("acquire the ptable lock before calling initFreeList\n");
  }

  struct proc* p;

  for (p = ptable.proc; p < ptable.proc + NPROC; ++p) {
    p->state = UNUSED;
    assertState(p, UNUSED);
    stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail, p);
  }
}
#endif
