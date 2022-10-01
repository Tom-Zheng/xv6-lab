#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"
#include "defs.h"

uint64 sys_sysinfo(void) {
  struct sysinfo _info;
  uint64 output_addr;
  if (argaddr(0, &output_addr) < 0) {
    return -1;
  }
  _info.freemem = CountFreeMem();
  _info.nproc = CountTotalProc();

  // copy to user space
  struct proc *p = myproc();
  if(copyout(p->pagetable, output_addr, (char *)&_info, sizeof(struct sysinfo)) < 0)
    return -1;
  return 0;
}