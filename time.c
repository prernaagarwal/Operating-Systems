#ifdef CS333_P2
#include "types.h"
#include "user.h"
	
int
main(int argc, char * argv[])
{
 
  int start = uptime();	
   
  int pid = fork();
  
   
  if (pid < 0)
  {
    printf(1,"Error: pid < 0");
    exit();
  }  
  
  else if (pid == 0)
  {
    // child process
    // call commands
    exec(argv[1], argv+1);
    exit();
  }
  
  
  else
  {
    wait();
    // parent stuff
    int time_ticks = uptime() - start;
    int time_rem = time_ticks %1000;
    int time_sec = time_ticks/1000;
    
    printf(1, "%s ran in ", argv[1]);
    if (time_rem <10)
    {
    	printf(1,"%d.00%d seconds\n", time_sec, time_rem);
    }
    else if (time_rem > 10 && time_rem < 100)
    {
	printf(1,"%d.0%d seconds\n", time_sec, time_rem);
    }
    else
    {
	printf(1,"%d.%d seconds\n", time_sec, time_rem);
    }
    
    exit();
  }
  exit();
}


#endif
