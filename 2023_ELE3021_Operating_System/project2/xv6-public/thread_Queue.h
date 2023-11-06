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

int 
isSubThreadQueueEmpty(struct threadQueue* queue){
    if(queue->head == NULL)
        return 1;
    return 0;
}

int
isAllThreadQueueEmpty(struct threadQueue* queue){
    if(queue->head == NULL)
        return 1;
    return 0; 
}

void 
enSubThreadQueue(struct threadQueue* subThreadQueue, struct proc* proc){
    if(isSubThreadQueueEmpty(subThreadQueue)){
        subThreadQueue->head = proc;
        subThreadQueue->tail = proc;
        subThreadQueue->subThreadNum++;
        return;
    }
    subThreadQueue->tail->threadInfo.subThreadNext = proc;
    subThreadQueue->tail = proc;
    subThreadQueue->subThreadNum++;
    return;
}

struct proc* 
deSubThreadQueue(struct threadQueue* subThreadQueue, struct proc* proc){
    struct proc *p;
    struct proc *temp;

    if(isSubThreadQueueEmpty(subThreadQueue))
        return NULL;

    for(p = subThreadQueue->head; p != NULL; p = p->threadInfo.subThreadNext){
        if(p == subThreadQueue->head && subThreadQueue->head == proc){
            if(proc == subThreadQueue->head && proc == subThreadQueue->tail){
                temp = p;
                subThreadQueue->head = subThreadQueue->tail = NULL;
                subThreadQueue->subThreadNum--;
                return temp;
            }
            temp = p;
            subThreadQueue->head = temp->threadInfo.subThreadNext;
            temp->threadInfo.subThreadNext = NULL;
            subThreadQueue->subThreadNum--;
            return temp;
        }
        else if(p->threadInfo.subThreadNext == subThreadQueue->tail && subThreadQueue->tail == proc){
            temp = p->threadInfo.subThreadNext;
            subThreadQueue->tail = p;
            p->threadInfo.subThreadNext = NULL;
            subThreadQueue->subThreadNum--;
            return temp;
        }
        else if(p->threadInfo.subThreadNext == proc){
            temp = p->threadInfo.subThreadNext;
            p->threadInfo.subThreadNext = temp->threadInfo.subThreadNext;
            temp->threadInfo.subThreadNext = NULL;
            subThreadQueue->subThreadNum--;
            return temp;
        }
    }   
    return NULL;
}

void 
enAllThreadQueue(struct threadQueue* allThreadQueue, struct proc* proc){
    if(isAllThreadQueueEmpty(allThreadQueue)){
        allThreadQueue->head = proc;
        allThreadQueue->tail = proc;
        return;
    }
    allThreadQueue->tail->threadInfo.allThreadNext = proc;
    allThreadQueue->tail = proc;
    return;
}

struct proc* 
deAllThreadQueue(struct threadQueue* allThreadQueue, struct proc* proc){
    struct proc *p;
    struct proc *temp;

    if(isAllThreadQueueEmpty(allThreadQueue))
        return NULL;

    for(p = allThreadQueue->head; p != NULL; p = p->threadInfo.allThreadNext){
        if(p == allThreadQueue->head && allThreadQueue->head == proc){
            if(proc == allThreadQueue->head && proc == allThreadQueue->tail){
                temp = p;
                allThreadQueue->head = allThreadQueue->tail = NULL;
                
                return temp;
            }
            temp = p;
            allThreadQueue->head = temp->threadInfo.allThreadNext;
            temp->threadInfo.allThreadNext = NULL;
            
            return temp;
        }
        else if(p->threadInfo.allThreadNext == allThreadQueue->tail && allThreadQueue->tail == proc){
            temp = p->threadInfo.allThreadNext;
            allThreadQueue->tail = p;
            p->threadInfo.allThreadNext = NULL;
            
            return temp;
        }
        else if(p->threadInfo.allThreadNext == proc){
            temp = p->threadInfo.allThreadNext;
            p->threadInfo.allThreadNext = temp->threadInfo.allThreadNext;
            temp->threadInfo.allThreadNext = NULL;
            
            return temp;
        }
    }   
    return NULL;
}

void
printSubThreadQueue(struct threadQueue* subThreadQueue){
    struct proc *p;
    for(p = subThreadQueue->head; p != NULL; p = p->threadInfo.subThreadNext){
        cprintf("%s %d %d %d %d\n", p->name, p->pid, p->threadInfo.tid, p->state, p->killed);
    }
}

void
printAllThreadQueue(struct threadQueue* allThreadQueue){
    struct proc *p;
    for(p = allThreadQueue->head; p != NULL; p = p->threadInfo.allThreadNext){
        cprintf("%s %d %d %d %d\n", p->name, p->pid, p->threadInfo.tid, p->state, p->killed);
    }
}