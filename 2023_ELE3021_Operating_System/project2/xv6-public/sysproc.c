#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;

  // 만약 프로세스가 할당 받을 수 있는 메모리의 최대치를 넘어서 할당을 받고자 한다면 실패하도록 설정.
  if((myproc()->limitSize != 0) && (myproc()->limitSize < addr + n))
    return -1;

  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// list 시스템 콜의 wrapper function
void
sys_list(void)
{
  list();
}

// setmemorylimit 시스템 콜의 wrapper function
int
sys_setmemorylimit(void)
{
  int pid;
  int limit;

  if(argint(0, &pid) < 0 || argint(1, &limit) < 0)
    return -1;

  if(pid < 0 || limit < 0)
    return -1;
    
  return setmemorylimit(pid, limit);
}

// thread_create 시스템 콜의 wrapper function
int
sys_thread_create(void)
{
  thread_t *thread;
  void* (*start_routine)(void *) = 0;
  void* arg = 0;

  if(argptr(0, (char**)&thread, sizeof(thread)) < 0 || argint(1, (int*)&start_routine) < 0 || argint(2, (int*)&arg) < 0)
    return -1;

  return thread_create(thread, start_routine, arg); 
}

// thread_exit을 위한 wrapper function
int 
sys_thread_exit(void)
{
  void* retVal;
  if(argint(0, (int*)&retVal) < 0)
    return -1;

  thread_exit(retVal);
  return 0;
}

// thread_join을 위한 wrapper function
int
sys_thread_join(void)
{
  thread_t thread;
  void** retVal;
  if(argthread(0, &thread) < 0 || argint(1, (int*)&retVal) < 0)
    return -1;

  return thread_join(thread, retVal);
}
