#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

void exec_(char* prog, char *argv[]) {
  // printf("exec_( %s, %s)\n", prog, argv[0]);
  int pid = fork();
  if (pid == 0) {
    exec(prog, argv);
    printf("exec failed!\n");
    exit(1);
  } else {
    wait(0);
  }
}

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "provide an argument.\n");
    exit(-1);
  }
  char buf[500];
  char *p = buf;
  char *argv_[MAXARG+1];
  argv_[argc-1] = buf;
  argv_[argc] = 0;

  memmove(argv_, argv+1, (argc-1) * sizeof(char*));
  char *program = argv[1];

  while (read(0, p, 1) == 1) {
    if (*p == '\n' || *p == '\0') {
      if (p-buf == 0) {
        continue;
      }
      *p = '\0';
      exec_(program, argv_);
      p = buf;
      continue;
    }
    ++p;
  }

  exit(0);
}
