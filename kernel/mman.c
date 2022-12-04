#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "file.h"
#include "mman.h"

static void swap(uint64 *a, uint64 *b) {
  uint64 tmp = *a;
  *a = *b;
  *b = tmp;
}

static uint64 AllocAddr(uint64 length) {
  // find available vm address for the mmap request
  // sort current VMAs
  uint64 sorted_addr[16];
  uint64 sorted_length[16];
  int size = 0;
  struct vma *vma_list = myproc()->vma;
  for (int i = 0; i < 16; i++) {
    if (vma_list[i].valid) {
      sorted_addr[size] = vma_list[i].addr;
      sorted_length[size] = vma_list[i].length;
      size++;
    }
  }

  // bubble sort
  for (int i = size - 1; i > 0; i--) {
    int do_swap = 0;
    for (int j = 0; j < i; j++) {
      if (sorted_addr[j] > sorted_addr[j + 1]) {
        swap(&sorted_addr[j], &sorted_addr[j+1]);
        swap(&sorted_length[j], &sorted_length[j+1]);
        do_swap++;
      }
    }
    if (do_swap == 0)
      break;
  }

  uint64 new_addr = MMAP_MIN_ADDR;
  if (size == 0) {
    return new_addr;
  }

  for (int i = 0; i < size; i++) {
    if (new_addr + length <= sorted_addr[i]) {
      return new_addr;
    }
    new_addr = sorted_addr[i] + sorted_length[i];
  }
  return new_addr;
}

uint64 mmap(uint64 addr, uint64 length, int prot, int flags,
            struct file *f, uint64 offset) {
  if (addr > 0 || offset > 0) {
    printf("mmap: not implemented\n");
    return -1;
  }
  struct vma *vma_list = myproc()->vma;
  for (int i = 0; i < 16; i++) {
    if (vma_list[i].valid == 0) {
      addr = AllocAddr(length);
      vma_list[i].valid = 1;
      vma_list[i].addr = addr;
      vma_list[i].length = length;
      vma_list[i].prot = prot;
      vma_list[i].flags = flags;
      vma_list[i].offset = offset;
      vma_list[i].ofile = filedup(f);
      printf("Create VMA: addr=%p\n", addr);
      return addr;
    }
  }
  panic("No available vma slot.");
  return -1;
}

int munmap(uint64 addr, uint64 length) {
  return -1;
}