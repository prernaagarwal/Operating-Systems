#ifdef CS333_P5
#include "types.h"
#include "user.h"
//#include "ulib.c"

int 
atoo(const char * s);

int
main(int argc, char * argv[])
{
  if (argc != 3)
  {
    printf(1," Error! chmod takes 2 arguments!");
    exit();
  }

  // I do the bounds checking in chmod function implemented in 
  // sysfile.c 
  if (chmod(argv[2], atoo(argv[1])) == -1)
  {
    printf(1, " Chmod system call returned an error!"); 
    exit();
  }

  exit();
}

#endif
