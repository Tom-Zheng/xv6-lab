#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void subprocess(int fd_input) {
  // printf("[DEBUG] process: %d\n", getpid());
  int n_recv;
  int buf;
  n_recv = read(fd_input, &buf, sizeof(int));
  if (n_recv == 0) {close(fd_input); return;}
  int base = buf;
  printf("prime %d\n", base);
  
  int pid = -1;
  int fds[2];
  fds[0] = fds[1] = -1;
  while(read(fd_input, &buf, sizeof(int))) {
    if (buf % base) {
      if (fds[1] == -1) {
        // create right neighbor and set up pipe
        pipe(fds);
        pid = fork();
        if (pid == 0) {
          break;
        }
      }
      write(fds[1], &buf, sizeof(int));
    }
  }
  close(fd_input);
  close(fds[1]);

  if (pid == 0) {
    subprocess(fds[0]);
  } else {
    wait(0);
  }
}

int
main(int argc, char *argv[])
{
  close(0);
  // printf("[DEBUG] process: %d\n", getpid());
  int fds[2];
  pipe(fds);
  int pid;
  pid = fork();
  if (pid == 0) {
    close(fds[1]);
    subprocess(fds[0]);
  } else {
    for (int i = 2; i < 36; i++) {
      write(fds[1], &i, sizeof(int));
    }
    close(fds[1]);
    wait(0);
  }
  exit(0);
}
