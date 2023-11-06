#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "thread_Queue.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
struct threadQueue allThreadQueue; // 모든 프로세스(쓰레드)를 담고있는 Queue - Singly Linked List로 구현
                                   // 해당 구현에서 프로세스 또한 쓰레드(메인쓰레드)로 생각하여 구현
                                   // 따라서, 모든 프로세스(메인쓰레드), 메인쓰레드로부터 만들어진 서브 쓰레드 또한 해당 큐에 저장된다.

int isCallThread = 0;

int nextpid = 1;
int nexttid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

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
  struct proc *curproc = myproc();
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;
  
found:
  // 모든 프로세스(쓰레드)는 allocproc 함수를 거쳐 proc 구조체를 할당 받는다.
  // 따라서, 해당 함수 내부에서 proc 구조체의 멤버 변수 초기화를 진행하여 준다.
  // isCallThread 전역변수를 통해서, allocproc을 호출한 것이 메인쓰레드(프로세스)인지 서브쓰레드인지 구분하여
  // 만약 메인쓰레드에서의 호출(즉, 새로운 프로세스 생성)이라면 메인쓰레드가 되도록 isMainThread를 1로 세팅하여 준다.
  p->threadQueue.head = NULL;
  p->threadQueue.tail = NULL;
  p->threadQueue.subThreadNum = 0;

  p->state = EMBRYO;
  p->pid = nextpid++;
  p->limitSize = 0;

  p->threadInfo.subThreadNext = NULL;
  p->threadInfo.allThreadNext = NULL;

  if(isCallThread == 0)
  {
    p->isMainThread = 1;
    p->threadInfo.main = p;  // 메인쓰레드는 threadInfo.main에 자신을 가리키도록 구현.
  }                          // 서브쓰레드는 해당 메인쓰레드를 가리킬 수 있도록 thread_create에서 값을 변경하여 준다.
  

  isCallThread = 0;
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

  enAllThreadQueue(&allThreadQueue, p); // 모든 쓰레드(메인쓰레드 혹은 서브쓰레드)는 allThreadQueue에 저장될 수 있도록 넣어준다.
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
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

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();
  
  cli();
  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;

  // 특정 쓰레드(메인쓰레드 혹은 서브쓰레드)에서 할당받은 메모리의 크기를 변경하고자 하는 경우,
  // 연관되어 있는 모든 쓰레드가 공유 될 수 있도록 sz 값을 변경하여 준다.
  for(struct proc *p = allThreadQueue.head; p != NULL; p = p->threadInfo.allThreadNext){
    if(p->threadInfo.main == curproc->threadInfo.main){
      p->sz = sz;
    }
  }

  switchuvm(curproc);

  sti();
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
  
  // 특정 쓰레드에서 exit을 호출한 경우, 연관되어 있는 모든 쓰레드가 종료될 수 있도록 구현
  // 연관되어 있는 모든 쓰레드의 부모 쓰레드를 main 쓰레드의 부모 쓰레드(프로세스)로 변경.
  // 이후, state를 ZOMBIE로 만들고 부모 쓰레드(프로세스)를 깨워 자원을 회수 하도록 구현
  for(p = allThreadQueue.head; p != NULL; p = p->threadInfo.allThreadNext){
    if(p->threadInfo.main == curproc->threadInfo.main){
      deSubThreadQueue(&(p->parent->threadQueue), p);
      enSubThreadQueue(&(p->threadInfo.main->parent->threadQueue), p);
      p->parent = curproc->threadInfo.main->parent;
      p->state = ZOMBIE;
      wakeup1(p->parent);
    }
  }

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
  sched();
  panic("zombie exit");
}


// exit이 호출되면 exit을 호출한 쓰레드와 연관되어 있는 모든 쓰레드는 
// 부모가 메인쓰레드(프로세스)의 부모쓰레드(프로세스)로 변경되고,
// 부모쓰레드는 ZOMBIE 상태에 있는 모든 쓰레드의 자원을 회수하여 주도록 구현
// ptable을 돌면서 자원을 회수하는데, 연관되어 있는 쓰레드들은 pgdir을 공유하므로
// 본래의 메인쓰레드가 가장 마지막으로 pgdir을 해제하도록 구현
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  int isCompleteKill = 0;
  
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
        deSubThreadQueue(&(curproc->threadQueue), p);
        deAllThreadQueue(&allThreadQueue, p);
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        if(curproc->isMainThread && curproc->threadQueue.subThreadNum == 0)
          freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->sz = 0;
        p->isMainThread = 0;
        p->tf->eip = 0;
        p->tf->esp = 0;
        p->state = UNUSED;
        p->threadInfo.tid = 0;
        isCompleteKill = 1;
      }
    }
    
    if(isCompleteKill){
      release(&ptable.lock);
      return pid;
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
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
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
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
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
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;  
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
// Process woFn't exit until it returns
// to user space (see trap in trap.c).

// 특정 프로세스(혹은 쓰레드)에서 pid를 통해서 kill을 호출 시,
// 연관된 모든 쓰레드는 pid를 공유하므로 그 중에서, 메인쓰레드(프로세스)의 killed를 1로 세팅
// 메인쓰레드는 이후 다시 스케줄링되어 인터럽트(타이머)에 의해 exit()이 호출
// exit에서 모든 연관된 쓰레드를 ZOMBIE로 만들고 부모를 바꾸면서 부모쓰레드(프로세스)가 연관된 모든 쓰레드의 자원을 회수하도록 구현
int
kill(int pid)
{
  struct proc *p;
  int completeKill = 0;
  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->isMainThread == 1){
      completeKill = 1;
      p->killed = 1;
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
    }  
  }
  
  if(completeKill){
    release(&ptable.lock);
    return 0;
  }
  release(&ptable.lock);
  return -1;
}

// pmanager를 위한 list 함수 - System Call
// 메인쓰레드(프로세스)의 proc 구조체에 저장되어 있는 각종 정보를 출력하도록 구현
void
list(void)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if((p->isMainThread == 1) && (p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING))
      cprintf("Name : %s, Pid : %d, sz : %d, stackpageNum : %d, memlimit : %d, eip : %d, esp : %d\n", p->name, p->pid, p->sz,p->stackPageNum, p->limitSize, p->tf->eip, p->tf->esp);
  }
}

// 프로세스의 메모리 크기의 한도를 지정하는 setmemorylimit 함수 - System Call
// proc 구조체에 limitSize 멤버 변수를 추가하여,
// sbrk - growproc 으로 프로세스의 메모리 크기를 변경하려고 할 때, 참조하여 가능 여부 결정
int
setmemorylimit(int pid, int limit)
{
  struct proc *p;
  int found = 0;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid == pid)
      {
        found = 1;
        break;
      }   
  }

  if(!found)
  {
    release(&ptable.lock);
    return -1;
  }

  if(p->sz > limit)
  {
    release(&ptable.lock);
    return -1;
  }

  p->limitSize = PGROUNDUP(limit);
  release(&ptable.lock);
  return 0;
}

// thread를 만드는 함수 
// xv6에서는 쓰레드가 구현되어 있지 않다. 명세에 따라 쓰레드는 프로세스와 동일하게 취급될 수 있도록 allocproc을 통해서 proc 구조체를 할당받는다.
// allocproc에 들어가기 전 isCallThread를 1로 세팅하여 thread를 위한 proc 구조체 할당임을 알려, isMainThread의 값을 0으로 세팅되도록 구현
// 대부분의 과정은 fork 함수와 유사. threadInfo.main을 메인쓰레드(프로세스)로 설정하여 다른 쓰레드와 연관성을 구분지음 - 부모는 상위 쓰레드가 되도록 구현하였음.
// 이후, 함수를 수행하는 과정은 exec 함수의 수행과정과 유사 eip 값을 함수의 주소로 설정하여 해당 함수의 첫번째 구문(명령어)부터 수행하도록 설정.
int
thread_create(thread_t* thread, void *(*start_routine)(void *), void *arg)
{
  uint sp, sz, ustack[3+MAXARG+1];
  pte_t *pgdir = 0;
  struct proc *th;
  struct proc *curproc = myproc();
  struct proc *p;

  // Allocate process.
  isCallThread = 1;
  if((th = allocproc()) == 0)
      goto bad;

  pgdir = curproc->pgdir;
  *th->tf = *curproc->tf;
  
  // limitSize가 제한되어 있는 경우, 쓰레드가 생성되어도 limitSize를 넘기지 않는지 확인
  // 넘는 경우 쓰레드 생성에 실패하도록 구현
  sz = curproc->sz;
  sz = PGROUNDUP(sz);
  if(sz + 2*PGSIZE > curproc->limitSize && curproc->limitSize != 0){
    th->state = UNUSED;
    return -1;
  }

  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;

  acquire(&ptable.lock);
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));

  sp = sz;
  ustack[0] = 0xffffffff;
  ustack[1] = (uint)arg;
  sp -=  2 * 4;

  if(copyout(pgdir, sp, ustack, 2*4) < 0)
    goto bad;
  
  for(int i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      th->ofile[i] = filedup(curproc->ofile[i]);
  th->cwd = idup(curproc->cwd);
  safestrcpy(th->name, curproc->name, sizeof(curproc->name));
  
  
  th->pgdir = pgdir;
  th->sz = sz;
  th->state = RUNNABLE;
  th->pid = curproc->pid;
  th->parent = curproc;
  th->tf->eip = (uint)start_routine;
  th->tf->esp = sp;

  th->isMainThread = 0;
  th->isExec = 0;
 
  th->threadInfo.tid = nexttid++;
  th->threadInfo.subThreadNext = NULL;
  th->threadInfo.allThreadNext = NULL;
  th->threadInfo.main = curproc->threadInfo.main;

  curproc->sz = sz;
  curproc->pgdir = pgdir;
  
  *thread = th->threadInfo.tid;

  // 연관된 쓰레드가 모두 같은 pgdir을 공유하고, 같은 메모리를 공유하도록 sz의 값을 동일하게 변경하여 준다.
  enSubThreadQueue(&(curproc->threadQueue), th);
  for(p = allThreadQueue.head; p != NULL; p = p->threadInfo.allThreadNext){
    if(p->threadInfo.main == curproc->threadInfo.main){
      p->sz = sz;
      p->limitSize = curproc->limitSize;
    }
  }
  release(&ptable.lock);
  return 0;

  bad:
    if(pgdir)
      freevm(pgdir);
    if(th)
      th->state = UNUSED;
    return -1;
}


// 쓰레드를 종료시키는 함수
// 모든 과정이 exit 함수와 동일하며, 상위 쓰레드가 exit을 호출한 쓰레드의 자원을 정리하도록 구현
void
thread_exit(void* retval)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  // 만약 메인쓰레드에서 thread_exit을 호출하게 되면 아무런 일도 일어나지 않고 종료
  if(curproc->isMainThread)
    return;

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
  curproc->threadInfo.retVal = retval;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // 하위 쓰레드가 모두 종료되지 않은 상태에서 쓰레드가 종료되는 경우
  // 자신의 하위 쓰레드를 ZOMBIE로 만들어 종료시킨 뒤, 부모를 자신의 부모와 같도록 만들고 부모가 종료시킬 수 있도록 구현.
  if(curproc->isMainThread != 1 && curproc->threadQueue.subThreadNum != 0){
    for(p = curproc->threadQueue.head; p != NULL; p = p->threadInfo.subThreadNext){
    if(p->parent == curproc){
      p->parent = curproc->parent;
      p->state = ZOMBIE;
      deSubThreadQueue(&(curproc->threadQueue), p);
      enSubThreadQueue(&(curproc->parent->threadQueue), p);
      }
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// 하위 쓰레드의 종료를 기다리면서 인자로 받은 쓰레드가 종료될때, 자원을 회수하는 함수
// 대부분의 과정이 wait 함수와 유사하지만, 연관된 모든 쓰레드가 pgdir을 공유하는 점이 기존의 wait와 다름
// 따라서, freevm(pgdir)을 진행하지 않도록 구현
int 
thread_join(thread_t thread, void** retval)
{
  struct proc* p;
  struct proc* curproc = myproc();
  int haveSubThreads = 0;
  int found = 0;

  for(p = allThreadQueue.head; p != NULL; p = p->threadInfo.allThreadNext){
    if(p->threadInfo.tid == thread){
      found = 1;
      break;
    }
  }

  if(!found)
    return -1;
  
  if(curproc->threadQueue.subThreadNum != 0)
    haveSubThreads = 1;
  
  acquire(&ptable.lock);
  for(; ;){
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == ZOMBIE && p->threadInfo.tid == thread){
          deSubThreadQueue(&(curproc->threadQueue), p);
          deAllThreadQueue(&allThreadQueue, p);
          *retval = p->threadInfo.retVal;
          kfree(p->kstack);
          p->kstack = 0;
          p->pid = 0;
          p->parent = 0;
          p->name[0] = 0;
          p->killed = 0;
          p->threadQueue.head = NULL;
          p->threadQueue.tail = NULL;
          p->threadQueue.subThreadNum = 0;
          p->tf->eip = 0;
          p->tf->esp = 0;
          p->threadInfo.tid = 0;
          p->state = UNUSED;
          release(&ptable.lock);
          return 0;
      }
    }
    
    if(!haveSubThreads || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock);
  }
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);    
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);

    }
    cprintf("\n");
  }
}
