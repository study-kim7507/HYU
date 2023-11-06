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

// Add Comment : Add yield System Call
void
sys_yield(void)
{
   yield();
}


// Add Comment : Add getLevel System Call
int
sys_getLevel(void)
{
   return myproc()->_level;
}


// Add Comment : Add setPriority System Call
void
sys_setPriority(void)
{
   int pid;
   int priority;
   if (argint(0, &pid) < 0 || argint(1, &priority) < 0)
      return;
   if (priority > 3)
      return;
   setPriority(pid, priority);
}


// Add Comment : Add schedulerLock System Call
void 
sys_schedulerLock(void)
{
   int password;
   if (argint(0, &password) < 0)
      schedulerLock(-1);
   schedulerLock(password);
}


// Add Comment : Add schedulerUnlock System Call
void
sys_schedulerUnlock(void)
{
    int password;
    if (argint(0, &password) < 0)
       schedulerUnlock(-1);
    schedulerUnlock(password);
}

