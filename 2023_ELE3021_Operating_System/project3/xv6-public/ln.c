#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){
    printf(2, "Usage: ln opt old new\n");
    exit();
  }

  if(!(strcmp(argv[1], "-h") || strcmp(argv[1], "-s"))){
    printf(2, "opt %s: failed\n", argv[1]);
	exit();
  }

  // Symbolic Link (Soft Link)
  if((!strcmp(argv[1], "-s")) && (symlink(argv[2], argv[3]) < 0)){
    printf(2, "symlink %s %s: failed\n", argv[2], argv[3]);
	exit();
  }
  // Hard Link
  if((!strcmp(argv[1], "-h")) && (link(argv[2], argv[3]) < 0)){
    printf(2, "link %s %s: failed\n", argv[2], argv[3]);
	  exit();
  }
  exit();
}
