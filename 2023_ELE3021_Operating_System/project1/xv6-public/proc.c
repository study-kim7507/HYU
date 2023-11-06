#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "Queue.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// Add Comment : Declare MLFQ
struct MLFQ mlfq;

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
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
  
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
// Add Comment : Function to create the initial process, the init process
void
userinit(void)
{
  // Add Comment : The _L0, _L1, _L2, _SleepQueue queues of the MLFQ are all initialized, and the _globalTicks for Priority Boosting is initialized.
  initQueue(&mlfq._L0, 0);
  initQueue(&mlfq._L1, 1);
  initQueue(&mlfq._L2, 2);
  initQueue(&mlfq._SleepQueue, -1);
  mlfq._globalTicks = 0;
  mlfq._isLocked = NULL;

  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

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

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  
  // Add Comment : Initialize member variables inside the proc structure that represents a process.
  initProc(p);

  // Add Comment : The newly created process is placed in the _L0 queue.
  enQueue(&mlfq._L0, p);
  
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
// Add Comment : After the creation of the initial process, the init process, new processes are created using the fork function.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  // Add Comment : Initialize member variables inside the proc structure that represents a process.
  initProc(np);

  // Add Comment : The newly created process is placed in the _L0 queue.  
  enQueue(&mlfq._L0, np);

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
	
  // Add Comment : If the process that called schedulerLock() exits
  if (mlfq._isLocked == curproc && curproc->_putLock == 1)
  {
	mlfq._isLocked = NULL;
	curproc->_putLock = 0;
  }
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

	// Add Comment : If the process that called the schedulerLock system call exits, initialize the _isLocked variable to indicate that the lock is released.
        if(mlfq._isLocked != NULL && p->_putLock)
	   mlfq._isLocked = NULL;
	
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Add Comment : Priority Boosting, Move all processes to the _L0 queue and initialize timeQuantum, priority, and level.
void
priorityBoosting()
{
  struct proc* p;
  mlfq._globalTicks = 0;

  // Add Comment : If the process that calls schedulerLock is not in the SLEEPING state, insert it at the front of the L0 queue.
  if (mlfq._isLocked != NULL && mlfq._isLocked->state == RUNNABLE)
  {
     deQueueAnywhere(&mlfq._L0, mlfq._isLocked);
     enL0Queue(&mlfq._L0, mlfq._isLocked);
     mlfq._isLocked->_putLock = 0;
     mlfq._isLocked = NULL;
  }
  
  // Add Comment : If the process that calls schedulerLock is in the SLEEPING state, design it so that it is not sent to the L0 queue.
  else if (mlfq._isLocked != NULL && mlfq._isLocked->state == SLEEPING)
  {
     mlfq._isLocked->_putLock = 0;
     mlfq._isLocked = NULL;
  }
  
  // Add Comment :  
  // I implemented PriorityBoosting by connecting the head and tail nodes of _L0, _L1, and _L2 to each other, 
  // but there may be cases where a null pointer is referenced due to an empty queue, so I resolved it by using conditional statements as follows.
 
  // Add Comment : If only the L0 queue is empty.
  if(isEmpty(&mlfq._L0) != 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._head = mlfq._L1._head;
    mlfq._L1._tail->_next = mlfq._L2._head;
    mlfq._L2._head->_prev = mlfq._L1._tail;
    mlfq._L0._tail = mlfq._L2._tail;
  }

  // Add Comment : If only the L1 queue is empty.
  else if(isEmpty(&mlfq._L0) == 0 && isEmpty(&mlfq._L1) != 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._tail->_next = mlfq._L2._head;
    mlfq._L2._head->_prev = mlfq._L0._tail;
    mlfq._L0._tail = mlfq._L2._tail;
  }

  // Add Comment : If only the L2 queue is empty.
  else if(isEmpty(&mlfq._L0) == 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) != 0)
  {
    mlfq._L0._tail->_next = mlfq._L1._head;
    mlfq._L1._head->_prev = mlfq._L0._tail;
    mlfq._L0._tail = mlfq._L1._tail;
  }
  // Add Comment : If the L0 and L1 queues are empty, and the L2 queue is not empty.
  else if(isEmpty(&mlfq._L0) != 0 && isEmpty(&mlfq._L1) != 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._head = mlfq._L2._head;
    mlfq._L0._tail = mlfq._L2._tail;
  }

  // Add Comment : If the L0 and L2 queues are empty, and the L1 queue is not empty.
  else if(isEmpty(&mlfq._L0) != 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) != 0)
  {
    mlfq._L0._head = mlfq._L1._head;
    mlfq._L0._tail = mlfq._L1._tail;
  }

  // Add Comment : If all of the L0, L1, and L2 queues are not empty. 
  else if(isEmpty(&mlfq._L0) == 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._tail->_next = mlfq._L1._head;
    mlfq._L1._head->_prev = mlfq._L0._tail;
    mlfq._L1._tail->_next = mlfq._L2._head;
    mlfq._L2._head->_prev = mlfq._L1._tail;
    mlfq._L0._tail = mlfq._L2._tail;
  }
  
  mlfq._L0._pCount = mlfq._L0._pCount + mlfq._L1._pCount + mlfq._L2._pCount; 

  // Add Comment : All processes are moved to the L0 queue and all member variables are initialized.
  for(p = mlfq._L0._head; p != NULL; p = p->_next)
     initProc(p);

  // Add Comment : Initialize the L1 and L2 queues.
  initQueue(&mlfq._L1, 1);
  initQueue(&mlfq._L2, 2);

  // Add Comment : All processes in the SleepQueue queue are also initialized by resetting all member variables.
  for(p = mlfq._SleepQueue._head; p != NULL; p = p->_next)
     initProc(p);
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
      // Enable interrupts on this processor.
      sti();

      // Loop over process table looking for process to run.
      acquire(&ptable.lock);

      // Add Comment : Take out the process from the front of the queue a d perform scheduling.
      if(mlfq._L0._pCount != 0)
        p = deQueue(&mlfq._L0);
      else if(mlfq._L1._pCount != 0)
        p = deQueue(&mlfq._L1);
      else if(mlfq._L2._pCount != 0)
        p = deQueue(&mlfq._L2);
      else                      // Add Comment : If all processes are in the SLEEPING state, keep looping until a process becomes RUNNABLE state
      {
        release(&ptable.lock);
        continue; 
      }
      
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();
     
      // Add Comment : If the global tick reaches 100, perform priorityBoosting
      if(mlfq._globalTicks == TIMEQUANTUM_MLFQ)
       priorityBoosting();
      
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
/*
  Add Comment : 
  The CPU is taken away by a timer interrupt.
  If the process has not used up its time quantum in the queue to which it belongs, it returns to its original queue.
  If the process has used up its time quantum, it moves to the next level queue.
  If a process has used up its time quantum in the L2 queue, its prioirty is decreased.
*/
void
yield(void)
  {
  acquire(&ptable.lock);  //DOC: yieldlock
  struct proc* p = myproc();
  myproc()->state = RUNNABLE;
 
  p->_localTicks++;
  mlfq._globalTicks++;

  /* Add Comment :
     If the process that called schedulerLock or the process that most recently called schedulerUnlock 
     is at the front of the L0 queue, insert it at the front of the L0 queue.
  */
  if((mlfq._isLocked != NULL && p->_putLock) || p->_putUnlock)
  {
    if(p->_putUnlock) 
      p->_putUnlock = 0;
    p->_level = 0;
    enL0Queue(&mlfq._L0, p);
  }
  else if(p->_level == 0 && p->_localTicks != TIMEQUANTUM_0)
    enQueue(&mlfq._L0, p);
  else if(p->_level == 0 && p->_localTicks == TIMEQUANTUM_0)
  {
    p->_localTicks = 0;
    p->_level = 1;
    enQueue(&mlfq._L1, p);
  }
  else if(p->_level == 1 && p->_localTicks != TIMEQUANTUM_1)
    enQueue(&mlfq._L1, p);
  else if(p->_level == 1 && p->_localTicks == TIMEQUANTUM_1)
  {
    p->_localTicks = 0;
    p->_level = 2;
    enL2Queue(&mlfq._L2, p);
  }
  else 
  {
    if(p->_localTicks == TIMEQUANTUM_2)
    {
      p->_localTicks = 0;
      if(--(p->_priority) < 0)
        p->_priority = 0;
    }
    enL2Queue(&mlfq._L2, p);
  }

  sched();
  release(&ptable.lock);
}

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
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  mlfq._globalTicks++;
  p->_localTicks++; 

  if(p->_localTicks == TIMEQUANTUM_0) {
     p->_level = 1;
     p->_localTicks = 0;
  }
  else if(p->_localTicks == TIMEQUANTUM_1) {
     p->_level = 2;
     p->_localTicks = 0;
  }
  else if(p->_localTicks == TIMEQUANTUM_2) {
     if(--p->_priority < 0) 
        p->_priority = 0;
     p->_localTicks = 0;
  }
  // Add Comment : A process that enters the SLEEPING state due to I/O or an event is placed in the SleepQueue queue.
  enQueue(&mlfq._SleepQueue, p);
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  /* Add Comment :
     Previously, in xv6, the system would iterate through the entire process table to wake up the process
     that matched the channel, which was not very efficient
     The modified code efficiently wake up processes in the SLEEPING state by iterating 
     through the SleepQueue queue, which contains only sleeping processes.
  */ 
  for(p = mlfq._SleepQueue._head; p != NULL; p = p->_next)
  {
      if(p->state == SLEEPING && p->chan == chan)
      {  
        struct proc* temp = p->_prev;
        deQueueAnywhere(&mlfq._SleepQueue, p);
          
        p->state = RUNNABLE;
	
        /* Add Comment : 
           If the process that called schedulerLock is in SLEEPING state, 
           or if the process that called schedulerUnlcok goes into SLEEPING state before the timer interrupt occurs
        */
	if((mlfq._isLocked != NULL && p->_putLock) || p->_putUnlock)
	  enL0Queue(&mlfq._L0, p);
        else if(p->_level == 0)
          enQueue(&mlfq._L0, p);
        else if(p->_level == 1)
          enQueue(&mlfq._L1, p);
        else if(p->_level == 2)
          enL2Queue(&mlfq._L2, p);
        p = temp;
      }
   }
}

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
      {
        deQueueAnywhere(&mlfq._SleepQueue, p);
        p->state = RUNNABLE;
      	
	if((mlfq._isLocked != NULL && p->_putLock) || p->_putUnlock)
	  enL0Queue(&mlfq._L0, p);
        else if(p->_level == 0)
          enQueue(&mlfq._L0, p);
        else if(p->_level == 1)
          enQueue(&mlfq._L1, p);
        else if(p->_level == 2)
          enL2Queue(&mlfq._L2, p);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// For setPriority System Call
int 
setPriority(int pid, int priority)
{
   int found = 0;
   struct proc* p;
   struct proc* mp = myproc();

   acquire(&ptable.lock);
   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
   {
      if(p->pid == pid)
      {
        found = 1;
        p->_priority = priority;

        /* Add Comment : 
           If a processes in L2 queue, other than itself, has a change in priority,
           it needs to be removed and reinserted to rearrange the order.
           Since L0 and L1 queues are Round-Robin, scheduling order is not changed by priority
           If there is a process in the L2 queue that is currently running on the CPU, 
           when it returns to the L2 queue from yield during the subsequent timer interrupt, 
           its order will be rearranged and it will be inserted.
        */
      	if(p->_level == 2 && p != mp)
        {
          p = deQueueAnywhere(&mlfq._L2, p);
          enL2Queue(&mlfq._L2, p);
        }
      }
   }
   release(&ptable.lock);
   return found;    
}


void
schedulerLock(int password)
{
   struct proc* p = myproc();

   /* Add Comment : 
      If no password was entered or an incorrect password was entered,
      and schedulerUnlock was not called, but schedulerLock was called.
   */
   if(password == -1 || password != 2017029343 || mlfq._isLocked != NULL)
   {
     cprintf("pid : %d, time quantum : %d, level : %d\n", p->pid, p->_localTicks, p->_level);
     exit();
   }
   mlfq._isLocked = p;          // Add Comment : Upon successful invocation of schedulerLock, the address of the process is stored in mlfq._isLocked variable.
   mlfq._globalTicks = 0;       
   p->_putLock = 1;             // Add Comment : The variable is used to indicate that the process calling the scheduler lock is itself.
}

void
schedulerUnlock(int password)
{
   struct proc* p = myproc();
   
   /* Add Comment : 
      If no password was entered or an incorrect password was entered,
      and schedulerLock was not called, but schedulerUnlock was called.
   */
   if(password == -1 || password != 2017029343 || mlfq._isLocked == NULL)
   {
     cprintf("pid : %d, time quantum : %d, level : %d\n", p->pid, p->_localTicks, p->_level);
     exit();
   }

   mlfq._isLocked = NULL;
   p->_localTicks = 0;
   p->_priority = 3;

   /* Add Comment : 
      When the process returns to RUNNABLE state due to a timer interrupt, 
      it should be inserted at the front of the L0 queue, as it is the process that has most recently called schedulerUnlock.
      The variable that indicates whether the process is the one that called the most recent schedulerUnlock. 
   */
   p->_putUnlock = 1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  cprintf("GlobalTicks : %d\n", mlfq._globalTicks);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s %d %d %d || %d %d %d %d %d", p->pid, p->name, state, p->_level, p->_priority, p->_localTicks, mlfq._L0._pCount, mlfq._L1._pCount, mlfq._L2._pCount, mlfq._SleepQueue._pCount, mlfq._isLocked);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }

  // Add Comment : Add code to display the current state of queues for debugging purposes.
  for(p = mlfq._L0._head; p != NULL; p = p->_next)
     cprintf("L0 Queue -> pid : %d, name : %s, level : %d, localticks : %d, L0 Queue pCount : %d\n", p->pid, p->name, p->_level, p->_localTicks, mlfq._L0._pCount);

  for(p = mlfq._L1._head; p != NULL; p = p->_next)
     cprintf("L1 Queue -> pid : %d, name : %s, level : %d, localticks : %d, L1 Queue pCount : %d\n", p->pid, p->name, p->_level, p->_localTicks, mlfq._L1._pCount);

  for(p = mlfq._L2._head; p != NULL; p = p->_next)
     cprintf("L2 Queue -> pid : %d, name : %s, level : %d, localticks : %d, L2 Queue pCount : %d\n", p->pid, p->name, p->_level, p->_localTicks, mlfq._L2._pCount);

  for(p = mlfq._SleepQueue._head; p != NULL; p = p->_next)
     cprintf("Sleep Queue -> pid : %d, name : %s, level : %d, localticks : %d, Sleep Queue pCount : %d\n", p->pid, p->name, p->_level, p->_localTicks, mlfq._SleepQueue._pCount);
}
