#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define BSIZE 512

#define MAGIC ('T')

int main()
{
	int n, fd;
	char *buf;
	char *fname = "TEST";
	char magic = MAGIC;
	if((fd = open(fname, O_CREATE|O_RDWR)) < 0){
		printf(1, "error : open %s failed.\n", fname);
		exit();
	}

	n = sizeof(char) * 10000000;
	// n = sizeof(char) * 5;
	if((buf = malloc(sizeof(char) * n)) < 0){
		printf(1, "error: malloc failed.\n");
		exit();
	}
	memset(buf, magic, sizeof(char) * n);
	
	if(write(fd, buf, n) != n){
		printf(1, "error: write failed.\n");
		exit();
	}

	close(fd);	
	exit();
}	
