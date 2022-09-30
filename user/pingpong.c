#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid;
  int pipe_parent_to_child[2];
  int pipe_child_to_parent[2];
  char buf;
  // create a pipe, with two FDs in fds[0], fds[1].
  pipe(pipe_parent_to_child);
  pipe(pipe_child_to_parent);
  pid = fork();
  if (pid == 0) {
    // child process
    read(pipe_parent_to_child[0], &buf, 1);
    printf("%d: received ping\n", getpid());
    write(pipe_child_to_parent[1], "b", 1);
  } else {
    // parent process
    write(pipe_parent_to_child[1], "a", 1);
    read(pipe_child_to_parent[0], &buf, 1);
    printf("%d: received pong\n", getpid());
  }
  exit(0);
}
