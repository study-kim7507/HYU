#include "types.h"
#include "stat.h"
#include "user.h"

#define MAXARGS 10

int getcmd(char*, int);
void parse(char*, char*, char*, char*);
void runcmd(char*, char*, char*);

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