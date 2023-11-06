/*
#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000
#define NUM_YIELD 20000
#define NUM_SLEEP 1000

#define NUM_THREAD 4
#define MAX_LEVEL 5

int parent;

int fork_children()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
    if ((p = fork()) == 0)
    {
      sleep(10);
      return getpid();
    }
  return parent;
}


int fork_children2()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
  {
    if ((p = fork()) == 0)
    {
      sleep(300);
      return getpid();
    }
    else
    {
      setPriority(p, i);
    }
  }
  return parent;
}

int max_level;

int fork_children3()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
  {
    if ((p = fork()) == 0)
    {
      sleep(10);
      max_level = i;
      return getpid();
    }
  }
  return parent;
}
void exit_children()
{
  if (getpid() != parent)
    exit();
  while (wait() != -1);
}

int main(int argc, char *argv[])
{
  int i, pid;
  int count[MAX_LEVEL] = {0};
  
  schedulerLock(2017029343);
  parent = getpid();

  printf(1, "MLFQ test start\n");

  printf(1, "[Test 1] default\n");
  pid = fork_children2();
  
  if (pid != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      if (x < 0 || x > 4)
      {
        printf(1, "Wrong level: %d\n", x);
        exit();
      }
      count[x]++;
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL; i++)
      printf(1, "L%d: %d\n", i, count[i]);
  }
  exit_children();
  printf(1, "[Test 1] finished\n");
  printf(1, "done\n");
  exit();
}

*/

#include "types.h"
#include "stat.h"
#include "user.h"
int main(int argc, char* argv[])
{
	printf(1, "TEST PROGRAM\n");
	printf(1, "This Program does not end\n");
	printf(1, "Test program to check if processes are inserted into L2 queue in order.\n");
        printf(1, "To run the test, priorityBoosting must be turned off.\n");
	int pid = 0;
	
	char* buff[1001];
	pid = fork();

	printf(1, "%s\n", buff);	
	if (pid == 0)
	{
		pid = fork();
                if (pid == 0) {
			while(1)
			{
				printf(1, "Child %d\n", getLevel());
			}
 		}
 	
		else {
			while(1)
			{
				printf(1, "Parent %d\n", getLevel());
			}
		}
	}
	else {	
		while(1)
		{
			printf(1, "Parent %d\n", getLevel());
		}
		wait();
	}
	exit();
}


