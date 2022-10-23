#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

uint64 sys_sigalarm(void) {
  int _tick;
  uint64 _fn;
  if(argint(0, &_tick) < 0 || argaddr(1, &_fn) < 0) {
    printf("sys_sigalarm: incorrect args\n");
    return -1;
  }
  // printf("sys_sigalarm, %d, %p\n", _tick, _fn);
  struct proc *p;
  p = myproc();
  p->alarm_ticks = _tick;
  p->alarm_handler = _fn;
  p->alarm_cnt = _tick - 1;
  return 0;
}

uint64 sys_sigreturn(void) {
  return 0;
}