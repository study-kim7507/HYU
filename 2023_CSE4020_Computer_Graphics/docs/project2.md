# Project #2 - LWP

> Operating System
> 

> 2017029343 김기환
> 

## Requirements

---

1. Process with various stack size
    - int exec2(char *path, char **argv, int stacksize);
2. Process memory limitation
    - int setmemorylimit(int pid, int limit);
3. Process manager
    - List, Kill, Execute, Memlim, Exit
4. Pthread in xv6
    - int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg);
    - int thread_join(thread_t thread, void **retval);
    - void thread_exit(void *retval);
    - Other system calls(fork, exec, wait ..)

## Background Knowledge

---

1. **Process Address Space (in xv6)**
    - xv6의 모든 프로세스는 fork와 exec을 통해서 생성이 된다. 해당 과정에서 커널 영역, Code(Text) 영역과 data 영역을 제외하고, 2개의 page를 할당받으며 생성이 된다. 하나는 가드용 페이지. 하나는 프로세스가 사용할 스택용 페이지이다. 이러한 페이지는 하나에 **4096 Bytes** 의 크기를 가지게 된다. 따라서 스택의 크기는 동적으로 늘어나거나 줄어들도록 할 수 없게 구현되어 있으며, 힙 영역을 sbrk 시스템 콜을 이용하여 늘리거나 줄이도록 구현되어 있다. 아래는 xv6에서 프로세스의 Address Space를 그림으로 나타낸 것이다.

![F2-3](https://github.com/study-kim7507/HYU/assets/63442832/b400d422-8480-478c-a76e-05ac4157fca2)

![F1-1](https://github.com/study-kim7507/HYU/assets/63442832/025e2b4a-160d-4e6b-bc30-9b57d9254c3e)

1. **Stack Page and Guard Page (in xv6)**
    - 기존의 xv6는 fork 함수를 통해 자신과 동일한 Address Space를 가지는 자식 프로세스를 생성한다. 이후, exec 함수를 통해서 Address Space를 exec 함수의 인자로 전달받은 프로그램으로 Address Space를 덮어씌운다. 해당 과정에서 다음과 같은 과정을 거친다.
    
    ```c
    sz = PGROUNDUP(sz);
      if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
        goto bad;
      clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
      sp = sz;
    
      // Push argument strings, prepare rest of stack in ustack.
      for(argc = 0; argv[argc]; argc++) {
        if(argc >= MAXARG)
          goto bad;
        sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
        if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
          goto bad;
        ustack[3+argc] = sp;
      }
      ustack[3+argc] = 0;
    
      ustack[0] = 0xffffffff;  // fake return PC
      ustack[1] = argc;
      ustack[2] = sp - (argc+1)*4;  // argv pointer
    
      sp -= (3+argc+1) * 4;
      if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    ```
    
    1. allocuvm(pgdir, sz, sz + 2 * PGSIZE)를 통해서 2개의 page를 할당받는데, 하나는 가드용 페이지, 하나는 스택용 페이지이다. **스택용 페이지는 프로세스가 지역변수나, Function Call을 위해 사용하는 메모리의 영역이고, 가드용 페이지는 메모리를 보호하기 위한 것이다.** 프로세스 메모리 공간 끝에 특별한 페이지를 할당하여 해당 페이지에 접근하게 되는 순간 Exception을 발생시킨다. 가드용 페이지를 사용하여 자신이 할당된 메모리 공간을 벗어나는 경우를 방지하도록 구현되어 있다. 
    2. 이후, exec 함수의 인자로 전달받아 새로운 프로그램에게 전달하도록 스택에 전달받은 인자를 저장하는 과정이 포함되어 있다.

1. **Thread**
    - 프로세스의 스케줄링 과정은 Context Switching에서 많은 오버헤드가 발생하게 된다. 이는 프로세스가 각각의 독립된 메모리 영역을 할당받음으로 프로세스가 공유하는 메모리가 존재하지 않아 Cache Flush와 같은 무거운 작업이 진행되면서 발생하는 오버헤드이다. 하지만 쓰레드를 이용하면 이러한 오버헤드를 줄일 수 있게되어 효율성이 증대된다.
    - **쓰레드(Thread)란 프로세스 내에서 실행되는 흐름의 단위 혹은 CPU 스케줄링의 기본 단위이다.**
        - 쓰레드는 각자 자신의 **Stack** 영역을 보유한다. (자신의 레지스터 상태를 보유)
        - 쓰레드는 프로세스 내에서 **Code, Data, Heap 영역을 공유한다.**
    - 쓰레드를 활용하면 Stack 영역을 제외한 Code, Data, Heap 영역을 공유하기에 독립적인 프로세스와 달리, 쓰레드 간 데이터를 주고 받는 것이 간단해지고 시스템 자원의 소모가 줄어들게 된다.
    - 하지만, 쓰레드는 Stack 영역을 제외한 영역을 공유하기에 Data 영역에 저장되어 있는 전역 변수를 공유하게 된다. 공유하는 전역 변수를 여러 쓰레드가 동시에 접근하여 사용하게 되면서 충돌하는 문제가 발생하는데, 이러한 문제가 발생하는 코드 영역을 ********임계 구역(Critical Section)******** 이라고 한다. 따라서 쓰레드를 사용한 프로그램은 동기화를 통하여 이러한 문제를 해결해주어야 한다.
    - 쓰레드는 다음과 같이 하나의 프로세스 내부에서 stack 영역의 한 자리를 차지하여 자신만의 stack 영역 이외의 부분을 프로세스로부터 공유받는다.
    - 즉, **쓰레드를 이용하면 하나의 프로세스안에서 여러 함수가 스케줄링되면서 동시에 수행되도록 할 수 있게 된다.**

![%EC%BA%A1%EC%B2%98](https://github.com/study-kim7507/HYU/assets/63442832/ee04d91f-66bc-44e2-a3fa-4476278d1d89)

![1](https://github.com/study-kim7507/HYU/assets/63442832/f64bbecc-4940-4ae0-9352-a4ace80c7610)

## Implementation of **proc struct** (proc.h)

---

- Requirements를 만족시키기 위해 proc 구조체를 수정하였으며, 이를 바탕으로 구현을 진행함.

```c
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
  
  int isThread;      
  int isMainThread;
  int isExec;
  struct threadInfo threadInfo;   // 메인쓰레드(프로세스)가 아닌 서브쓰레드라면 threadInfo 구조체를 통해 관리.
  struct threadQueue threadQueue; // 모든 쓰레드는 자신의 하위 쓰레드를 가질 수 있도록 구현.
                                  // threadQueue라는 구조체를 이용하여 자신의 하위 쓰레드를 관리할 수 있도록 추가.
};

```

## Implementation of Process Manager → [exec2, setmemorylimit, pmanager]

---

1. **exec2(char *path, char **argv, int stacksize);  → exec2.c**
    - 기존의 exec과 수행은 동일하지만 가드용 페이지, 스택용 페이지 하나씩 할당받는 기존의 exec과 달리, 스택용 페이지를 원하는 개수만큼 할당받을 수 있는 있도록 하는 시스템 콜.
    - 기존의 exec과 대부분 동일하며, **인자로 받아온 stacksize(스택용 페이지의 개수) 만큼 프로세스의 크기(스택용 페이지의 개수)를 조절하여 할당하도록 구현하였다.**
    - sysfile.c에 sys_exec2 Wrapper Function을 만들었으며, System Call Number를 8번으로 설정함.
    
    ```c
    // 범위 내의 개수가 아니면 -1을 리턴하면서 종료.
      if(!(stacksize >= 1 && stacksize <= 100))
        return -1;
    
    // Allocate two pages at the next page boundary.
    // Make the first inaccessible.  Use the second as the user stack.
      sz = PGROUNDUP(sz);                                             // sz가 4096(페이지크기)의 배수이면 그대로 반환, 아니라면 4096의 배수가 되도록 올림.
      if((sz = allocuvm(pgdir, sz, sz + (stacksize+1)*PGSIZE)) == 0)  // 가드용 페이지를 위해 인자로 전달받은 statcsize보다 하나 더 많은 페이지를 할당받을 수 있도록
        goto bad;
      clearpteu(pgdir, (char*)(sz - (stacksize+1)*PGSIZE));
      sp = sz;
    
    // proc 구조체에 저장되는 프로세스의 정보를 변경하여 준다.
    oldpgdir = curproc->pgdir;                                         // 기존의 pgdir를 저장
    curproc->pgdir = pgdir;                                            // 새로 수행하려는 프로그램의 pgdir로 변경
    curproc->sz = sz;                                                  // 변경된 메모리 사이즈를 저장
    curproc->tf->eip = elf.entry;  // main
    curproc->tf->esp = sp;
    curproc->stackPageNum = stacksize;                                 // pmanager의 list를 위한 변수
    switchuvm(curproc);
    freevm(oldpgdir);                                                  // 기존의 pgdir을 할당해제
    return 0;
    ```
    

1. **setmemorylimit(int pid, int limit);  → proc.c**
    - 특정 프로세스가 **할당 받을 수 있는 메모리의 최대값을 설정하는 시스템 콜**
    - 비교적 간단. 인자로 전달 받은 pid에 해당하는 프로세스가 있는지 확인 후 있다면, proc 구조체에 선언한 **limitSize** 변수의 값을 설정하여 준다.
        - 후에, limitSize를 기준으로 sbrk→growproc에서 메모리를 더욱 늘릴 수 있는지 없는지 판단하도록 구현.
    - sysproc.c에 sys_setmemorylimit Wrapper Function을 만들었으며, System Call Number를 24번으로 설정함.

```c
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
```

1. **sys_sbrk  → sysproc.c**
    - sbrk 시스템 콜은 메모리의 크기를 조절하는 함수이다. (동적 메모리 할당)
    - setmemorylimit에서 설정한 프로세스가 할당받을 수 있는 **메모리의 최대값을 기준으로 메모리 할당 가능 여부를 결정하고, 할당이 가능한 상황이라면 메모리의 크기를 인sbrk의 인자로 받은 만큼 할당시켜 준다.**
    - **여러 쓰레드에서 동시에 메모리 크기를 변경하려는 경우, sz라는 공유 변수의 값을 바꾸는 상황이 되므로 동기화 문제가 발생할 수 있다. 따라서, 인터럽트 제어 명령어인 cli와 sti를 사용한다. growproc 시작과 끝에 cli, sti를 호출하여 sz의 값을 바꾸는 과정에서 다른 쓰레드가 수행되지 않도록 구현하였다.**
    
    ```c
    int
    sys_sbrk(void)
    {
      int addr;
      int n;
    
      cli();
    
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
    ```
    

1. **pmanager(user program) → pmanager.c**
    - xv6의 내장 되어있는 sh.c 프로그램을 기반으로 구현하였으며 입력을 받고 문자열 파싱 이후, 각각의 커맨드에 따라 조건문으로 분기를 나누어 수행하도록 구현하였다.
    - 특히, **execute 커맨드 구현**에서 “실행한 프로그램의 종료를 기다리지 않고 pmanager는 이어서 실행되어야 합니다.”라는 조건을 위해 sh.c에서 **백그라운드에서 프로세스를 동작시키는 방법**을 참조하여 구현하였다. → sh.c를 참조하여 구현하였기에 백그라운드에서 프로세스가 종료되면 initprocess가 자신의 자식프로세스를 종료하면서 출력하는 “zombie!”라는 문장도 출력되는 모습을 볼 수 있다.
    1. list → list 시스템 콜을 구현하여 해당 시스템 콜을 호출하도록 하여 커맨드를 수행하도록 하였다.
        - int list(void) → System Call
            - pmanager의 list 커맨드를 위해 구현한 시스템 콜. ptable을 순회하면서 **현재 메모리에 로드되어 있는 쓰레드를 제외한 프로세스(메인쓰레드). 즉, state가 RUNNING, RUNNABLE, SLEEPING인 프로세스**의 이름, pid, 스택용 페이지의 개수, 할당받은 메모리의 크기, 메모리의 최대 제한을 출력하도록 구현하였다.
            - sysproc.c에 sys_list Wrapper Function을 만들었으며, System Call Number를 23번으로 설정함.
            
            ```c
            int
            list(void)
            {
              struct proc *p;
              for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
                if((p->isMainThread == 1) && (p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING))
                  cprintf("Name : %s, Pid : %d, sz : %d, stackpageNum : %d, memlimit : %d, eip : %d, esp : %d\n", p->name, p->pid, p->sz,p->stackPageNum, p->limitSize, p->tf->eip, p->tf->esp);
              }
              return 0;
            }
            ```
            
    2. kill → 기존에 구현되어 있는 xv6 내장 시스템 콜인 kill 시스템 콜을 이용하여 커맨드를 수행하도록 하였다.
    3. execute → 위에서 구현한 exec2 시스템 콜을 이용하여 인자로 받은 stacksize만큼 메모리를 할당 받고 프로세스를 동작시키도록 수행하였다. → sh.c를 참고하여 백그라운드로 돌도록 구현
    4. memlim → 위에서 구현한 setmemorylimit 시스템 콜을 이용하여 인자로 받은 메모리의 크기를 해당 프로세스가 할당 받을 수 있는 최대 메모리의 크기로 설정하였다.
        
        → **memlim으로 프로세스가 할당 받을 수 있는 최대 크기 설정 이후, sbrk 시스템 콜을 이용하여 그 보다 큰 메모리 할당을 요청하는 경우 실패함을 확인하였다.**
        
    5. exit → pmanager를 종료하도록, exit 시스템콜을 호출하도록 하여 커맨드를 수행하도록 하였다.

```c
int main(int argc, char* argv[])
{
    char buff[100];
    char cmd[100];
    char arg1[100];
    char arg2[100];

    // 사용자로부터 명령어를 입력받고, 버퍼에 저장
    while(getcmd(buff, sizeof(buff)) >= 0){
      buff[strlen(buff)-1] = 0;  
      memset(cmd, 0, sizeof(cmd));   // 커맨드가 저장될 변수 초기화
      memset(arg1, 0, sizeof(arg1)); // 커맨드 뒤이어 올 인자 첫번째
      memset(arg2, 0, sizeof(arg2)); // 커맨드 뒤이어 올 인자 두번째
      parse(buff, cmd, arg1, arg2);  // 명령어를 파싱하여 cmd, arg1, arg2 변수에 저장
      
      if(!strcmp(cmd, "exit")) // 커맨드가 exit이면 pmanager 종료
        break;

      if(fork() == 0)
        runcmd(cmd, arg1, arg2); // 사용자로부터 입력받은 명령어를 바탕으로 명령을 수행
      wait();
    }
    exit();
}

int
getcmd(char *buf, int nbuf)
{
  printf(2, "-> ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

// 널문자, 공백을 기준으로 문자열 파싱하는 함수
void
parse(char* buff, char* cmd, char* arg1, char* arg2)
{
  int i = 0;
  int j = 0;

  for(i = 0; buff[i] != '\0' && buff[i] != ' '; i++)
    cmd[i] = buff[i];
  
  for(i=i+1, j=0; buff[i] != '\0' && buff[i] != ' '; i++, j++)
    arg1[j] = buff[i];
    
  for(i=i+1, j=0; buff[i] != '\0' && buff[i] != ' '; i++, j++)
    arg2[j] = buff[i];
}

void
runcmd(char* cmd, char* arg1, char* arg2)
{
  if(!strcmp(cmd, "list") && !strcmp(arg1, "") && !strcmp(arg2, ""))
    list();
  else if(!strcmp(cmd, "kill") && atoi(arg1) && !strcmp(arg2, ""))
  {
    if(atoi(arg2) < 0 || atoi(arg2) > 10000000000)
    {
      printf(1, "Failed to execute the 'kill' command.\n");
      exit();
    }
    if(kill(atoi(arg1)) == 0)
      printf(1, "Successfully executed the 'kill' command.\n");
  }
  // execute의 경우, pmanager와 동시에 실행될 수 있도록 백그라운드로..
  // fork()를 한 번 더 진행하여 자식프로세스에서 exec2를 수행함.
  else if(!strcmp(cmd, "execute") && strcmp(arg1, "") && atoi(arg2))
  {
    if(atoi(arg2) < 0 || atoi(arg2) > 10000000000)
    {
      printf(1, "Failed to execute the 'execute' command.\n");
      exit();
    }
    if(fork() == 0)
      runcmd("back", arg1, arg2);
  }
  else if(!strcmp(cmd, "memlim") && atoi(arg1) && atoi(arg2))
  {
    if(atoi(arg2) < 0 || atoi(arg2) > 10000000000)
    {
      printf(1, "Failed to execute the 'memlim' command.\n");
      exit();
    }
    if(setmemorylimit(atoi(arg1), atoi(arg2)) != -1)
      printf(1, "Successfully executed the 'memlim' command.\n");
  }
  else if(!strcmp(cmd, "back") && strcmp(arg1, "") && atoi(arg2))
  {
    char* argv[MAXARGS];
    strcpy(argv[0], arg1);
    if(exec2(arg1, argv, atoi(arg2)) == -1)
      printf(1, "Failed to execute the 'execute' command.\n");
  }
  exit();
}
```

## Design of LWP → [thread_create, thread_exit, thread_join]

---

- **모든 프로세스는 쓰레드로 간주하며**, 모든 쓰레드는 자신의 하위 쓰레드를 가질 수 있도록 구현하였다. 따라서, 모든 쓰레드는 자신의 하위 쓰레드(자식 쓰레드)들을 관리하는 큐(**threadQueue**)를 가지고 있도록 구현하였다.
- 기존의 xv6의 프로세스 관리와 동일하게, 특정 쓰레드가 생성(thread_create)이 되면 생성하려는 쓰레드가 부모 쓰레드가 되도록 설정하였다. 그리고, 특정 쓰레드가 종료(thread_exit)되면 SLEEPING 상태로 대기 중(thread_join)인 자신의 상위 쓰레드(부모 쓰레드)를 깨워(wakeup) 자신의 자원을 회수하도록 구현하였다.
    
    ![test1_(5)](https://github.com/study-kim7507/HYU/assets/63442832/9d87d969-b6c1-4616-b05c-4379f38494bc)
    
    ******************thread_create, thread_exit, thread_join은 위와 같은 계층 구조를 바탕으로 운용되며 모든 쓰레드는 자신의 하위 쓰레드를 관리하는 큐(threadQueue)를 가지고 있다. 추가로 메모리에 존재하는 모든 쓰레드가 참조할 수 있는 allThreadQueue가 존재한다. allThreadQueue에는 현재 메모리에서 로드된 모든 쓰레드가 담겨 있다.******************
    

- **만약**, **특정 쓰레드가 자신의 하위 쓰레드(자식 쓰레드)가 종료되지 않은 상황에서 종료(thread_exit)된다면 자신과 자신의 하위 쓰레드를 ZOMBIE 상태로 만들고, 부모 쓰레드를 자신의 부모와 같도록 만들어 자신의 부모가 자신을 포함한 자신의 하위 쓰레드를 종료하도록 구현.**
    
    ![test1_(6)](https://github.com/study-kim7507/HYU/assets/63442832/7bad3b05-6418-4762-86cc-ca7d8d1584ed)

    **********************************특정 쓰레드(위의 시나리오에서는 Sub Thread3)가 자신의 하위 쓰레드가 종료되지 않았음에도 thread_exit을 호출한 경우, 자신을 포함한 자신의 하위 쓰레드를 모두 ZOMBIE로 만들고, 자신의 부모쓰레드(위의 시나리오에서는 메인쓰레드)에게 자식을 넘겨 부모쓰레드가 자신과 자신의 하위 쓰레드를 모두 종료하도록 구현.**********************************
    
- 더하여, 모든 쓰레드들이 담겨 있으며 모든 쓰레드에서 접근이 가능한 **allThreadQueue**를 구현하여 kill이나 특정 쓰레드에서 exit(≠ thread_exit)이 발생하는 경우 관련된 모든 쓰레드들이 동시에 종료될 수 있도록 하였다. → **즉, 특정 쓰레드가 thread_exit이 아닌 exit을 호출하거나, kill에 의해 모든 쓰레드가 죽게되면 모든 쓰레드가 동시에 죽도록 구현하였다.**
    - allThreadQueue를 순회하며, 관련된 모든 쓰레드의 부모를 메인 쓰레드의 부모로 만들어 주고, 부모 프로세스가 자식인 메인 쓰레드와 그와 관련된 쓰레드를 종료하도록 구현.

    ![test1_(12)](https://github.com/study-kim7507/HYU/assets/63442832/d1162f7c-c450-47d1-9bd6-aa40592ad997)

**관련된 모든 쓰레드는 같은 pid를 갖는데, 다른 쓰레드(프로세스)에서 kill을 특정 pid로 호출하게 되는 경우 해당 pid를 갖는 모든 쓰레드가 죽도록 설계. kill 되는 경우 다음 번 스케줄링되어 인터럽트가 발생하면 그 때 exit 함수에 의해 종료되는데, 이 때, allThreadQueue를 순회하며 관련된 모든 쓰레드를 ZOMBIE로 만드는 동시에 부모를 메인 쓰레드의 부모(위의 시나리오에서는 Parent Process)로 바꾸어주어 해당 부모 쓰레드(프로세스)가 pid에 해당하는 모든 쓰레드의 자원을 회수하도록 구현.**

## Implementation of LWP → [thread_create, thread_exit, thread_join]

---

- 기존의 xv6에는 쓰레드가 구현되어 있지 않음. 이번 구현에서 xv6의 쓰레드는 프로세스와 유사하도록 구현. 따라서 대부분의 코드가 기존에 xv6에 구현되어있는 코드와 유사하다.
    - thread_create : 쓰레드를 만드는 부분(allocproc → proc 구조체를 할당받는 부분)은 기존의 xv6의 fork 함수와 유사하고, 쓰레드의 Address Space를 세팅하는 부분은 exec 함수와 유사하다.
        - 관련된 모든 쓰레드가 thread_create 되면서 변경되는 메모리의 크기를 알 수 있도록 **allThreadQueue를 순회하면서 관련된 모든 쓰레드의 sz값을 변경하는 코드도 추가하였다. → 동기화 문제 발생 가능 ptable.lock 필요.**
        - 만약 setmemorylimit 시스템 콜로 메모리 크기의 최대값이 설정되어 있는 경우에서 쓰레드가 생성되었을 때,  **쓰레드가 생성되면서 변경되는 메모리의 크기가 최대값을 넘게 되는 경우 쓰레드가 생성 되지 않도록하는 코드도 추가하였다.**
    - thread_exit : 대부분의 코드가 xv6 exit 함수와 유사하지만, **쓰레드의 자원은 해당 쓰레드의 부모 쓰레드가 해제하도록 구현**. **만약 하위 쓰레드(자식 쓰레드)가 종료되기 전에 쓰레드가 종료되면, 자식을 init 프로세스에게 넘기는 기존 xv6 exit과 달리 thread_exit은 자신의 부모 쓰레드에게 하위 쓰레드를 넘기도록 구현.**
        - 즉, 자신의 부모 쓰레드가 자신의 자원을 회수하도록 구현. 하지만, **하위 쓰레드가 모두 종료되지 않은 상황에서 종료를 하게되는 경우, 자신의 하위 쓰레드도 종료 시키며 부모 쓰레드가 자신과 자신의 하위 쓰레드도 해제하도록 구현함.**
        - 메인 쓰레드(프로세스)는 thread_exit을 호출하여 종료될 수 없기에 메인 쓰레드에서 thread_exit을 호출하였을 때, 아무런 수행을 하지 않도록 하는 코드를 추가하였다.
    - thread_join : 대부분의 코드가 xv6의 wait 함수와 유사하지만, pgdir을 공유하는 쓰레드의 특성 상, 특정 쓰레드에서 pgdir을 할당 해제하게 되면 다른 쓰레드에서 pgdir을 접근할 수 없어지기 때문에 freevm(p→pgdir) 부분을 삭제하였다.
        - → **즉, 메인 쓰레드가 종료되는 시점에 관련된 모든 쓰레드(하위 쓰레드)가 머물렀던 Address Space를 최종적으로 정리하도록 구현.**
        - **만약 인자로 전달된 thread_t에 해당하는 값이 잘못들어오게 되면, -1을 반환하면서 종료. 하지만, 정상적으로 종료될 수 있도록 메인 쓰레드가 exit을 호출하면 자원을 회수하는 주체를 메인 쓰레드의 부모 쓰레드(프로세스)로 변경하여 메인 쓰레드의 부모 쓰레드(프로세스)가 자원을 할당 해제 하도록 구현함.**
    - 프로세스(쓰레드)를 관리하는 작업이므로 커널 영역에서 수행될 수 있도록 **세 함수 모두 시스템 콜로 정의하였다.**
    - sysproc.c에 sys_thread_create, sys_thread_exit,  sys_thread_join Wrapper Function을 만들었으며, 각각은 25번, 26번, 27번 시스템 콜 번호를 가진다.

1. thread_Queue.h
    - 모든 쓰레드는 자신의 하위 쓰레드들을 보관하는 threadQueue를 가지고 있도록 하였으며 더하여, 메모리에 로드된 모든 쓰레드(프로세스)가 담기고, 모든 쓰레드에서 참조할 수 있는 allThreadQueue가 존재하도록 구현하였다.
    - Singly Linked List Queue 형태로 구현하였으며, 큐 관리가 용이하도록 thread_Queue.h에 다음과 같은 함수를 정의한 뒤, 헤더파일로 만들어 proc.c에 include하여 사용할 수 있도록 하였다.
    
    ```c
    // 프로세스(쓰레드)마다 가지는 자신의 하위 쓰레드를 가지는 큐를 Singly Linked List로 구현
    // 큐에 쓰레드를 넣고 빼는 과정을 함수로 구현
    // 추가로, 모든 쓰레드를 관리하는 allThreadQueue를 위한 함수도 추가로 구현함.
    
    int isSubThreadQueueEmpty(struct threadQueue*);
    int isAllThreadQueueEmpty(struct threadQueue*);
    
    void enSubThreadQueue(struct threadQueue*, struct proc*);
    struct proc* deSubThreadQueue(struct threadQueue*, struct proc*);
    void printSubThreadQueue(struct threadQueue*);
    
    void enAllThreadQueue(struct threadQueue*, struct proc*);
    struct proc* deAllThreadQueue(struct threadQueue*, struct proc*);
    void printAllThreadQueue(struct threadQueue*);
    ```
    

1. thread_create → proc.c

```c
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
```

1. thread_exit → proc.c

```c
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

**  // 하위 쓰레드가 모두 종료되지 않은 상태에서 쓰레드가 종료되는 경우
  // 자신의 하위 쓰레드를 ZOMBIE로 만들어 종료시킨 뒤, 부모를 자신의 부모와 같도록 만들고 부모가 종료시킬 수 있도록 구현.
  // 부모가 종료할 수 있도록 본래의 부모 쓰레드의 threadQueue에서 뺀 뒤, 부모의 threadQueue에 넣는다.**
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
```

1. thread_join → proc.c

```c
// 하위 쓰레드의 종료를 기다리면서 인자로 받은 쓰레드가 종료될때, 자원을 회수하는 함수
// 대부분의 과정이 wait 함수와 유사하지만, 연관된 모든 쓰레드가 pgdir을 공유하는 점이 기존의 wait와 다름
// 따라서, freevm(pgdir)을 진행하지 않도록 구현
int 
thread_join(thread_t thread, void** retval)
{
  struct proc* p;
  struct proc* curproc = myproc();
  int haveSubThreads;
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
          p->threadQueue.allThreadNum = 0;
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
```

## Design of System Call → [fork, exec, sbrk, kill, sleep, pipe]

---

- 쓰레드 생성, 종료, 대기와 같은 경우는 구현에서 프로세스와 동일하게 취급되므로 큰 변경 사항 없이 작동된다.  하지만, fork, exec, sbrk, kill, sleep, pipe 등, 특정 쓰레드에서 해당하는 시스템 콜을 호출하는 경우, 기존과 다르게 수행되어야 하므로 수정이 필요하다.
- xv6에서 LWP는 프로세스와 비슷하게 취급되므로 대부분의 기능이 큰 수정없이 작동된다. 하지만, 메인쓰레드(프로세스)가 아닌 서브쓰레드에서 주어진 시스템 콜을 호출하는 경우에 기존과 다른 동작을 하도록 일부 수정을 진행하였다.
    - fork : 쓰레드의 경우, 메인쓰레드를 포함한 관련된 모든 쓰레드와 Address Space를 공유하는데 특정 서브 쓰레드에서 fork를 호출하게 되면 공유하는 Address Space를 복제한 자식 프로세스(쓰레드)를 생성하는 것이므로 큰 문제 없이 작동. 수정하지 않음.
    - exec : 쓰레드에서 exec 함수를 호출하게 되면 두 가지 경우로 나누어 구현하였는데,  메인 쓰레드에서 exec을 호출하는 경우와 서브 쓰레드에서 exec을 호출하는 경우로 나누어서 구현하였다. 기본적인 동작은 exec한 프로세스가 종료되면서 exit을 만날 때, 기존의 관련된 모든 쓰레드가 종료되도록 구현
        
        → **exec을 호출한 주체에 따라 처리 상황이 달라짐.** 이에 따라, 코드를 분기로 나누어 처리 하였음.
        
        1. **메인 쓰레드에서 exec을 호출하는 경우** : 본인을 제외한 관련된 모든 쓰레드의 상태를 ZOMBIE로 만들고, 관련된 모든 쓰레드를 자신의 threadQueue로 이동시킴. → 후에 종료를 위한 것임
            
            ![test1_(11)](https://github.com/study-kim7507/HYU/assets/63442832/f3335266-d80f-4850-84ef-591cdd59ffd1)
            ******************************************메인 쓰레드에서 exec을 호출하여 모든 쓰레드의 하위 쓰레드가 메인 쓰레드의 하위 쓰레드로 변경 됨과 동시에 하위 쓰레드의 부모도 메인 쓰레드로 변경이 된다. 더하여 메인 쓰레드를 제외한 관련된 모든 쓰레드는 ZOMBIE 상태가 되어 스케줄링되지 않게 된다. 메인 쓰레드가 exec을 호출하게 되면 구조가 위와 같이 변화하게 되고, 후에 exec된 프로그램이 exit 됨과 동시에 할당이 해제된다.******************************************
            
        2. 메인 쓰레드가 아닌 서브 쓰레드에서 exec을 호출하는 경우 : 본인을 메인 쓰레드로 변경. 원래의 메인 쓰레드를 서브 쓰레드로 바꿈.  이후, 본인을 제외한 관련된 모든 쓰레드의 상태를 ZOMBIE로 만들고, 관련된 모든 쓰레드를 자신의 threadQueue로 이동시킴 → 후에 종료를 위한 것임.
            
            ![test1_(10)](https://github.com/study-kim7507/HYU/assets/63442832/bdd5547d-dd4a-4cf0-9548-3cf5d407029e)
            ********************************************************************************************************메인 쓰레드가 아닌 하위의 서브 쓰레드에서 exec을 호출하게 되면 위와 같이 구조가 변경되며, 메인 쓰레드에서 exec을 호출한 경우와 동일하게 동작된다.********************************************************************************************************
            
        - 이러한 구현은 exec에서 기존에 존재하던 pgdir을 해제하는 것과 wait에서 pgdir을 해제하는 것 총 두 번의 pgdir을 해제하는 경우가 생김. proc 구조체에 isExec 변수를 추가하여 쓰레드에서 exec을 호출하는 경우 pgdir을 해제하지 않도록 구현 → 후에 종료될 때 wait에서 pgdir이 해제 될 것임.
        
        → exec을 호출한 쓰레드에서 pgdir을 해제하는 것과, 관련된 쓰레드 중에서 exec을 호출한 쓰레드가 아닌 다른 쓰레드에 의해 wait에서 pgdir이 해제되는 총 두 번 해제 되는 경우가 생기게 되므로 한 번만 해제되도록 해야함.
        
        → 쓰레드 pgdir을 공유하는 문제로 인한 것으로 특정 쓰레드가 exit되지 않았는데 pgdir을 해제하는 것, 쓰레드가 모든 종료되었음에도 pgdir을 해제하지 않는 것 등 pgdir을 해제하지 않거나, 중복해제하려는 문제를 막기 위한 구현임.
        
        1. 이후, exec된 쓰레드(프로세스)가 exit 되면 관련되어 있던 모든 쓰레드가 그 시점에 같이 종료되도록 구현
            - 즉, exec을 호출한 쓰레드가 exit을 만나 할당 해제 될 때, 기존 exec 전에 관련되어 있던 모든 쓰레드를 ZOMBIE 상태가 되어 스케줄링 되지 않도록 구현하였음.
            
              → exec 된 프로그램이 exit을 만나 종료될 때, 관련된 모든 쓰레드가 종료됌.
            
            ![test1_(11) 1](https://github.com/study-kim7507/HYU/assets/63442832/d980e167-cb7e-45dd-a151-f256afa10d86)
            ****************************************************기존에 존재하던 쓰레드들은 exec된 쓰레드의 하위 쓰레드로 모두 변경된다.****************************************************
            
            - exec된 쓰레드(프로세스)가 자신의 작업을 수행하고 exit 함수를 호출하면서 종료될 때, 좀비 상태인 기존의 하위 쓰레드도 같이 해제된다. 해당 과정은 위에서 설명한 exit과정과 동일하며 아래와 같이 종료된다. → 메인 쓰레드의 부모 쓰레드가 모두 종료시키게 된다.
                
                 ![test1_(11) 2](https://github.com/study-kim7507/HYU/assets/63442832/0dfac95e-84fd-4c4a-9b28-e88d8557445e)
                
                - sbrk : 기존의 sbrk 함수와 큰 다른 변화는 없지만, **setmemorylimit에서 설정한 메모리 크기의 최대 값보다 더 큰 메모리 할당을 요청하는 경우 메모리 크기가 변화하지 않도록 구현 → 동기화 문제를 해결하기 위해 growproc에서 인터럽트 제어 명령어 사용**
                - kill : 기존의 kill 함수와 큰 다른 변화는 없지만 특정 pid값을 통해 kill 함수가 수행될 때, pid를 공유하는 관련된 하위 쓰레드가 존재할 수도 있다. **모든 쓰레드는 자신의 부모 쓰레드, 부모 프로세스에 의해 exit되는 구현 상의 특징을 이용 메인 쓰레드의 killed 값만 1로 세팅되도록 하고, 다음 번에 메인 쓰레드가 스케줄링되고 exit될 때 모두 다 같이 종료되도록 구현**
                - sleep, pipe : 쓰레드도 하나의 프로세스로 간주하고 구현한 것이므로 큰 수정없이 작동된다.
            
            ## Implementation of System Call → [fork, exec, sbrk, kill, sleep, pipe]
            
            ---
            
            1. fork : 기존의 xv6와 변화가 없음 → proc.c
            2. exec → exec.c
            
            ```c
            extern struct threadQueue allThreadQueue;
            
            extern int isEmpty(struct threadQueue*);
            
            extern void enSubThreadQueue(struct threadQueue*, struct proc*);
            extern struct proc* deSubThreadQueue(struct threadQueue*, struct proc*);
            extern void printSubThreadQueue(struct threadQueue*);
            
            extern void enAllThreadQueue(struct threadQueue*, struct proc*);
            extern struct proc* deAllThreadQueue(struct threadQueue*, struct proc*);
            extern void printAllThreadQueue(struct threadQueue*);
            
            int
            exec(char *path, char **argv)
            {
              char *s, *last;
              int i, off;
              uint argc, sz, sp, ustack[3+MAXARG+1];
              struct elfhdr elf;
              struct inode *ip;
              struct proghdr ph;
              pde_t *pgdir, *oldpgdir;
              struct proc *curproc = myproc();
            
              
              cli(); // 여러 쓰레드에서 동시에 exec을 호출하는 경우를 막기 위해.
                     // 특정 쓰레드에서 exec을 호출한 상황이라면 타이머 인터럽트가 발생하여 스케줄링되지 않도록
                     // exec 함수 수행중에는 인터럽트가 발생하지 않도록 잠시 꺼두도록 함.
              // 1. 서브 쓰레드에서 exec을 호출한 경우, 기존의 메인 쓰레드를 호출한 서브 쓰레드의 하위 쓰레드로 만듦
              // 이후, 호출한 서브 쓰레드를 메인 쓰레드로 변경
              // 호출한 서브 쓰레드를 제외한 모든 쓰레드를 ZOMBIE 상태로 만들어 종료시켜 스케줄링되지 않도록 함.
              // 2. 하위 쓰레드가 존재하는 메인 쓰레드에서 exec을 호출한 경우, 모든 하위 쓰레드를 ZOMBIE 상태로 만듦
              // 관련된 모든 쓰레드를 모두 자신의 하위 쓰레드로 만듦
              
              if(!(curproc->isMainThread)){
                curproc->parent = curproc->threadInfo.main->parent;
            
                curproc->threadInfo.main->isMainThread = 0;
                curproc->threadInfo.main->threadQueue.subThreadNum = 0;
                curproc->threadInfo.main->threadQueue.head = NULL;
                curproc->threadInfo.main->threadQueue.tail = NULL;
              }
            
              if((curproc->isMainThread && curproc->threadQueue.subThreadNum != 0) || !(curproc->isMainThread)){
                for(struct proc *p = allThreadQueue.head; p != NULL; p = p->threadInfo.allThreadNext){
                  if((p->threadInfo.main == curproc->threadInfo.main) && (p != curproc)){
                    deSubThreadQueue(&(p->parent->threadQueue), p);
                    enSubThreadQueue(&(curproc->threadQueue), p);
            
                    p->parent = curproc;
                    p->threadInfo.main = curproc;
                    p->state = ZOMBIE;
                    curproc->isExec = 1;
                    }
                  }
                curproc->threadInfo.main = curproc;
                curproc->isMainThread = 1;
              }
              
              // 하위 쓰레드가 존재하는 메인 쓰레드에서 exec을 호출한 경우, 모든 하위 쓰레드를 ZOMBIE 상태로 만듦
              // 관련된 모든 쓰레드를 모두 자신의 하위 쓰레드로 만듦
              else if(curproc->isMainThread && curproc->threadQueue.subThreadNum != 0){
                for(struct proc *p = allThreadQueue.head; p != NULL; p = p->threadInfo.allThreadNext){
                  if(p->threadInfo.main == curproc->threadInfo.main && p != curproc){
                    deSubThreadQueue(&(p->parent->threadQueue), p);
                    enSubThreadQueue(&(curproc->threadQueue), p);
                    p->parent = curproc;
                    p->threadInfo.main = curproc;
                    p->state = ZOMBIE;
                    curproc->isExec = 1;
                  }
                }
                curproc->threadInfo.main = curproc;
              }
        
              // Commit to the user image.
              oldpgdir = curproc->pgdir;
              curproc->pgdir = pgdir;
              curproc->sz = sz;
              curproc->tf->eip = elf.entry;  // main
              curproc->tf->esp = sp;
              switchuvm(curproc);
              if(curproc->isExec == 0)  // 페이지 테이블을 두번 해제하는 것을 막기위해서.
                freevm(oldpgdir);
                  
              curproc->isExec = 0;
            
              sti();
              return 0;
            
             bad:
              if(pgdir)
                freevm(pgdir);
              if(ip){
                iunlockput(ip);
                end_op();
              }
              sti();
              return -1;
            }
            ```
            
            2-1. exit → proc.c
            
            - exit을 다음과 같이 수정하여 1. kill에 의해 exit이 수행되는 경우, 2. 하위 쓰레드에서 직접 exit을 호출하여 수행되는 경우, 3. exec된 쓰레드(프로세스)에서 exit을 수행하는 경우에 메인 쓰레드의 부모 쓰레드(프로세스)가 관련된 모든 쓰레드를 종료시키고 자원을 회수하도록 함.
            
            ```c
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
            ```
            
            1. sbrk → sysproc.c (sys_sbrk)
                - sbrk의 경우 setmemorylimit을 위해 수정한 부분 이외에는 모두 동일. 해당 코드는 위에서 이미 설명하였음.
            2. kill → 
                - 메인 쓰레드의 killed만 1로 바꾸고 후에 exit이 수행될 때 관련된 모든 쓰레드가 종료되도록 구현
            
            ```c
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
            ```
            
            1. sleep, pipe
                - 구현에서 쓰레드는 프로세스와 유사하게 동작 기존의 xv6의 큰 수정없이 잘 동작함.
                

## Result of Implementation → [thread_test, thread_exit, thread_exec, thread_kill]

---

1. thread_test
    
    ![thread_test](https://github.com/study-kim7507/HYU/assets/63442832/d3ae1074-b8ae-4157-95e0-dddb08a56a73)
    

1. thread_exit
    
    ![thread_exit](https://github.com/study-kim7507/HYU/assets/63442832/b818017e-ff03-4413-80df-3401c37dd8ea)


1. thread_exec
    
    ![thread_exec](https://github.com/study-kim7507/HYU/assets/63442832/390d4915-934b-4803-ba37-7e60a862b27a)


1. thread_kill
    
    ![thread_kill](https://github.com/study-kim7507/HYU/assets/63442832/cee96cb2-36b6-453e-baa4-8773db80eede)


## What i have to do (Trouble Shooting)

---

- 현재 디자인과 구현은 쓰레드가 상황에 따라 구조가 바뀌도록 되어있는데 굉장히 복잡하게 느껴짐. 또한, Nested Thread 즉, 쓰레드가 쓰레드를 생성하는 경우에서 깊이가 깊어질수록 더욱 많은 예외 상황이 생길 것임.
    - 어떠한 예외가 발생하여도 유연하게 대처될 수 있는 구조로 변경하는 것이 좋을 것 같다고 생각함.
    - 다양한 예외에 대한 처리가 가능하도록 더욱 많은 경우를 생각해보고 코드를 수정해야할 것 같다고 생각함
