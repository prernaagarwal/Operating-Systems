#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char * argv[])
{
  if (argc != 3)
  {
    printf(1,"Error! chgrp takes 2 arguments!");
    exit();
  }
  
  // I do the bounds checking in chgrp function implemented in 
  // sysfile.c  
  if (chgrp(argv[2], atoi(argv[1])) == -1)
  {
    printf(1, " Chgrp system call returned an error!"); 
    exit();
  }
  //printf(1, "Not imlpemented yet.\n");
  exit();
}

#endif
