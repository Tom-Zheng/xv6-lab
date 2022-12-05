#pragma once

struct vma {
  int valid;
  uint64 base_addr;
  uint64 addr;
  uint64 length;
  int prot;
  int flags;
  int offset;
  struct file *ofile;
};
