#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "[USER] before write syscall\n");

  write(1, "hello xv6\n", 10);

  printf(1, "[USER] after write syscall\n");

  exit();
}
