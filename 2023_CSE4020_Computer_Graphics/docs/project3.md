# Project #3 - File System

> Operating System
> 

> 2017029343 김기환
> 

### Requirements

---

1. Multi Indirect
    - xv6는 기본적으로 Direct와 Single Indirect를 통해 파일의 정보를 저장합니다.
    - 기존의 방식은 최대 140개의 Block에 해당하는 파일만 쓸 수 있다는 한계가 있습니다.
    - 이번 과제는 xv6가 더 큰 파일을 다루게 할 수 있게 하기 위해, Mulit Indirect를 구현하는 것을 목표로 합니다.
2. Symbolic Link
    - xv6는 Hard Link를 지원하지만 Symbolic Link를 지원하지 않습니다.
    - Symbolic Link를 xv6에 구현하는 것을 목표로 합니다.
3. Sync
    - 일반적으로 Disk I/O는 컴퓨터 Operation 중에서 가장 느린 Operation입니다.
    - 기존의 xv6는 write operation에 대하여, Group flush를 진행합니다. 해당 방식은 특정 프로세스가 다수의 Write Operation을 발생시킬 때 성능을 저하시키는 문제점을 가지고 있습니다.
    - 위의 문제를 해결하기 위해, Sync 함수가 호출될 때만 flush하도록 하는 Buffered I/O를 구현합니다.

### Background Knowledge

---

1. inode
    - xv6는 파일 시스템에서 파일이나 디렉토리의 메타데이터와 관련된 정보를 저장하는 데이터 구조인 inode를 가지고 있습니다. 각 파일이나 디렉토리는 파일시스템의 inode 번호를 가지고 있으며, 이 inode 번호를 사용하여 해당 파일의 메타 데이터를 검색합니다.

```c
// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode 
	short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT];
};
```

- file.h에 위와 같은 구조체가 정의 되어있으며, 각 파일은 위와 같은 inode를 하나씩 할당 받고, 각각 다른 inode 번호를 가지고 있게 됩니다.

1. dinode
    - dinode는 디스크 상에 저장되는 inode의 구조를 나타내며 디스크 상의 각 inode는 dinode로서 저장되며, dinode는 inode 구조를 유지하면서 디스크 블록에 저장됩니다. dinode에는 inode와 동일하게 파일의 메타데이터와 실제 데이터가 저장되어 있는 블록에 대한 포인터들이 저장되어 있습니다. 즉, 실제 파일 시스템의 디스크에 기록되는 것은 dinode입니다.

```c
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+3];   // Data block addresses 
                           // uint addrs[NDIRECT+1] 
};
```

- fs.h에 위와 같은 구조체가 정의되어있으며, 각 파일은 실제 디스크에 기록될 때, 위와 같은 구조를 유지하며 저장됩니다.

1. Structure of xv6 file System
    - xv6는 아래와 같은 파일 시스템 구조를 갖습니다.
    
    ![%EC%BA%A1%EC%B2%98](https://github.com/study-kim7507/HYU/assets/63442832/a3d54552-9d95-4e0f-bb05-cf0b022fcdeb)
    
    - **super :** boot 부분을 제외, 파일 시스템의 시작점에 위치하며, **파일 시스템의 전반적인 정보를 담고 있습니다.**
        
        ```c
        struct superblock {
          uint size;         // Size of file system image (blocks)
          uint nblocks;      // Number of data blocks
          uint ninodes;      // Number of inodes.
          uint nlog;         // Number of log blocks
          uint logstart;     // Block number of first log block
          uint inodestart;   // Block number of first inode block
          uint bmapstart;    // Block number of first free map block
        };
        ```
        
        - 위와 같은 구조체로 정의 되어 있으며, 파일 시스템의 크기, 데이터 블록의 수, inode의 수, log 블록의 수, 각 영역의 시작점을 가리키는 데이터를 저장합니다. super 블록을 통해 파일 시스템에서 얼마나 많은 데이터가 저장되어 있으며, 사용중인 블록과 빈 블록이 어디인지 확인할 수 있게 됩니다.(bit map)
    - **log :** xv6는 로그 기능을 이용하여 저널링과 비슷한 효과를 얻습니다. 잘못된 종료와 같은 비정상적인 현상으로 파일 시스템에 데이터가 일관성을 잃지 않도록 데이터가 변경이되면, **로그에 먼저 기록하고 이후 실제 데이터를 접근하여 변경하도록 구현되어 있습니다.** 만약 비정상적인 종료와 같은 시스템 장애 시에 로그를 확인하고 이를 바탕으로 파일 시스템을 복구하도록 구현되어 있습니다.
    - **inodes** : 위에서 설명한 dinode가 해당 블록에 저장이 됩니다. 각 dinode는 파일의 메타데이터와 실제 디스크에서의 위치가 어디인지 가리키는 포인터가 함께 저장이 됩니다. 즉, **실제 파일에 접근을 하기 위해서는 inodes 블록에 저장이 되어 있는 dinode를 먼저 확인 그 이후 데이터가 저장되어있는 블록을 가리키는 포인터를 확인, 해당 위치로 접근하여 데이터에 접근하도록 구현 되어 있습니다.**
    - ********************bit map :******************** 어떤 블록이 비어있는지 신속하게 찾을 수 있도록 도와주는 정보가 담긴 블록으로, 0과 1의 두 비트를 사용하여, 특정 블록이 비어있는 경우 0이, 비어있지 않은 경우 1이 저장되어 있습니다.
    
2. Multi Indirect
    - xv6는 기본적으로 Direct와 Single Indirect를 통해 파일의 정보를 저장합니다. dinode 구조체에서 확인할 수 있듯이, 12개의 Direct와 하나의 Single Indirect를 통해 데이터가 저장된 블록의 위치를 저장합니다.
    - xv6는 하나의 블록 크기는 512 bytes로, 512 bytes 블록을 직접 가리키는 12개의 Direct와 총 128개의 블록의 위치를 저장한 블록을 가리키는 하나의 Single Indirect로 구성되어 총 512 * 140 = 70KB. 최대 파일의 크기가 70KB로 정해져 있습니다.
    
    ![1](https://github.com/study-kim7507/HYU/assets/63442832/f470414f-c5ff-47a4-9797-9607933b45f9)
    
3. Hard Link and Symbolic Link
    - 하드링크와 소프트링크는 파일 시스템에서 파일을 참조하는 방법입니다.
    - 하드링크는 파일의 실제 데이터를 가리키는 반면, 소프트 링크는 파일의 경로를 가리킵니다.
    - 하드링크는 파일의 실제 데이터를 가리키기 때문에, 하드링크를 생성하면 하드링크 파일은 원본데이터와 동일한 파일 크기를 가집니다. 반면, 소프트링크는 원본파일의 경로를 데이터로 가지고 있어, 원본파일의 경로의 길이(크기)만큼의 크기를 가집니다.
    - 또한, 하드링크는 원본파일이 삭제되더라도, 데이터에 접근이 가능하지만, 소프트링크는 연결된 원본파일이 삭제되면, 데이터에 접근이 불가능해집니다.
        
        ![2](https://github.com/study-kim7507/HYU/assets/63442832/ceda7633-dc7d-43f9-a844-623057ca53cb)
        
        ******************Hard Link******************
        
        ![3](https://github.com/study-kim7507/HYU/assets/63442832/388cb3c2-ee86-43c8-b139-a6254f9f393d)
        
        ******************Soft Link******************
        
    
4. Buffered I/O
    - 일반적으로 Disk I/O는 컴퓨터 Operation 중에서 가장 느린 Operation으로 자주, 즉, 다수의 Write Operation이 발생할 때마다 디스크에 접근하여 성능을 저하시킵니다.
    - begin_op 함수를 통해 파일시스템 접근을 시작함을 알리고, end_op 함수를 통해 파일시스템 접근을 종료함을 알리는데, 해당 과정에서 commit 되면서, 디스크 블록에 접근하여 데이터가 쓰여집니다. 해당 과정에서 Disk I/O가 발생하는데 이 Disk I/O 과정에서 성능의 저하가 나타납니다.

### Implementation - Multiple Indirect

---

1. fs.h
    - 기존의 xv6는 12개의 Direct와 1개의 Indirect를 가지고 있습니다. 각각의 dinode에서 addrs 배열에 주소가 저장되어 있었는데, 이를 수정하여 10개의 Direct와 3개의 Indirect가 되도록 구현하였습니다.
    - D_NINDIRECT 매크로로 Double Indirect가 가리키는 디스크 상의 Data Block의 수가 128 * 128 총, 16384개의 블록을 가리키도록 하였습니다.
    - T_NINDIRECT 매크로로 Triple Indirect가 가리키는 디스크 상의 Data Block의 수가 128 * 128 * 128 총, 2097152개의 블록을 가리키도록 하였습니다.
    - addrs[10]은 Single Indirect, addrs[11]는 Double Indirect, addrs[12]는 Triple Indirect로 구현하였습니다.
    - dinode의 크기가 512(블록사이즈)의 약수가 되도록 구현되지 않으면 부팅하여 파일시스템이 만들어질 때, assert가 발생하여 위와 같은 개수로 Direct와 Indirect를 구현하였습니다.

```c
#define NDIRECT 10
#define NINDIRECT (BSIZE / sizeof(uint)) // 하나의 Indirect Block이 가리킬 수 있는 블록의 개수
#define D_NINDIRECT (NINDIRECT * NINDIRECT)
#define T_NINDIRECT (D_NINDIRECT * NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT + D_NINDIRECT + T_NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+3];   // Data block addresses 
                           // uint addrs[NDIRECT+1] -> 기존에는 indirect가 하나, Double Indirect, Triple Indirect 구현을 위해 2개로 늘림.
};
```

1. fs.c - bmap, itrunc
- 기존의 bmap 함수를 수정하여 Double Indirect와 Triple Indirect를 구현하였으며, 각각 128의 몫과 나머지 연산을 통해서 구현하였습니다.
- 추가로 기존의 itrunc 함수도 수정하여 파일의 데이터가 지워지고 메타데이터만 남는 trunc 과정에서도 연결된 블록들의 데이터가 지워지고 메타데이터만 남도록 구현하였습니다.
- 대부분의 과정은 Single Indirect를 과정과 비슷하며, 중첩 반복문을 통해 Mulitple Indirection을 구현하였습니다.
- 다음은 fs.c의 bmap 함수와 itrunc 함수입니다.

```c
/ Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  // Direct.
  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  // Single Indirect
  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;
  
  // Double Indirect
  if(bn < D_NINDIRECT){
    if((addr = ip->addrs[NDIRECT+1]) == 0)
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    int quotient = bn / 128;
    int remainder = bn % 128; 
    
    if((addr = a[quotient]) == 0){
      a[quotient] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[remainder]) == 0){
      a[remainder] = addr = balloc(ip->dev);
      log_write(bp);
    }

    brelse(bp);
    return addr;
  }
  bn -= D_NINDIRECT;

  // Triple Indirect
  if(bn < T_NINDIRECT){
    if((addr = ip->addrs[NDIRECT+2]) == 0)
      ip->addrs[NDIRECT+2] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    int quotient = bn / (128 * 128);
    int remainder = bn % (128 * 128);
    int remain_quotient = remainder / 128;
    int remain_remainder = remainder % 128; 

    if((addr = a[quotient]) == 0){
     a[quotient] = addr = balloc(ip->dev);
     log_write(bp);
    }
    brelse(bp);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[remain_quotient]) == 0){
      a[remain_quotient] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[remain_remainder]) == 0){
      a[remain_remainder] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
static void
itrunc(struct inode *ip)
{
  int i, j, k;
  struct buf *bp;
  uint *a, *b, *c;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  // Single Indirect Block Free
  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  // Double Indirect Block Free
  if(ip->addrs[NDIRECT + 1]){
    bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
    a = (uint*)bp->data;

    struct buf *temp = bp;

    for(i = 0; i < NINDIRECT; i++){
      if(a[i]){
        bp = bread(ip->dev, a[i]);
        b = (uint*)bp->data;

        for(j = 0; j < NINDIRECT; j++){
          if(b[j])
            bfree(ip->dev, b[j]);
        }
        brelse(bp);
        bfree(ip->dev, a[i]);
      }
    }
    brelse(temp);
    bfree(ip->dev, ip->addrs[NDIRECT + 1]);
    ip->addrs[NDIRECT + 1] = 0;
  }

  // Triple Indirect Block Free
  if(ip->addrs[NDIRECT + 2]){
    bp = bread(ip->dev, ip->addrs[NDIRECT + 2]);
    a = (uint*)bp->data;
    struct buf *temp = bp; 
    for(i = 0; i < NINDIRECT; i++){
      if(a[i]){
        bp = bread(ip->dev, a[i]);
        b = (uint*)bp->data;
        struct buf *temp1 = bp;
        for(j = 0; j < NINDIRECT; j++){
          if(b[j]){
            bp = bread(ip->dev, b[j]);
            c = (uint*)bp->data;
            for(k = 0; k < NINDIRECT; k++){
              if(c[k])
                bfree(ip->dev, c[k]);
            }
            brelse(bp);
            bfree(ip->dev, b[j]);
          }
        }
        brelse(temp1);
        bfree(ip->dev, a[i]);
      }
    }
    brelse(temp);
    bfree(ip->dev, ip->addrs[NDIRECT + 2]);
    ip->addrs[NDIRECT + 2] = 0;
  }
  ip->size = 0;
  iupdate(ip);
}

```

### Implemetation - Soft Link(Symbolic Link)

---

- **심볼릭링크 파일은 하드링크 파일과 다르게 자신만의 inode를 따로 가지고 있으며 심볼릭 링크 파일의 데이터는 원본파일의 경로가 저장되어 있습니다. 심볼릭링크 파일을 열게되면 해당 파일에 적혀있는 원본파일의 경로를 읽고, 원본파일이 열리도록 구현하였습니다. 따라서, symlink 시스템콜을 추가하고, open 함수를 수정하였습니다.**
- **ls와 같이 open을 통하여 파일의 메타데이터를 얻어오는 명령어에서 원본파일의 메타데이터가 아닌 심볼릭링크 파일의 메타데이터를 얻어 올 수 있도록 리눅스의 구현을 참고하여 lstat 시스템콜 또한 추가하였습니다.**
    
    
    1. stat.h
        - 특정 파일이 심볼릭링크 파일임을 구분짓기 위해 새로운 타입인 T_SYMLINK를 추가하였습니다.
    
    ```c
    
    #define T_SYMLINK 4
    ```
    
    1. sysfilc.c - sys_symlink()
        - ln -s 명령어를 통해서 심볼릭링크 파일을 만들면, T_SYMLINK 타입의 하나의 inode가 할당되고, writei 함수를 통해 inode를 통해서 해당 파일에 데이터를 써줍니다. 해당 데이터는 원본파일의 경로가 됩니다. **즉, 심볼릭링크 파일을 하나 만들게 되면 해당 파일에는 원본파일의 경로에 해당하는 데이터가 써지도록 구현하였습니다.**
        - **sync() 구현을 진행하게 되면 심볼링링크, 하드링크 파일 생성 시에 버퍼 사이즈보다 작은 경우 디스크에 반영되지 않는 문제가 있음. 따라서, 이를 해결하기 위해서 symlink와 기존의 link가 호출될 때에는 바로 디스크에 반영되도록 sync()를 호출하도록 구현하였다.**
    
    ```c
    // Symbolic Link 생성을 위해, symlink 호출 시, create 함수를 통해서 새로운 inode를 할당받음.
    // 이때, 새로 만들어진 Symbolic Link 파일은 T_SYMLINK라는 타입을 가지도록 stat.h에 매크로로 정의.
    // 리턴 받은 inode 포인터를 통해 실제 Disk Block에 원본파일의 경로에 해당하는 데이터를 저장.
    // Symbolic Link 파일을 open시에, 원본 파일을 열도록 open 함수도 추가로 수정함.
    int 
    sys_symlink(void)
    {
      struct inode *ip;
      // char name[DIRSIZ], *new, *old;
      char *new, *old;
       
      if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
        return -1;
      
      begin_op();
      if((ip = create(new, T_SYMLINK, 0, 0)) == 0) {
        end_op();
        return -1;
      }
    
      if(writei(ip, old, 0, strlen(old)) != strlen(old)) {
        panic("symlink: writei");
      }
      
      iunlockput(ip);
      end_op();
      sync();
      return 0;
    }
    ```
    

1. sysfile.c - sys_open()
    - 만약 심볼릭링크 파일이 open되게 되면 해당 파일에 적혀있는 원본파일의 경로를 읽고, 경로를 바탕으로 원본파일의 inode를 가리키는 포인터를 반환받습니다. 이후, 해당 inode 포인터를 바탕으로 파일을 만들고 File descriptor를 할당 받습니다.
    - 즉, **심볼릭링크 파일이 open되게 되면 원본파일의 File descriptor가 반환되어 사용자는 원본파일에 대해 read/write 작업을 수행하게 됩니다.**

```c
		// 만약 Symbolic Link 파일을 Open 하게 되면, Symbolic Link 파일에 적혀있는 원본파일의 경로를 읽음.
    // 원본파일의 경로를 읽어 이를 바탕으로 원본파일의 inode를 반환받고, 원본파일이 open되도록 구현함.
    if(ip->type == T_SYMLINK){
      char dst[DIRSIZ] = {0};
      if(readi(ip, dst, 0, DIRSIZ) == 0)
        panic("open: readi (symlink)");
      if(ip->type == T_DIR)
        panic("open: original file is DIR");
      iunlockput(ip);
      if((ip = namei(dst)) == 0) {
        end_op();
        return -1;
      }
      ilock(ip);
		}
```

1. sysfile.c - sys_lstat()
    - open 함수를 수정하여 원본파일이 열리도록 구현하였기 때문에, ls를 통해 현재 폴더의 내용을 나열하는 과정에서 원본파일의 정보가 출력되는 문제가 있어 리눅스의 구현과정과 동일하게 lstat 시스템콜을 만들어 심볼릭링크 파일에 대한 정보가 출력될 수 있도록 구현하였습니다.
    
    ```c
    // File의 타입이 Symbolic Link 파일일 경우, lstat을 통하여 Meta Data를 확인할 수 있음.
    int
    sys_lstat(void)
    {
      struct inode *ip;
      char *dst;
      struct stat *st;
      
      if(argstr(0, &dst) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
        return -1;
      
      begin_op();
      if((ip = namei(dst)) == 0) {
        end_op();
        return -1;
      }
      
      if(ip->type != T_SYMLINK) {
        end_op();
        return -1;
      }
    
      stati(ip, st);
      end_op();
    
      return 0;
    }
    ```
    
2. ls.c
    - 기존의 ls 명령어는 현재 폴더 내용을 나열하는 명령어인데, 각 파일의 경로를 통해 open 함수를 거쳐 File Descriptor를 얻어 낸 후, 이를 바탕으로 파일의 메타데이터를 얻어내 이를 쉘에 출력하여 줍니다. 하지만 위의 구현에서 심볼릭링크 파일이 open 함수를 통해 open되면 원본파일의 File Descriptor를 반환하도록 구현하였기 때문에 ls 명령어 수행시 심볼릭링크 파일이 가리키고 있는 원본 파일의 메타데이터를 쉘에 출력하게 됩니다. 따라서 위에서 설명한 lstat 시스템 콜을 통해서 만약 ls 명령어를 통해 출력하는 파일이 심볼릭링크 파일일 경우, 심볼릭링크 파일 자신의 메타데이터를 출력하도록 ls.c 파일도 변경하였습니다.
    
    ```c
    case T_DIR:
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
          printf(1, "ls: path too long\n");
          break;
        }
        strcpy(buf, path);
        p = buf+strlen(buf);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
          if(de.inum == 0)
            continue;
          memmove(p, de.name, DIRSIZ);
          p[DIRSIZ] = 0;
          // Symbolic Link File이 아닌 경우, 기존과 같이 출력되도록 구현.
          // Symbolic Link File인 경우, stat 구조체에 Symbolic Link File과 관련된 MetaData가 저장되도록 함.
          if(lstat(buf, &st) < 0){ 
            if(stat(buf, &st) < 0){
              printf(1, "ls: cannot stat %s\n", buf);
              continue;
            }
          }
          printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
        }
        break;
    ```
    
    1. ln.c
        - 기존의 xv6의 ln 명령어는 하드링크 파일만을 생성할 수 있도록 구현되어 있습니다. 과제의 명세에 맞게 옵션에 따라 하드링크, 심볼릭링크를 구분지어 만들 수 있도록 구현하였습니다.
    
    ### Implementation - Sync()
    
    ---
    
    1. log.c - end_op()
        - 기존의 xv6의 구현과 거의 동일하지만, 버퍼가 가득찼을 경우에만 sync() 시스템콜을 호출하여 버퍼에 저장된 데이터가 디스크블록에 쓰이도록 구현하였습니다.(commit)
    
    ```c
    void
    end_op(void)
    {
      int do_commit = 0;
    
      acquire(&log.lock);
      log.outstanding -= 1;
      if(log.committing)
        panic("log.committing");
      // 버퍼가 가득찼을 때에만, sync() 함수가 호출되어 commit 되도록 구현.
      if(log.outstanding == 0){
        if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
          do_commit = 1;
          log.committing = 1;
        }
      } else {
        // begin_op() may be waiting for log space,
        // and decrementing log.outstanding has decreased
        // the amount of reserved space.
        wakeup(&log);
      }
      release(&log.lock);
      
      if(do_commit){
       // call commit w/o holding locks, since not allowed
       // to sleep with locks.
       sync();
      }
    }
    ```
    
    1. log.c - sync()
        - 기존의 commit 함수가 진행하던 버퍼에 저장된 데이터가 디크스 블록에 쓰여지는 과정을 sync() 함수가 하도록 구현하였으며, 그때, commit 되어 쓰여지는 블록의 수를 반환하도록 하였습니다. 만약 commit 되어 쓰여지는 블록이 없을 경우 -1을 리턴하도록 구현하였습니다.
        - 해당 함수의 Wrapper 함수는 sysfile.c에 구현되어 있으며, 단순히 sync() 함수를 호출하는 식으로 구현하였습니다.
    
    ```c
    // 기존의 commit 과정을 sync 함수로 묶고, 그에대한 리턴값으로
    // 커밋을 진행한 블록의 수를 반환하도록 구현
    int
    sync()
    {
      int blockNum = log.lh.n;
      if(blockNum <= 0)
        return -1;
      commit();
      acquire(&log.lock);
      log.committing = 0;
      wakeup(&log);
      release(&log.lock);
    	return blockNum;
    }
    ```
    
    1. param.h
        - 기존의 xv6는 버퍼사이즈가 10 * 3 * 512B = 15K로 상당히 작기 때문에, 버퍼가 금방 가득차 자주 Disk I/O가 발생할 수 있기 때문에, 버퍼사이즈를 좀더 크게 수정하였습니다.
        
        ```c
        #define NPROC        64  // maximum number of processes
        #define KSTACKSIZE 4096  // size of per-process kernel stack
        #define NCPU          8  // maximum number of CPUs
        #define NOFILE       16  // open files per process
        #define NFILE       100  // open files per system
        #define NINODE       50  // maximum number of active i-nodes
        #define NDEV         10  // maximum major device number
        #define ROOTDEV       1  // device number of file system root disk
        #define MAXARG       32  // max exec arguments
        #define MAXOPBLOCKS  10  // max # of blocks any FS op writes
        #define LOGSIZE      (MAXOPBLOCKS*10)  // max data blocks in on-disk log
        #define NBUF         (MAXOPBLOCKS*10)  // size of disk block cache
        #define FSSIZE       40000 // size of file system in blocks
                                    // Real Disk Size
        ```
        
    
    ### Result
    
    ---
    
    1. userwrite_test.c - MultiIndirect 테스트
        - 테스트를 위한 유저프로그램을 만들고, 기존의 xv6의 파일 시스템에서 지원하는 70KB보다 큰 파일 TEST라는 이름을 가진 파일을 만들도록 하였습니다.
        - 정상적으로 큰 크기의 파일이 쓰여진 것을 확인할 수 있었습니다.
            
            ![6](https://github.com/study-kim7507/HYU/assets/63442832/86a9a9f6-8231-415a-bfbe-9db88657d653)
            
    2. Symbolic Link 테스트
        - 기존 xv6에 존재하는 README 파일을 원본으로한 TEST1 이라는 이름을 가진 심볼릭링크 파일 생성하였습니다. 해당 파일에는 README 라는 파일명이 적혀있어 6바이트의 크기임을 확인 할 수 있습니다.
        - cat 명령어를 통해 TEST1 파일을 확인할 시 원본 파일인 README 파일의 내용이 출력됨 또한 확인하였습니다.
        - 원본 파일인 README 파일을 삭제하고 TEST1 파일을 cat할 경우 하드링크와 다르게 원본파일이 지워져 실패함 또한 확인하였습니다.
            
            ![7](https://github.com/study-kim7507/HYU/assets/63442832/4855dfbe-735b-4bc9-a523-051d5d925a0b)
            
            ![8](https://github.com/study-kim7507/HYU/assets/63442832/78f0bb1f-ef10-4767-9b29-ba99e2b9aab1)

        
    3. userwrite_test.c - sync 테스트
        - 테스트를 위한 유저프로그램을 만들어 TEST라는 이름을 가진 파일을 하나 만들고, 해당 파일에 5바이트의 데이터를 쓰도록 하였습니다.
        - 해당 테스트 파일 종료시 sync()를 호출하게 되면, xv6 재부팅 시에도 파일이 남아있지만, sync()를 호출하지 않게되면 디스크에 버퍼가 기록되지 않아 파일이 남아있지 않게 됩니다.

![4](https://github.com/study-kim7507/HYU/assets/63442832/33527f1d-429a-488a-8b56-7ca915cca007)

**Sync() 호출시**

![5](https://github.com/study-kim7507/HYU/assets/63442832/6dd52d20-8179-492c-a8ed-bd317463d8ed)

**Sync() 호출 안할시**

### **Trouble**

---

1. 심볼릭링크 파일 생성 후, 바로 ls 명령어를 통해 심볼릭링크 파일의 메타데이터 확인시, 부팅 즉시 ls 명령어를 통해 심볼릭링크 파일의 메타데이터 확인시 원본파일의 메타데이터가 나타나는 문제가 있습니다. 다시 ls하게 되면 심볼릭링크 파일의 메타데이터가 확인됩니다. 
    - inode 포인터를 통해 심볼릭링크 파일의 inode 구조체의 각 값을 확인했을 때, 심볼릭링크 파일의 값이 나타나는 것으로 보아(inode num, size 등) 정상적으로 심볼릭링크 파일이 생성된 것이지만, lstat, ls 등 심볼릭링크 파일 stat 구조체를 가져오는 부분에서 문제가 있는 것으로 확인됩니다.
    
     → 추후 수정이 필요할 것이라고 생각이 듭니다.
