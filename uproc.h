#define STRMAX 32
 
struct uproc {
  uint pid;
  uint uid;
  uint gid;
  uint ppid;
  uint elapsed_ticks;
  uint CPU_total_ticks;
  char state[STRMAX];
  uint size;
  char name[STRMAX];
  #ifdef CS333_P3P4
  int priority;
  int budget;
  #endif
};

int getprocs_helper(uint max, struct uproc * up);
