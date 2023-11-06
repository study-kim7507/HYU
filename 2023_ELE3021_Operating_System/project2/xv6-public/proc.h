// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct threadInfo {
  thread_t tid; 
  struct proc* main;    // 메인쓰레드를 가리키는 변수. 만약 자신이 메인쓰레드라면 본인을 가리키도록 설정.
                        // 메인쓰레드에서 파생되는 모든 하위쓰레드는 메인쓰레드를 가리키도록 thread_create에서 설정.
  struct proc* subThreadNext; // 하위 쓰레드를 관리하는 큐에서 다음 쓰레드를 가리키도록하는 변수
  struct proc* allThreadNext; // 모든 쓰레드를 관리하는 큐에서 다음 쓰레드를 가리키도록하는 변수.
  void* retVal;
};

struct threadQueue{
  struct proc* head;  
  struct proc* tail;
  int subThreadNum;   // 하위 쓰레드의 개수를 저장하기 위한 멤버 변수.
};

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  uint limitSize;    // setmemorylimit에서 프로세스의 최대 크기를 설정하는 변수.
  uint stackPageNum; // exec2에서 인자로 전달받은 스택의 개수를 설정하기 위한 변수. 

  int isMainThread;
  int isExec;
  struct threadInfo threadInfo;   // 메인쓰레드(프로세스)가 아닌 서브쓰레드라면 threadInfo 구조체를 통해 관리.
  struct threadQueue threadQueue; // 모든 쓰레드는 자신의 하위 쓰레드를 가질 수 있도록 구현.
                                  // threadQueue라는 구조체를 이용하여 자신의 하위 쓰레드를 관리할 수 있도록 추가.
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
