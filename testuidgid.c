#ifdef CS333_P2
#include "types.h"
#include "user.h"
int
testuidgid(void)
{
  uint uid, gid, ppid;
  uid = getuid();
  printf(2, "Current UID is: %d\n", uid);
  printf(2, "Setting UID to 100\n");
  int val= setuid(100);
  
  if (val < 0)
  {
    printf(2,"error: given value is less than or greater than 32767\n");
  }
  uid = getuid();
  printf(2, "Current UID is: %d\n", uid); 
  
  printf(2, "Setting UID to -55\n");
  val = setuid(-55);

  if (val < 0)
  {
    printf(2,"error: given value is less than 0\n");
  }
  uid = getuid();
  printf(2, "Current UID is: %d\n", uid);
  
  printf(2, "Setting UID to 40000\n");
  val = setuid(40000);
  if (val < 0)
  {
    printf(2,"error: given value is greataer than 32767\n");
  } 
  uid = getuid();
  printf(2, "Current UID is: %d\n", uid);
  
  
  
  gid = getgid();
  printf(2, "Current GID is: %d\n", gid);
  
  printf(2, "Setting GID to 100\n");
  val = setgid(100);
  
  if (val < 0)
  {
    printf(2,"error: given value is less than 0 or greater than 0\n");
  } 
  gid = getgid();
  printf(2, "Current GID is: %d\n", gid);
  
  printf(2, "Setting GID to 32769\n");
  val = setgid(32769);
  
  if (val < 0)
  {
    printf(2,"error: given value is greater than 32767\n");
  } 
  gid = getgid();
  printf(2, "Current GID is: %d\n", gid);
 
  printf(2, "Setting GID to -1000\n");
  val= setgid(-1000);
  if(val < 0)
  {
    printf(2,"error: given value is less than 0\n");
  } 
  gid = getgid();
  printf(2, "Current GID is: %d\n", gid);
 
  ppid = getppid();
  printf(2, "My parent process is: %d\n", ppid);
  printf(2, "Done!\n");
  return 0;
}

int 
main()
{
  testuidgid();
  exit();
}
#endif
