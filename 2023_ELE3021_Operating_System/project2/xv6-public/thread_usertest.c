#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_THREAD 10
int test = 10;

void*
routine1(void* arg)
{
  int tid = (int) arg;
  int i;
  
  for (i = 0; i < 50000000; i++){
    if (i % 10000000 == 0){
      printf(1, "%d ", tid);
    }
  }
 thread_exit((void *)(tid+1));
 exit(); 
}

void
test1(void* arg){
  thread_t threads[NUM_THREAD];
  int i;
  void *retval;

  if(*(int*)arg == 1)
    thread_exit((void*)0);

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_create(&threads[i], routine1, (void*)i) != 0){
      printf(1, "Error thread_create\n");
      return;
    }
  }

  for (i = 0; i < NUM_THREAD; i++){
    if (thread_join(threads[i], &retval) != 0){
      printf(1, "panic at thread_join\n");
      return;
    }

    if ((int)retval != i+1){
      printf(1, "panic at thread_join (wrong retval)\n");
      printf(1, "Expected: %d, Real: %d\n", i+1, (int)retval);
      return;
    }
  }
  if(*(int*)arg == 2)
    while(test-->0) { }
  printf(1, "Test %d is done!\n", *(int*)arg);
  thread_exit((void*)0);
}


int
main(int argc, char *argv[])
{
  thread_t thread1;
  thread_t thread2;
  int a = 1;
  int b = 2;
  void* retVal = 0;
  
  printf(1, "===========Test1===========\n");
  thread_create(&thread1, (void*)test1, (void*)&a);
  thread_create(&thread2, (void*)test1, (void*)&b);
  thread_join(thread1, (void**)retVal);
  thread_join(thread2, (void**)retVal);
  
  printf(1, "All tests are done\n");
  exit();
}  