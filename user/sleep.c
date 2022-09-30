#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("fatal: sleep needs an argument\n");
    exit(-1);
  }
  int n_tick = atoi(argv[1]);
  int status = sleep(n_tick);
  exit(status);
}
