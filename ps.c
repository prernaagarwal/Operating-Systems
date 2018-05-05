#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"


void
display_time(int elapsed_time)
{
    int time_rem = elapsed_time % 1000;
    int time_sec = elapsed_time / 1000;
    if (time_rem <10)
    {
    	printf(1,"%d.00%d\t", time_sec, time_rem);
    }
    else if ( time_rem > 10 && time_rem <100)
    {
	printf(1,"%d.0%d\t", time_sec, time_rem);
    }
    else
    {
    	printf(1,"%d.%d\t", time_sec, time_rem);
    }
}

int
main(void)
{
  
//change max here
  
  int max = 16;
  struct uproc * utable = malloc(sizeof(struct uproc)*max); 
  int num = getprocs(max, utable);
  int i;
  if (num > max || num < 0)
  {
    free(utable);
    exit();
  }

  printf(1, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", "PID", "Name", "UID", "GID","PPID","Elapsed", "CPU", "State", "Size");
  for(i = 0;i < num; ++i)
  {
    printf(1, "%d\t%s\t%d\t%d\t%d\t", utable[i].pid, utable[i].name, utable[i].uid, utable[i].gid, utable[i].ppid);
    
    int elapsed_ticks = utable[i].elapsed_ticks;
  
    display_time(elapsed_ticks);
    display_time(utable[i].CPU_total_ticks);
    printf(1,"%s\t%d\n", utable->state, utable->size); 
	 
  }
  free(utable);
  exit();
}
#endif
