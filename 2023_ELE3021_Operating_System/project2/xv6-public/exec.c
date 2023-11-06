#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

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
  // 서브 쓰레드에서 exec을 호출한 경우, 기존의 메인 쓰레드를 호출한 서브 쓰레드의 하위 쓰레드로 만듦
  // 이후, 호출한 서브 쓰레드를 메인 쓰레드로 변경
  // 호출한 서브 쓰레드를 제외한 모든 쓰레드를 ZOMBIE 상태로 만들어 종료시켜 스케줄링되지 않도록 함.
  // 하위 쓰레드가 존재하는 메인 쓰레드에서 exec을 호출한 경우, 모든 하위 쓰레드를 ZOMBIE 상태로 만듦
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
  
  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;
  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
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
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  
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
