#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  int pid = getpid();
  int pid_plus = getpid_plus();

  printf(1, "pid = %d\n", pid);
  printf(1, "pid+1 = %d\n", pid_plus);

  exit();
}
