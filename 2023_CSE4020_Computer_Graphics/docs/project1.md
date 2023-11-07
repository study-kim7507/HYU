
# Project #1 - MLFQ Scheduler

> Operating System
> 

> 2017029343 김기환
> 

## Design Goal

---

- **MLFQ Scheduler**
    - 다양한 TimeQuantum을 가지는 여러 Queue를 동시에 운용. Level0 부터 Level2 까지 3개의 Queue와 SLEEPING(WATING) 상태의 프로세스를 삽입할 Sleep Queue로 총 4개의 Queue 구성.
    - Level0 Queue와 Level1 Queue는 Round Robin 방식을 채택, Level2 Queue는 Priority가 존재하는 FCFS 방식을 채택.
- **********************************Priority Boosting**********************************
    - Level2 Queue의 Priority가 존재하는 FCFS 방식은 우선순위가 높은 프로세스가 우선적으로 스케줄링되어 우선순위가 낮은 프로세스는 영영 실행되지 않는 Starvation에 빠지는 문제가 생김
    - GlobalTicks이 100이 될 때 마다 모든 프로세스를 Level0 Queue로 이동시켜 특정 프로세스가 Starvation에 빠지는 문제를 해결함.
- **setPriority**
    - setPriority System Call을 통해 pid, priority를 전달하여, pid에 해당하는 프로세스의 priority를 변경시킬 수 있다.
- **schedulerLock / schedulerUnlock System Call**
    - CPU를 독점하는 특정 프로세스가 존재할 수 있음. schedulerLock System Call과 schedulerUnlock System Call을 통해서 특정 프로세스가 원할 때 CPU를 독점하고, 독점을 포기하도록 구현.

## Transition of process states in xv6 - Background Knowledge

---

- 기존의 xv6는 RR Scheduler 방식을 채택. 프로세스는 다음과 같은 상태 변화가 일어나게 된다.

![test](https://github.com/study-kim7507/HYU/assets/63442832/e3b3aee6-8742-4b83-bc98-6ef7366e0107)

**Process Lifecycle**

1. **UNUSED → EMBRYO**
    - xv6는 부팅하는 과정에서 Process Table을 모두 UNUSED 상태의 프로세스로 채움. UNUSED는 실행 가능한 프로세스가 아닌 메모리에 상주하지 않는 프로세스로 아직 생성되지 않은 프로세스 혹은 종료된 프로세스를 의미 한다.
    - allocproc() 함수에서 Process Table을 돌면서 UNUSED 상태인 프로세스를 찾고, 해당 프로세스를 EMBRYO 상태로 바꾼다.
2. **EMBRYO → RUNNABLE** 
    - EMBRYO 상태인 프로세스를 부모프로세스를 복제하는 fork() 함수에서 RUNNABLE 상태로 바꾸게 되고 해당 프로세스는 Ready Queue에서 스케줄링 되도록 대기하게 된다.
3. **RUNNABLE ↔ RUNNING** 
    - 기존의 xv6는 Round-Robin Scheduling 방식을 채택, scheduler() 함수 내부에서 Process Table을 반복문을 통해서 순회하며 RUNNABLE 상태인 프로세스를 찾아 RUNNING 으로 프로세스 상태를 변화 시키고, 해당 프로세스를 CPU가 수행하도록 함.
    - 프로세스가 CPU를 점유하여 작업을 수행하던 중, 인터럽트(Timer)가 발생하게 되면, CPU는 해당 프로세스로 부터 벗어나 인터럽트 핸들러를 수행하러 이동, 해당 프로세스는 RUNNABLE 로 다시 상태 변화가 일어나게 된다.
    
4. **RUNNING → SLEEPING**
    - 프로세스가 CPU를 점유하여 작업을 수행하던 중, I/O와 같은 이벤트가 발생하게 되었을 때, 프로세스가 CPU를 점유하여 이벤트가 완료되기를 기다리는 것은 비효율적이므로, CPU 점유를 포기하고 프로세스의 상태를 SLEEPING 상태로 전환한다.
5. **SLEEPING → RUNNABLE**
    - 이벤트가 완료되길 SLEEPING 상태로 기다리던 프로세스가 이벤트가 완료되어 wakeup1() 함수에서 RUNNABLE로 상태를 전환하고, Ready Queue에서 스케줄링되기를 기다린다.
6. ************RUNNING → ZOMBIE → UNUSED************
    - 프로세스의 모든 작업이 완료된 상황으로 해당 프로세스는 exit() 함수를 호출함으로써 ZOMBIE로 상태를 전환. 이후, 부모 프로세스는 wait() 함수를 통해서 자식 프로세스가 종료되었음을 확인하고, 이때 ZOMBIE 상태인 자식프로세스는 UNUSED로 상태가 전환된다.
    

## Design of CPU scheduler

---

### Flow Chart

![test1](https://github.com/study-kim7507/HYU/assets/63442832/0f461b11-7cd8-45a8-94a2-96c5fdd32005)

**CPU Scheduling Flowchart**

### **Data Structure**

- 총 4개의 Doubly Linked List Queue로 MLFQ를 구현.
    - L0, L1, L2 Queue : 서로 다른 TimeQuantum을 가지며, L0, L1 Queue는 기존의 xv6와 동일한 RR 방식을 채택, L2 Queue는 Priority FCFS 방식을 채택하여 구현.
    - 각 큐마다 서로 다른 TimeQuantum이 존재하며, 해당 큐에서 주어진 TimeQuantum을 다 사용한 프로세스는 다음 레벨의 큐로 이동하게 된다.
        - L0 Queue는 4 Ticks, L1 Queue는 6 Ticks, L2 Queue는 8 Ticks
        - L2 Queue에서는 주어진 TimeQuantum을 다 사용한 경우, Priority를 감소 시켜, 우선적으로 스케줄링 되도록 한다.
    - Sleep Queue : I/O나 이벤트 발생으로 SLEEPING 상태로 전환된 프로세스가 삽입된다.

![test2](https://github.com/study-kim7507/HYU/assets/63442832/d32902ee-c728-49d4-ac21-a677e16cec93)


**MLFQ Data Structure**

### **MLFQ Scheduler**

1. **userinit()**
    - 최초의 프로세스인 init 프로세스가 생성되는 함수.  userinit() 함수 내부에서 모든 큐를 초기화 하며, proc 구조체 내부의 변수들 또한 초기화 한다. 모든 프로세스는 처음 생성된 후 L0 Queue의 맨 뒤로 삽입되므로, init 프로세스 또한 L0 Queue로 삽입된다.
2. **fork()**
    - init 프로세스 이후 생성되는 프로세스는 부모프로세스로부터 fork() 되어 생성, init 프로세스를 제외한 모든 프로세스는 fork() 함수를 통해 생성이 된다. 따라서 fork() 함수 내부에서 새로 생성되는 프로세스의 상태를 RUNNABLE 상태로 설정하고, 이후 변수들을 초기화하는 동시에 L0 Queue의 맨뒤로 삽입한다.
3. **scheduler()**
    - Sleep Queue는 I/O 또는 이벤트의 발생으로 SLEEPING 상태에 빠진 프로세스들이 존재하는 큐.
    - 처음 생성된 프로세스는 RUNNABLE 상태로 됨과 동시에 L0 Queue의 맨 뒤로 삽입되며, 해당 큐에서 스케줄링 될 때 까지 RUNNABLE 상태로 대기하게 된다.
    - CPU는 스케줄링 할 프로세스를 선택함에 있어서 가장 먼저 L0 Queue에 프로세스가 존재하는지 확인하고, 프로세스가 존재하면 L0 Queue의 가장 맨 앞의 존재하는 프로세스를 큐에서 꺼냄과 동시에 CPU 자원을 할당하여 작업을 수행하게 된다. 같은 과정으로 L0 Queue에 프로세스가 하나도 존재하지 않는 경우, L1 Queue의 가장 맨 앞에 존재하는 프로세스를 스케줄링하며, L1 Queue에도 아무런 프로세스가 존재하지 않는 경우 L2 Queue에서 스케줄링 할 프로세스를 찾게 된다.
    - 만약, 모든 프로세스가 SLEEPING 상태가 되어 Sleep Queue에 존재하는 경우, RUNNABLE 상태가 되어 스케줄링되길 기다리는 프로세스가 나타나게 될 때 까지 scheduler() 함수 내부의 반복문을 돌면서 기다리게 된다.
    - 스케줄링될 프로세스가 선택되면 기본적으로 큐에서 해당 프로세스를 제거하고, 해당 프로세스의 작업을 수행하게 된다. 이후 인터럽트나 I/O, 이벤트가 발생하여 CPU 자원을 다른 프로세스에게 넘길 상황이 되면, 프로세스의 상태를 확인하여 그에 맞는 큐로 다시 삽입되어 진다. 따라서, L0, L1, L2 Queue에는 RUNNALBLE 상태의 프로세스만 존재하게 되고, SLEEPING 상태의 프로세스는 Sleep Queue에만 존재하게 된다.
4. **yield()**
    - xv6는 1 Tick마다 Timer 인터럽트가 발생하며, CPU가 다른 프로세스를 스케줄링 하도록 하는데, CPU 자원을 할당 받아 수행 중이던 프로세스가 이러한 Timer 인터럽트에 의해 CPU 점유 권한을 다른 프로세스에게 넘길 때, yield() 함수를 거쳐 Context Switch가 발생한다.
    - 따라서, yield() 함수 내부에서 PriorityBoosting을 위한 GlobalTicks과 프로세스마다 존재하는 LocalTicks을 증가시켜주며, 증가된 LocalTicks을 확인하여 주어진 큐에서의 TimeQuantum을 다사용한 경우 다음 레벨의 큐로 이동시키게 한다.
        - L0 → L1 → L2 순으로 큐를 이동하며, L2 Queue에서는 주어진 TimeQuantum을 다 사용하게 되면 Priority 값을 3 → 2 → 1 → 0 순으로 감소시켜 Priority가 낮은 프로세스가 먼저 스케줄링 되도록 한다.
        - L0 Queue와 L1 Queue로 돌아가게 되는 프로세스인 경우, Queue의 가장 맨 뒤에 삽입되도록하며, L2 Queue로 돌아가는 프로세스는 L2 Queue에서 같은 Priority를 갖는 프로세스 중에서 가장 뒤에 삽입되도록 한다.
5. **sleep()**
    - 스케줄링된 프로세스가 I/O 나 이벤트 발생으로 SLEEPING 상태가 되는 함수로, 해당 함수에서 SLEEPING 상태로 전환이 일어나고, 해당 프로세스는 L0, L1, L2가 아닌 SleepQueue의 맨 뒤로 삽입이 된다.
    - Timer 인터럽트가 발생하기 전에 CPU 자원을 놓게 된 상황이므로 yield() 함수를 통해 GlobalTicks과 LocalTicks이 증가하지 않게 되므로 sleep() 함수 내부에서 GlobalTicks과 LocalTicks이 증가되도록 하며, 증가된 LocalTicks 값을 기준으로 SLEEPING 상태에서 벗어나게 될 경우, 어떠한 Level의 큐로 돌아갈지 설정해준다.
6. **wakeup1()**
    - SLEEPING 상태의 프로세스가 I/O나 이벤트에 대한 처리가 다 끝나 RUNNABLE 상태로 돌아가기 위한 함수로, SleepQueue에서 해당하는 프로세스를 찾고, 프로세스의 Level 즉, 돌아가야할 큐의 Level을 확인, Priority 등을 확인하여, 알맞은 레벨의 큐, 알맞은 위치로 프로세스가 삽입되도록 한다.

## Design of PriorityBoosting

---

1. **scheduler()**
    - I/O, 이벤트, 인터럽트 등으로 본래 수행하던 프로세스의 Context를 해당 프로세스의 Address Space에 저장한 뒤, 다른 프로세스를 찾기 전 scheduler() 함수 내부에서 GlobalTicks 값을 확인한다. L2 Queue에서 발생할 수 있는 Starvation 현상을 막기 위해 GlobalTicks이 100이 되었으면 priorityBoosting() 함수를 호출하여  모든 프로세스의 상태 (LocalTicks, Priority)를 초기화 하고, 모두 L0 Queue로 이동시키는 Priority Boosting을 진행한다.
2. **priorityBoosting()**
    - L2 Queue는 Priority FCFS 스케줄링하므로 Priority가 먼저 감소하는, 우선순위가 먼저 높아지는 프로세스가 계속 우선적으로 스케줄링된다. 즉, 특정 프로세스는 영영 실행되지 않는 경우가 생기기도 한다는 것이다 (Starvation). 따라서 지정된 시간마다 (100 Ticks) 모든 프로세스의 LocalTicks과 Priority 등을 초기화하고 모두 L0 큐로 이동시키는 PriorityBoosting이 진행되어야 한다.
    - scheduler() 내부에서 GlobalTicks이 100이 되는 순간, priorityBoosting() 함수가 호출되며, 해당 함수는 다음과 같이 진행된다.
    
    ![test 1](https://github.com/study-kim7507/HYU/assets/63442832/b3603bb3-3260-442f-98a1-5739427f6583)

    - 각 큐의 Head와 Tail을 서로 연결을 해준 뒤, 각 프로세스의 Level 멤버 변수와 Priority, LocalTicks을 초기화 시키고, L1 Queue와 L2 Queue를 초기화 시키면서 모든 프로세스를 L0 Queue로 이동시키게 된다.
        - 경우에 따라 프로세스가 존재하지 않는 큐도 있을 수 있기 때문에 조건문을 통해 NULL 포인터를 가리키지 않도록 하였다.
        
        ![test1 1](https://github.com/study-kim7507/HYU/assets/63442832/4c0f32a3-279b-4219-a34e-f670ee2bb9ba)

    - SleepQueue에 존재하는 프로세스들 또한 Level, Priority, LocalTicks을 초기화 시켜줌으로서, SLEEPING 상태에서 RUNNABLE 상태로 전환이 될 때, L0 Queue의 맨 뒤로 삽입될 수 있도록 한다.

## Design of setPriority

---

1. **setPriority(int pid, int priority)**
    - 인자로 전달받은 pid에 해당하는 프로세스의 priority를 변경하여 준다. 0, 1, 2, 3이 아닌 다른 값을 priority로 전달하거나, 전달받은 pid에 해당하는 프로세스가 존재하지 않는 경우 아무런 변화가 나타나지 않는다.
    - priority를 변경하려는 프로세스가 L0, L1 Queue에 존재하는 프로세스라면 간단하게 priority만 변경하여 주면 된다.
        - L0 Queue와 L1 Queue는 RR Scheduler 이므로 priority의 영향을 받지 않기 때문.
    - priority를 변경하려는 프로세스가 L2 Queue에 존재하는 프로세스이며, 현재 CPU가 수행중인 프로세스인 경우. scheduler() 함수에서 스케줄링될 때 이미 L2 Queue에서 빠져나온 상태이므로 단지 자신의 priority만 변경해주면 된다. 이후 인터럽트(Timer)에 의해서 다시 L2 Queue로 돌아갈 때, 자동으로 변경된 priority를 기반으로 순서가 재정립되어 들어간다. (For Priority FCFS)
    - priority를 변경하려는 프로세스가 L2 Queue에 존재하는 프로세스이며, 현재 CPU가 수행중인 프로세스가 아닌 경우, priority를 변경하여 주고, 해당 프로세스를 L2 Queue에서 뽑아낸 뒤, 다시 L2 Queue의 priority에 의한 순서가 맞도록 삽입하여 준다. (For Priority FCFS)
    

## Design of **schedulerLock / schedulerUnLock**

---

1. **schedulerLock(int password)**
    - schedulerLock을 호출한 프로세스는 Unlock이 될 때 까지 L0 Queue의 가장 맨 앞으로 계속 삽입하여 가장 우선적으로 스케줄링 되도록한다. 즉, Unlock이 될 때까지 schedulerLock을 호출한 프로세스가 스케줄링 된다.
        - password는 학번인 2017029343이며, 잘못된 password가 입력되거나, 이미 schedulerLock을 호출한 프로세스가 있는 경우. 즉, 잘못된 schedulerLock을 호출한 프로세스는 강제적으로 종료가 된다. 종료가 될 때, 해당 프로세스의 pid, 해당 프로세스가 존재하던 큐에서 얼마만큼 CPU를 점유하여 사용했는지(TimeQuantum. 즉, LocalTicks), 해당 프로세스가 존재하는 큐의 Level을 출력한다.
        - 성공적으로 수행이 된 경우, 해당 프로세스가 RUNNABLE로 되어 큐로 들어가려는 곳 (yield(), wakeup1()…) 에서 이를 확인하여 L0 Queue의 가장 맨 앞으로 들어갈 수 있도록 한다. 즉, Unlock이 될 때까지 해당 프로세스는 L0 Queue의 맨 앞으로만 삽입된다.
        - 우선적으로 스케줄링 되도록 설정된 프로세스(schedulerLock을 호출한 프로세스)가 I/O나 이벤트 등이 발생하여 SLEEPING 상태로 들어가게 되는 경우, 기존의 MLFQ 스케줄링 형태로 진행하다, SLEEPING 상태에서 RUNNABLE 상태로 변경이 되게 되면, 해당 프로세스를 우선적으로 다시 스케줄링 하도록 한다. (CPU Utilization을 높이기 위해서)
2. **schedulerUnlock(int password)**
    - 기존의 특정 프로세스가 우선적으로 스케줄링되던 것을 해제하는 것으로, schedulerUnlock을 호출하려는 프로세스는 이전에 schedulerLock을 호출한 프로세스이어야 할 것임. 성공적으로 schedulerUnlock이 수행되었다면, 기존의 MLFQ 스케줄링 형태로 돌아가게 된다. 즉, 이제부터는 해당 특정 프로세스가 우선적으로 스케줄링 되지 않는다.
        - password는 학번인 2017029343이며, 잘못된 password가 입력되거나, schedulerLock을 호출한 프로세스가 없는 경우(특정 프로세스가 우선적으로 스케줄링 되는 상황이 아닌 경우). 즉, 잘못된 schedulerUnlock을 호출한 프로세스는 강제적으로 종료가 된다. 종료가 될 때, 해당 프로세스의 pid, 해당 프로세스가 존재하던 큐에서 얼마만큼 CPU를 점유하여 사용했는지(TimeQuantum. 즉, LocalTicks), 해당 프로세스가 존재하는 큐의 Level을 출력한다.
        - 성공적으로 수행이 된 경우, 해당 프로세스가 L0 Queue의 가장 맨 앞에 삽입되고, 앞으로는 해당 프로세스가 우선적으로 스케줄링되지 않게 된다. 즉, 기존의 MLFQ 스케줄링 형태로 돌아가게 된다.
3. **priorityBoosting()**
    - GlobalTicks가 100이 되어, priorityBoosting()이 호출될 때, schedulerLock을 호출하여 우선적으로 스케줄링 되는 프로세스가 존재한다면, 해당 프로세스를 L0 Queue의 가장 맨 앞에 삽입하여 준 다음, 앞으로 우선적으로 스케줄링되지 않고 기존의 MLFQ 스케줄링 형태로 돌아가게 설정 후, 다른 모든 프로세스를 L0 Queue로 이동 시킨다. (PriorityBoosting 진행)
    - schedulerLock을 호출한 프로세스가 I/O나 이벤트 발생으로 SLEEPING 상태로 전환되면 그 동안 다른 프로세스가 스케줄링되는데(기존의 MLFQ에 따라) 이 때, GlobalTicks이 100이 되어 priorityBoosting()이 호출되게 되면,  schedulerLock을 호출한 프로세스는 SLEEPING 상태이기 때문에 L0 Queue의 가장 맨 앞에 삽입하여 주지 않도록 설계. 단지, 기존의 MLFQ 스케줄링 형태로 완전히 돌아가게 설정만 한다.
    

## Code Implementation - Scheduler, PriorityBoosting and schedulerLock / schdulerUnlock

---

1. Queue.h : MLFQ의 주된 구성요소인 기본적인 3 Level Queue와 추가로 구현한 SleepQueue를 헤더파일로 작성하여 proc.c에서 포함시켜 주었습니다.

```c
// 총 4개의 Queue가 존재하며, 각각의 Queue는 다음과 같은 멤버 변수를 포함합니다.
struct Queue
{
	      struct proc* _head;            // Queue의 가장 맨 앞에 있는 프로세스를 가리킵니다.
        struct proc* _tail;            // Queue의 가장 맨 뒤에 있는 프로세스를 가리킵니다.
        int _level;                    // 해당 Queue의 Level을 확인할 수 있습니다.
        int _pCount;                   // 해당 Queue에 존재하는 프로세스의 개수를 확인할 수 있습니다.
        int _timeQuantum;              // Queue마다 서로 다른 TimeQuantum을 가지기 때문에 해당 Queue의 레벨에 맞는 TimeQuantum이 몇인지 확인할 수 있습니다.
};

// MLFQ 구조체를 사용하여 구조체 멤버 변수로 총 4개의 Queue와 GlobalTicks을 저장하는 변수,
// schedulerLock()을 호출한 프로세스가 있는지 확인하는 변수가 존재합니다.
struct MLFQ
{
    struct Queue _L0;
    struct Queue _L1;
    struct Queue _L2;
    struct Queue _SleepQueue;
    int _globalTicks;                   
    struct proc* _isLocked;             // schedulerLock()을 호출한 프로세스가 있을 때,
                                        // PriorityBoosting이 발생할 경우와 같은 특정 상황에 대비.                                         
};

// 내부 구현이 길기 때문에 함수 프로토타입을 WIKI에 명시하고 그에 대한 설명을 주석으로 대체합니다.
void initProc(struct proc* proc);                              // proc 구조체의 멤버 변수를 초기화하는 함수입니다.
void initQueue(struct Queue* queue, int level);                // queue 구조체의 멤버 변수를 초기화하는 함수입니다. 
int isEmpty(struct Queue* queue);                              // 인자로 전달받은 Queue가 비어있는지 확인합니다.
void enQueue(struct Queue* queue, struct proc* proc);          // 인자로 전달받은 Queue의 맨 뒤에 인자로 전달받은 프로세스를 삽입합니다.
void enL0Queue(struct Queue* queue, struct proc* proc);        // schedulerLock() 또는 schedulerUnlock()을 위한 함수로 L0 Queue의 가장 맨 앞에 프로세스를 삽입합니다.
void enL2Queue(struct Queue* queue, struct proc* proc);        // L2 Queue의 Prioirty FCFS를 위한 함수로 전달받은 프로세스의 Priority에 맞게 적절한 위치에 삽입합니다.
struct proc* deQueue(struct Queue* queue);                     // 인자로 전달받은 Queue의 가장 맨 앞에서 프로세스를 제거합니다.
struct proc* deQueueAnywhere(struct Queue* queue, struct proc* proc);  // 인자로 전달받은 Queue에서 인자로 전달받은 프로세스를 어느 위치에서든 상관없이 제거합니다.
```

1. param.h : 기존의 xv6 구동에 필요한 전처리기 지시문에 더하여 코드 가독성을 위해 다음과 같은 전처리기 지시문을 추가 하였습니다.

```c
#define NULL ((void *)0)
#define TIMEQUANTUM_0 4
#define TIMEQUANTUM_1 6
#define TIMEQUANTUM_2 8
#define TIMEQUANTUM_MLFQ 100
```

1. proc.h : xv6는 프로세스를 구조체로 관리하며, 프로세스의 대한 정보를 proc 구조체 내부의 멤버 변수에 저장합니다. 기존의 xv6에서 존재하던 멤버 변수에 더불어 구현에 필요한 변수를 추가 하였습니다.

```c
struct proc {
   
  struct proc* _next;
  struct proc* _prev;
  int _localTicks;
  int _priority;
  int _level;
  int _putLock;   // schedulerLock()을 호출한 프로세스가 자신임을 알리기 위한 변수.
  int _putUnlock; // 최근에 schedulerUnlock()을 호출한 프로세스가 자신임을 알리기 위한 변수.
                  // 두 변수는 CPU를 독점적으로 할당받거나, 기존의 MLFQ로 돌아가기 위한 변수로 해당 변수의 값에 따라
                  // L0 Queue의 가장 맨 앞에 삽입될지 말지 결정하는 중요한 변수입니다.
};
```

1. proc.c - userinit() : init 프로세스는 xv6 부팅 과정에서 가장 먼저 생성되는 프로세스로 userinit() 함수는 부팅 과정에서 한 번 호출이 됩니다. 따라서 해당 함수 내부에서 전역변수로 선언한 MLFQ 구조체의 멤버 변수를 초기화 해줍니다.

```c
void
userinit(void)
{
  // Queue.h 에서 정의한 initQueue 함수를 통해 구조체 MLFQ 변수 mlfq의 속한 멤버 변수를 모두 초기화 시켜줍니다.
  initQueue(&mlfq._L0, 0);
  initQueue(&mlfq._L1, 1);
  initQueue(&mlfq._L2, 2);
  initQueue(&mlfq._SleepQueue, -1);
  mlfq._globalTicks = 0;
  mlfq._isLocked = NULL;


  enQueue(&mlfq._L0, p);         // 최초로 생성된 init 프로세스를 L0 Queue의 가장 맨 뒤에 삽입하여 줍니다.
}
```

1. proc.c - fork() : init 프로세스를 제외한 모든 프로세스는 부모프로세스를 복제하여 생성되므로 새로운 프로세스는 fork() 함수를 통해서 생성이 됩니다. 생성된 프로세스의 proc 구조체 내부의 멤버 변수를 초기화 하는 동시에 L0 Queue의 맨 뒤로 프로세스를 삽입합니다.

```c
int
fork(void)
{
  // 새로 생성된 프로세스의 구조체 내부의 멤버 변수를 초기화 해주는 함수로 Queue.h에 정의되어 있습니다.
  initProc(np);
 
  // 새로 생성된 프로세스는 L0 Queue의 맨 뒤로 삽입 됩니다.
  enQueue(&mlfq._L0, np);
}
```

1. proc.c - exit() : 프로세스의 모든 작업이 수행한 뒤에 ZOMBIE 상태로 전환하여, 부모프로세스가 UNUSED 상태로 전환시키기 이전에 거치는 함수로 해당 함수 내부에서 schedulerLock() 을 호출한 프로세스가 종료되는 상황이라면 기존의 MLFQ로 돌아가도록 변수의 값을 변경하여 줍니다.

```c
void
exit(void)
{
  // mlfq._isLocked에는 schedulerLock()을 호출한 프로세스의 주소가 저장되어 있고,
  // _putLock은 가장 최근에 schedulerLock()을 호출한 프로세스가 자신임을 알리는 변수로 proc 구조체 내부에 선언되어 있습니다.
  // 이 두 변수의 값을 통해 현재 종료되는 프로세스가 schedulerLock()을 호출한 프로세스임을 확인하고, 
  // 그에 맞게 MLFQ로 돌아갈 수 있도록 합니다.
  if (mlfq._isLocked == curproc && curproc->_putLock == 1)
  {
        mlfq._isLocked = NULL;
        curproc->_putLock = 0;
  }
  sched();
  panic("zombie exit");
}
```

1. proc.c - priorityBoosting() : Starvation을 막기 위해 GlobalTicks값이 100이 되면, SleepQueue에 존재하는 프로세스를 제외한 모든 프로세스를 L0 Queue로 이동시키고, 명세에 맞도록 멤버 변수들을 초기화 시켜줍니다.

```c
void 
priorityBoosting(void)
{
  struct proc* p;
  mlfq._globalTicks = 0;

  // 모든 프로세스를 L0 Queue로 이동시키기 전에, schedulerLock()을 호출한 프로세스가 존재한다면
  // MLFQ 방식으로 스케줄링되도록 변수를 변경하고, 해당프로세스를 L0 Queue의 가장 맨 앞에 삽입시킵니다.
  if (mlfq._isLocked != NULL && mlfq._isLocked->state == RUNNABLE)
  {
     deQueueAnywhere(&mlfq._L0, mlfq._isLocked);
     enL0Queue(&mlfq._L0, mlfq._isLocked);
     mlfq._isLocked->_putLock = 0;
     mlfq._isLocked = NULL;
  }
  
  // schedulerLock()을 호출한 프로세스가 I/O나 이벤트 발생으로 SLEEPING 상태에 있는 경우,
  // 기존의 MLFQ 방식으로 스케줄링 되는데, 그 도중에 PriorityBoosting이 발생한 경우로
  // 해당 프로세스는 가장 맨 앞에 삽입되도록하지 않고, 단순히 기존의 MLFQ 방식으로 스케줄링 되도록
  // 변수의 값을 변경하여 줍니다.
  else if (mlfq._isLocked != NULL && mlfq._isLocked->state == SLEEPING)
  {
     mlfq._isLocked->_putLock = 0;
     mlfq._isLocked = NULL;
  }
  
  
  // 총 3 Level Queue로 PriorityBoosting 당시 각 Queue의 상태를 확인하여 head와 tail을 연결시킴으로
  // 각 Queue의 모든 프로세스들을 연결하여 L0 Queue로 이동 시킵니다.
  // 특정 Level Queue가 비어있는 경우, NULL 값을 참조하여 오류가 발생할 수 있으므로 분기를 나누어 
  // 이를 대처하였습니다.

  // L0 Queue만 비어있는 경우 
  if(isEmpty(&mlfq._L0) != 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._head = mlfq._L1._head;
    mlfq._L1._tail->_next = mlfq._L2._head;
    mlfq._L2._head->_prev = mlfq._L1._tail;
    mlfq._L0._tail = mlfq._L2._tail;
  }

  // L1 Queue만 비어있는 경우
  else if(isEmpty(&mlfq._L0) == 0 && isEmpty(&mlfq._L1) != 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._tail->_next = mlfq._L2._head;
    mlfq._L2._head->_prev = mlfq._L0._tail;
    mlfq._L0._tail = mlfq._L2._tail;
  }

  // L2 Queue만 비어있는 경우
  else if(isEmpty(&mlfq._L0) == 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) != 0)
  {
    mlfq._L0._tail->_next = mlfq._L1._head;
    mlfq._L1._head->_prev = mlfq._L0._tail;
    mlfq._L0._tail = mlfq._L1._tail;
  }
  // L0 Queue와 L1 Queue가 비어있고, L2 Queue에는 프로세스가 존재하는 경우.
  else if(isEmpty(&mlfq._L0) != 0 && isEmpty(&mlfq._L1) != 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._head = mlfq._L2._head;
    mlfq._L0._tail = mlfq._L2._tail;
  }

  // L0 Queue와 L2 Queue가 비어있고, L1 Queue에는 프로세스가 존재하는 경우.
  else if(isEmpty(&mlfq._L0) != 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) != 0)
  {
    mlfq._L0._head = mlfq._L1._head;
    mlfq._L0._tail = mlfq._L1._tail;
  }

  // 3 Level Queue가 모두 비어있지 않는 경우. 
  else if(isEmpty(&mlfq._L0) == 0 && isEmpty(&mlfq._L1) == 0 && isEmpty(&mlfq._L2) == 0)
  {
    mlfq._L0._tail->_next = mlfq._L1._head;
    mlfq._L1._head->_prev = mlfq._L0._tail;
    mlfq._L1._tail->_next = mlfq._L2._head;
    mlfq._L2._head->_prev = mlfq._L1._tail;
    mlfq._L0._tail = mlfq._L2._tail;
  }
  
  mlfq._L0._pCount = mlfq._L0._pCount + mlfq._L1._pCount + mlfq._L2._pCount; 

  // L0 Queue로 모든 프로세스가 이동된 이후
  // 각 프로세스의 Level, LocalTicks, Priority 등을 초기화 해줍니다.
  for(p = mlfq._L0._head; p != NULL; p = p->_next)
     initProc(p);

  // L1 Queue와 L2 Queue에는 더이상 프로세스가 존재하지 않으므로 초기화 해줍니다.
  initQueue(&mlfq._L1, 1);
  initQueue(&mlfq._L2, 2);

  // SleepQueue에 존재하는 프로세스 또한 RUNNABLE 상태로 전환된 이후 L0 Queue로 돌아가도록 멤버 변수를 초기화 해줍니다.
  for(p = mlfq._SleepQueue._head; p != NULL; p = p->_next)
     initProc(p);
}
```

1. proc.c - scheduler() : CPU 자원을 할당받을 프로세스를 결정하는 함수. 각 Queue의 상태를 확인하여 스케줄링될 프로세스를 선택합니다. 프로세스의 작업을 수행 후 다시 scheduler() 함수로 돌아와 다음으로 스케줄링 될 프로세스를 선택하기 전에 GlobalTicks 값을 확인하여 100이 되었으면 PriorityBoosting을 진행합니다.

```c
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

      // 각 Queue의 상태를 확인하고, Queue 내부에 스케줄링 될 프로세스가 존재한다면 Queue의 가장 맨 앞에서 프로세스를 
      // 뺴내고, 스케줄링 되도록합니다.
      if(mlfq._L0._pCount != 0)
        p = deQueue(&mlfq._L0);
      else if(mlfq._L1._pCount != 0)
        p = deQueue(&mlfq._L1);
      else if(mlfq._L2._pCount != 0)
        p = deQueue(&mlfq._L2);
      else                      
      {
        // 모든 프로세스가 SLEEPING 상태에 있거나, 스케줄링될 프로세스가 모든 Queue에 존재하지 않는 경우,
        // 스케줄링될 프로세스가 나타날 때까지 반복문을 돕니다.
        release(&ptable.lock);
        continue; 
      }
     
      // 프로세스 스케줄링 이후, 다음 프로세스를 스케줄링하기 전에 수행되는 곳으로,
      // 해당 위치에서 GlobalTicks을 확인하여 100에 도달하였을 경우 priorityBoosting() 함수를 호출하여
      // PriorityBoosting을 진행합니다.
      if(mlfq._globalTicks == TIMEQUANTUM_MLFQ)
       priorityBoosting();
    
  }
}
```

1. proc.c - yield() : CPU자원을 할당받아 수행중이던 프로세스가 Timer 인터럽트에 의해 CPU 자원을 다른 프로세스에게 넘겨주는 과정에서 호출되는 함수로 해당 위치에서 프로세스의 LocalTicks과 GlobalTicks을 변화시키고, 변화된 값에 따라 프로세스가 돌아갈 Queue를 결정합니다.

```c
void
yield(void)
  {
  acquire(&ptable.lock);  //DOC: yieldlock
  struct proc* p = myproc();
  myproc()->state = RUNNABLE;
 
  // 프로세스의 LocalTicks과 GlobalTicks 값을 증가시켜줍니다.
  p->_localTicks++;
  mlfq._globalTicks++;

  // 만약 schedulerLock()을 호출한 프로세스가 CPU 자원을 다른 프로세스에게 넘겨주지 않도록
  // L0 Queue의 가장 맨 앞에 삽입하여 줍니다.
  // 더하여, schedulerUnlock()을 호출한 프로세스 또한 마지막으로 L0 Queue의 가장 맨 앞으로 보내주도록 합니다.
  if((mlfq._isLocked != NULL && p->_putLock) || p->_putUnlock)
  {
    if(p->_putUnlock) 
      p->_putUnlock = 0;
    p->_level = 0;
    enL0Queue(&mlfq._L0, p);
  }

  // 변화된 LocalTicks값을 확인하여 그에 맞도록 Queue를 이동시켜 해당 큐에 삽입시켜 줍니다.
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
    enL2Queue(&mlfq._L2, p); // L2 Queue의 경우 Priority FCFS에 맞도록 Priority 값을 확인하여
                             // L2 Queue의 알맞은 위치에 삽입되도록 합니다.
  }

  sched();
  release(&ptable.lock);
}
```

1. proc.c - sleep(* chan, struct spinlock *lk) : CPU자원을 할당받아 수행중이던 프로세스가 I/O나 이벤트의 발생으로 SLEEPING 상태로 전환되는 함수로, 해당 프로세스들을 yield()가 아닌 wakeup1() 함수를 통해 본래 Queue로 돌아가게 되는데, CPU자원을 잠시라도 할당받았던 프로세스라면 LocalTicks값과 GlobalTicks을 증가시켜주어야 하기 때문에 해당 함수 내부에서 값을 변경시켜 준다. 

```c
void
sleep(void *chan, struct spinlock *lk)
{

  mlfq._globalTicks++;
  p->_localTicks++; 

  // wakeup1() 함수 내부에서 증가된 LocalTicks에 맞게 Queue에 들어가도록 변수의 값을 변경하여준다.
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
  // I/O나 이벤트 발생으로 SLEEPING 상태로 전환된 프로세스는 SleepQueue로 들어가게 된다.
  // 이후, I/O나 이벤트가 끝나게 되면 SleepQueue에서 빠져나와 알맞은 Queue로 들어가게 된다.
  enQueue(&mlfq._SleepQueue, p);
  sched();

}
```

1. proc.c - wakeup1(void* chan) : I/O나 이벤트 발생으로 SleepQueue에 있던 프로세스를 증가된 LocalTicks을 기반으로하여 알맞은 Queue로 돌아가게 하는 함수.
    - 기존의 xv6는 channel에 맞는 SLEEPING 상태의 프로세스를 찾기 위해 프로세스 테이블을 모두 순회하기 때문에 비효율적이었습니다. 해당 코드를 SleepQueue만 순회하도록 하여 기존의 xv6를 최적화를 진행할 수 있었습니다.

```c
static void
wakeup1(void *chan)
{
  struct proc *p;

  // SleepQueue를 돌면서 RUNNABLE 상태로 전환될 프로세스를 찾은 뒤, 
  // schedulerLock()을 호출한 프로세스나, schedulerUnlock()을 호출한 뒤 SLEEPING 상태가 된 프로세스는 
  // L0 Queue의 가장 맨 앞에 삽입되도록하고, 그외의 프로세스들은 sleep() 함수에서 증가된 LocalTicks를 기준으로
  // 알맞은 Queue로 삽입되도록한다.
  for(p = mlfq._SleepQueue._head; p != NULL; p = p->_next)
  {
      if(p->state == SLEEPING && p->chan == chan)
      {  
        struct proc* temp = p->_prev;
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
       p = temp;
     }
   }
}
```

1. proc.c - kill(int pid) : kill System Call로 인자로 받은 pid에 해당하는 프로세스를 강제로 종료시키는 함수, 해당 프로세스의 killed 플래그를 1로 세팅하고, 프로세스의 상태를 확인하여 알맞은 Queue로 들어가도록 합니다. 이후, 해당 프로세스는 스케줄링되면 활성화된 killed 플래그로 인해 종료됩니다. 코드 구현은 wakeup1()과 동일하므로 생략합니다.

1. proc.c - setPriority(int pid, int priority) : 인자로 받은 pid에 해당하는 프로세스의 Priority를 인자로 받은 프로세스의 priority로 설정합니다. 해당 프로세스가 L0 Queue 혹은 L1 Queue에 존재하는 프로세스라면 Priority는 스케줄링에 아무런 영향을 주지 않으므로 단순히 값만 바꾸어 주도록 합니다. 반면, L2 Queue에 존재하는 프로세스라면 Priority는 스케줄링에 영향을 미치므로 프로세스가 L2 Queue에서 Priority에 의한 순서가 맞도록 재설정하기 위해 빼낸 이후, 다시 삽입하여 주도록합니다.

```c
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
        
        // L2 Queue에 존재하는 프로세스.
        // pid가 현재 수행중인 프로세스와 동일하다면, yield()에서 본래 Queue로 돌아갈 때, 순서가 재정립되어 들어가므로
        // 따로 재정립이 필요없다.
        // 하지만 pid 현재 수행중인 프로세스가 아닌, 다른 프로세스라면 해당 프로세스는 변경된 priority 값에 의한
        // 순서 재정립이 필요하므로 L2 Queue에서 뺴낸 이후, 다시 삽입하여 준다.
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
```

1. proc.c - schedulerLock(int password), schedulerUnlock(int password) : schedulerLock(), schedulerUnlock() System Call 구현을 위한 함수로, 잘못된 password, 중복된 호출 등. 잘못된 호출이 되면, 명세에 나온대로 출력한 뒤 종료하게 구현하였습니다.

```c
// 잘못된 호출은 pid, time quantum, level을 출력하고 종료되도록 합니다.
// 올바르게 수행이 되면, 해당 프로세스가 우선적으로 스케줄링 되도록 관련 변수의 값을 설정하여 줍니다.
void
schedulerLock(int password)
{
   struct proc* p = myproc();

   if(password == -1 || password != 2017029343 || mlfq._isLocked != NULL)
   {
     cprintf("pid : %d, time quantum : %d, level : %d\n", p->pid, p->_localTicks, p->_level);
     exit();
   }
   mlfq._isLocked = p;         
   mlfq._globalTicks = 0;       
   p->_putLock = 1;            
}

// 잘못된 호출은 pid, time quantum, level을 출력하고 종료되도록 합니다.
// 올바르게 수행이 되면, 기존의 MLFQ 스케줄링 방식으로 되돌아가도록 변수의 값을 설정하여 줍니다.
// 추가로, 성공적으로 호출한 프로세스는 마지막으로 L0 Queue의 가장 맨 앞으로 들어가도록 _putUnlock 변수를 1로 설정하여 줍니다.
void
schedulerUnlock(int password)
{
   struct proc* p = myproc();
   
   if(password == -1 || password != 2017029343 || mlfq._isLocked == NULL)
   {
     cprintf("pid : %d, time quantum : %d, level : %d\n", p->pid, p->_localTicks, p->_level);
     exit();
   }

   mlfq._isLocked = NULL;
   p->_localTicks = 0;
   p->_priority = 3;
   p->_putUnlock = 1;
}
```

## Code Implementation - Add System Call

---

1. syscall.h : 명세에 주어진 System Call 추가를 위한 System Call number를 추가하여 주었습니다.

```c
#define SYS_yield 23
#define SYS_getLevel 24
#define SYS_setPriority 25
#define SYS_schedulerLock 26
#define SYS_schedulerUnlock 27
```

1. user.h

```c
void yield(void);
int getLevel(void);
void setPriority(int pid, int priority);
void schedulerLock(int password);
void schedulerUnlock(int password);

```

1. defs.h : 유저프로그램에서 정의한 System Call들을 호출할 수 있도록 함수의 프로토타입을 선언하여 줍니다.

```c

void            yield(void);
int             getLevel(void);
int             setPriority(int, int);
void            schedulerLock(int password);
void            schedulerUnlock(int password);

```

1. usys.S

```c
SYSCALL(yield)
SYSCALL(getLevel)
SYSCALL(setPriority)
SYSCALL(schedulerLock)
SYSCALL(schedulerUnlock)

```

1. syscall.c

```c
extern int sys_yield(void);
extern int sys_getLevel(void);
extern int sys_setPriority(void);
extern int sys_schedulerLock(void);
extern int sys_schedulerUnlock(void);


[SYS_yield]   sys_yield,
[SYS_getLevel] sys_getLevel,
[SYS_setPriority] sys_setPriority,
[SYS_schedulerLock] sys_schedulerLock,
[SYS_schedulerUnlock] sys_schedulerUnlock,

```

1. sysproc.c 

```c
void
sys_yield(void)
{
   yield();
}

int
sys_getLevel(void)
{
   return myproc()->_level;
}

// 만약 priority 값이 0, 1, 2, 3이 아닌 다른 값인 경우 아무런 변화 없이 종료되도록 합니다.
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

// 아무런 인자를 받지 못한경우 proc.c에 정의되어 있는 schedulerLock() 함수에 -1을 인자로 전달하여
// 잘못된 password로 해당 프로세스는 종료되도록 합니다.
void
sys_schedulerLock(void)
{
   int password;
   if (argint(0, &password) < 0)
      schedulerLock(-1);
   schedulerLock(password);
}

// 아무런 인자를 받지 못한경우 proc.c에 정의되어 있는 schedulerUnlock() 함수에 -1을 인자로 전달하여
// 잘못된 password로 해당 프로세스는 종료되도록 합니다.
void
sys_schedulerUnlock(void)
{
    int password;
    if (argint(0, &password) < 0)
       schedulerUnlock(-1);
    schedulerUnlock(password);
}
```

## Code Implementation - Add Interrupt (schedulerLock, schedulerUnlock)

---

**schedulerLock과 schedulerUnlock은 각각 129, 130번 Interrupt로 호출될 수 있도록 다음과 같이 구현하였습니다.**

1. traps.h


```c
#define T_SCHEDULERLOCK   129
#define T_SCHEDULERUNLOCK 130

```

1. trap.c - tvinit() : xv6 부팅 이후 인터럽트 테이블을 초기화 하는 과정에서 schedulerLock과 schedulerUnlock이 유저프로그램에서도 호출이 될 수 있도록 dpl 값을 DPL_USER로 설정하여 주었습니다.

```c
// xv6 initializes, idt after booting
void
tvinit(void)
{
  SETGATE(idt[T_SCHEDULERLOCK], 0, SEG_KCODE<<3, vectors[T_SCHEDULERLOCK], DPL_USER);
  SETGATE(idt[T_SCHEDULERUNLOCK], 0, SEG_KCODE<<3, vectors[T_SCHEDULERUNLOCK], DPL_USER);
  initlock(&tickslock, "time");
}
```

1. trap.c - trap(struct trapframe* tf) : trapframe에 저장되어 있는 trap number를 확인하여 switch 분기문을 통해 값에 해당하는 인터럽트 핸들러를 수행하도록 하였습니다. 

```c
void
trap(struct trapframe *tf)
{

  // Interrupt를 통해 호출시 학번이 자동으로 schedulerLock과 schedulerUnlock의 인자로 전달되며
  // 우선적으로 스케줄링 되는 프로세스가 있는지 여부. 즉, schedulerLock()을 호출한 프로세스가 있는지 여부에 따라 
  // 성공, 실패 여부가 결정됩니다.
  case T_SCHEDULERLOCK:
    schedulerLock(2017029343);
    break;
  case T_SCHEDULERUNLOCK:
    schedulerUnlock(2017029343);
    break;

}
```

## Result of Implementation

---

**test_mlfq라는 User program을 만들어 위에서 구현한 MLFQ 기능을 Test합니다.**

1. 부모프로세스에서 자식프로세스를 fork()하기 전에 schedulerLock()을 호출하여 부모프로세스만 독점적으로 CPU 자원을 할당 받도록 한 뒤, 작동 과정을 살펴봅니다.

![test6](https://github.com/study-kim7507/HYU/assets/63442832/e52eed38-8899-44bd-bfc3-95e2823619af)

![test7](https://github.com/study-kim7507/HYU/assets/63442832/8c9d4ed7-6add-47cd-9190-baaa38dd97af)

![test8](https://github.com/study-kim7507/HYU/assets/63442832/f1183845-d848-4b1d-baec-e0518fdfefea)


- Result : 부모프로세스가 fork()하기 전 schedulerLock()을 호출하여 fork() 이후 부모프로세스만 수행되는 것을 볼 수 있으며, 이후 PriorityBoosting으로 인하여 기존의 MLFQ 방식으로 돌아가 스케줄링 되는 것 또한 확인이 가능합니다.  마지막으로 부모프로세스와 자식프로세스가 모두 L2 Queue로 이동. Priority FCFS 방식의 스케줄링이 되어 L2 Queue에서 먼저 스케줄링된 부모프로세스의 우선순위가 더 높아져 부모프로세스만 스케줄링됨을 확인할 수 있었습니다.

1. 부모프로세스에서 자식프로세스의 fork() 이후, 자식프로세스에서 schedulerLock()을 호출하여 자식프로세스만 독점적으로 CPU 자원을 할당 받도록 한 뒤, 작동 과정을 살펴봅니다.
    
   ![test9](https://github.com/study-kim7507/HYU/assets/63442832/1453f86c-1e48-4447-a0af-456c3f3da025)

    
    - Result : fork() 이전에는 부모프로세스만 수행되다가 fork() 이후 자식프로세스가 생성되고, 자식프로세스에서 schedulerLock()을 호출함으로서 독점적으로 CPU 자원을 할당받아 자식프로세스만 수행이 됨을 확인할 수 있었습니다. 이후의 과정은 첫번째 테스트의 경우와 동일합니다.

1. Piazza에 주어진 테스트 케이스
    
    ![test10](https://github.com/study-kim7507/HYU/assets/63442832/2bb8901b-506b-4673-b7ad-07d9af755b40)

    - Result : 각 Queue의 TimeQuantum에 의해서 각 Queue에서 스케줄링된 횟수의 차이가 존재함을 확인 할 수 있었습니다.
    
    |  | Process 5 | Process 6 | Process 7 | Process 4 |
    | --- | --- | --- | --- | --- |
    | L0 Queue | 9775 | 10635 | 15926 | 20732 |
    | L1 Queue | 16013 | 19490 | 26979 | 26071 |
    | L2 Queue | 74212 | 69875 | 57095 | 53197 |
    | Sum | 100000 | 100000 | 100000 | 100000 |
2. usertests : xv6에 내장되어 있어 있으며 운영체제의 핵심 기능을 테스트하고 검증하는데 사용되는 유저프로그램입니다.

![****************************************************************기존의 xv6 usertests****************************************************************]
![test11](https://github.com/study-kim7507/HYU/assets/63442832/5d6d5167-0a77-4dfb-b3e9-cb8236902b1b)


****************************************************************기존의 xv6 usertests****************************************************************

![********************************구현한 xv6 usertests********************************]
![test12](https://github.com/study-kim7507/HYU/assets/63442832/0b42e61c-655d-4ac1-8685-3c41ab113e1c)


********************************구현한 xv6 usertests********************************

- Result : 정상적으로 작동함을 확인할 수 있었습니다. 추가로, 기존의 usertests를 수정하여 소요된 시간을 확인하도록 하였는데, PriorityBoosting이라는 오버헤드가 생긴 상황에서 wakeup1() 에서와 같이 모든 프로세스를 확인하지 않도록 최적화 하여 조금 더 성능이 향상됨을 확인할 수 있었습니다.

## What i have to do (Trouble shooting)

---

1. 구현한 MLFQ에서 PriorityBoosting을 진행하기 위해 여러 분기를 나누어 구현하였는데, 이는 코드 가독성 뿐만 아니라 효율이 떨어질 것이라고 생각이 듭니다. 이를 효율적으로 해결할 수 있는 방안을 생각하여 수정하면 좋을 것 같습니다.
