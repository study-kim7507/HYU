#include "types.h"
#include "defs.h"

int myFunction(char* str)
{
	cprintf("%s\n", str);
	return 0xABCD;
}

int sys_myFunction(void)
{
	char* str;
	if (argstr(0, &str) < 0)
		return -1;
	return myFunction(str);
}
