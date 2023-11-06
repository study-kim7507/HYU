struct Queue
{
	struct proc* _head;
	struct proc* _tail;
	int _level;
	int _pCount;
	int _timeQuantum;
};

struct MLFQ
{
    struct Queue _L0;
    struct Queue _L1;
    struct Queue _L2;
    struct Queue _SleepQueue;
    int _globalTicks; 			// Add Comment : For Priority Boosting
    struct proc* _isLocked; 		// Add Comment : This variable checks whether there is a process that has called "schedulerLock". If there is, it contains the address value of that process.
};

void initProc(struct proc* proc);
void initQueue(struct Queue* queue, int level);
int isEmpty(struct Queue* queue);
void enQueue(struct Queue* queue, struct proc* proc);
void enL0Queue(struct Queue* queue, struct proc* proc); // Add Comment : Inserting a process at the front of the L0 queue.
void enL2Queue(struct Queue* queue, struct proc* proc); // Add Comment : Inserting a process at the correct position for L2 queue's Priority FCFS.
struct proc* deQueue(struct Queue* queue);
struct proc* deQueueAnywhere(struct Queue* queue, struct proc* proc);


void initProc(struct proc* proc)
{
	proc->_localTicks = 0;
	proc->_priority = 3;
	proc->_level = 0;
	proc->_putLock = 0;
	proc->_putUnlock = 0;
}

// Add Comment : Initialize Queue
void initQueue(struct Queue* queue, int level)
{
	queue->_head = queue->_tail = NULL;
	queue->_level = level;
	queue->_pCount = 0;
	if (level == 0) queue->_timeQuantum = TIMEQUANTUM_0;
	else if (level == 1) queue->_timeQuantum = TIMEQUANTUM_1;
	else if (level == 2) queue->_timeQuantum = TIMEQUANTUM_2;
}

// Add Comment : Checking if the queue is empty.
int isEmpty(struct Queue* queue)
{
	return queue->_pCount == 0;
}

// Add Comment : Inserting a process at the back of the queue.
void enQueue(struct Queue* queue, struct proc* proc)
{
	if(isEmpty(queue))
	{
		queue->_head = proc;
		proc->_prev = queue->_head;
		proc->_next = NULL;
		queue->_tail = proc;
	}
	else
	{
		proc->_prev = queue->_tail;
		queue->_tail->_next = proc;
		queue->_tail = proc;
		proc->_next = NULL;
	}
	queue->_pCount++;
	return;
}

// Add Comment : Inserting a process at the front of the L0 queue.
void enL0Queue(struct Queue* queue, struct proc* proc)
{
        // Add Comment : If the queue is empty
	if (isEmpty(queue))
        {
		queue->_head = proc;
		proc->_prev = queue->_head;
		proc->_next = NULL;
		queue->_tail = proc;
		
		queue->_pCount++;
		return;
	}

	// Add Comment : If the queue is not empty
	queue->_head->_prev = proc;
        proc->_next = queue->_head;
        queue->_head = proc;
        proc->_prev = NULL;

	queue->_pCount++;
}

// Add Comment : Inserting a process into the L2 queue according to its priority.
void enL2Queue(struct Queue* queue, struct proc* proc)
{
	struct proc* p;

	// Add Comment : If the queue is empty
	if (isEmpty(queue))
	{
		queue->_head = proc;
		proc->_prev = queue->_head;
		proc->_next = NULL;
		queue->_tail = proc;

		queue->_pCount++;
		return;
	}

	// Add Comment : If the queue is not empty
	for (p = queue->_head; p != NULL; p = p->_next)
	{
		if (proc->_priority < p->_priority)
		{
			if (p == queue->_head)
			{
				queue->_head = proc;
				proc->_next = p;
				p->_prev = proc;
				break;
			}
			p->_prev->_next = proc;
			proc->_prev = p->_prev;
			proc->_next = p;
			p->_prev = proc;
			break;
		}
	}

	// Add Comment : When inserting at the back
	if (p == NULL)
	{
		proc->_prev = queue->_tail;
		queue->_tail->_next = proc;
		queue->_tail = proc;
		proc->_next = NULL;
	}

	queue->_pCount++;
	return;
}


// Add Comment : A function that takes a process out of the queue, removing the process at the front of the queue.
struct proc* deQueue(struct Queue* queue)
{
    if (isEmpty(queue))
        return NULL;
   
    struct proc* retProc;

    retProc = queue->_head;
    queue->_head = retProc->_next;
    queue->_pCount--;

    if (isEmpty(queue))
	queue->_head = queue->_tail = NULL;
    return retProc;
}

// A function that is implemented to remove a process from the queue at any position.
struct proc* deQueueAnywhere(struct Queue* queue, struct proc* proc)
{
    struct proc* retProc;
    
    if (isEmpty(queue))
	return NULL;

    for (retProc = queue->_head; retProc != NULL; retProc = retProc->_next)
    {
        if (retProc->pid == proc->pid && retProc == queue->_head)
        {
	   queue->_head = retProc->_next;
	   
	   break;
	}
        else if (retProc->pid == proc->pid  && retProc->_next != NULL)
        {
           retProc->_next->_prev = retProc->_prev;
           retProc->_prev->_next = retProc->_next;
            
	   break;
        }
        else if (retProc->pid == proc->pid && retProc->_next == NULL)
        {
           retProc->_prev->_next = NULL;
	   queue->_tail = retProc->_prev;
          
           break;
        }
    }
    
    if (isEmpty(queue))
	queue->_head = queue->_tail = NULL;

    queue->_pCount--;
    return retProc;
}

