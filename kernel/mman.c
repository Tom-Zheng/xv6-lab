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
#include "fcntl.h"

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
    // printf("mmap: not implemented\n");
    return -1;
  }
  if ((!f->readable && prot & PROT_READ) || 
        (flags == MAP_SHARED && !f->writable && prot & PROT_WRITE)) {
    return -1;
  }
  struct vma *vma_list = myproc()->vma;
  for (int i = 0; i < 16; i++) {
    if (vma_list[i].valid == 0) {
      addr = AllocAddr(length);
      vma_list[i].valid = 1;
      vma_list[i].base_addr = addr;
      vma_list[i].addr = addr;
      vma_list[i].length = length;
      vma_list[i].prot = prot;
      vma_list[i].flags = flags;
      vma_list[i].offset = offset;
      vma_list[i].ofile = filedup(f);
      // printf("Create VMA: addr=%p\n", addr);
      return addr;
    }
  }
  panic("No available vma slot.");
  return -1;
}

void free_vma(struct vma *v) {
  v->valid = 0;
  fileclose(v->ofile);
}

int write_page(struct file *f, uint off, uint64 pa) {
  begin_op();
  ilock(f->ip);
  uint write_size = PGSIZE;
  uint file_size = f->ip->size;
  if (off + write_size > file_size) {
    write_size = file_size - off;
  }
  iunlock(f->ip);
  end_op();

  int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
  int i = 0;
  int r, ret = 0;
  while(i < write_size){
    int n1 = write_size - i;
    if(n1 > max)
      n1 = max;
    begin_op();
    ilock(f->ip);
    if ((r = writei(f->ip, 0, pa, off, n1)) > 0)
      off += r;
    iunlock(f->ip);
    end_op();

    if(r != n1){
      // error from writei
      printf("off: %d, write: %d, actual: %d\n", off, n1, r);
      break;
    }
    i += r;
  }
  ret = (i == write_size ? write_size : -1);
  return ret;
}
// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
vmaunmap(pagetable_t pagetable, uint64 va, uint64 npages, uint64 base, struct file *f)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("vmaunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("vmaunmap: not a leaf");
    uint64 pa = PTE2PA(*pte);
    // write back
    if (f && (*pte & PTE_D)) {
      uint off = a - base;
      // printf("a=%p, base=%p, off=%d\n", a, base, off);
      if (write_page(f, off, pa) < 0) {
        panic("vmaunmap: write back err");
      }
    }
    // free physical page
    kfree((void*)pa);
    *pte = 0;
  }
}

int munmap(uint64 addr, uint64 length) {
  struct proc *p = myproc();
  struct vma *vma_list = p->vma;
  struct vma *curr_vma = 0;
  for (int i = 0; i < 16; i++) {
    if (!vma_list[i].valid)
      continue;
    if (addr >= vma_list[i].addr && 
          addr < vma_list[i].addr + vma_list[i].length) {
      curr_vma = &vma_list[i];
      break;
    }
  }
  if (curr_vma == 0) {
    return -1;
  }

  uint64 free_start = addr, free_end = addr + length;
  if (addr == curr_vma->addr) {
    if (length > curr_vma->length) {
      return -1;
    } else {
      curr_vma->addr = free_end;
      curr_vma->length -= length;
    }
  } else if (free_end == curr_vma->addr + curr_vma->length) {
    curr_vma->length -= length;
  } else {
    return -1;
  }

  struct file *f = curr_vma->ofile;
  if (curr_vma->flags == MAP_PRIVATE)
    f = 0;
  // free pages
  vmaunmap(p->pagetable, free_start, (free_end - free_start) / PGSIZE, curr_vma->base_addr, f);
  if (curr_vma->length == 0) {
    free_vma(curr_vma);
  }
  return 0;
}

int mmap_trap_handler(uint64 va) {
  struct proc *p = myproc();
  struct vma *vma_list = p->vma;
  struct vma *curr_vma = 0;
  for (int i = 0; i < 16; i++) {
    if (!vma_list[i].valid)
      continue;
    if (va >= vma_list[i].addr && 
          va < vma_list[i].addr + vma_list[i].length) {
      curr_vma = &vma_list[i];
      break;
    }
  }
  if (curr_vma == 0) {
    return -1;
  }
  // allocate the page
  uint64 ka = (uint64) kalloc();
  memset((void*) ka, 0, PGSIZE);
  if (ka == 0) {
    return -1;
  } else {
    // memset((void*) ka, 0, PGSIZE);
    // read from file to ka
    va = PGROUNDDOWN(va);
    struct file *f = curr_vma->ofile;
    if (f->type != FD_INODE) {
      panic("mmap_trap_handler: Not a file");
    }
    begin_op();
    ilock(f->ip);
    uint64 off = va - curr_vma->base_addr;
    uint fetch_size = PGSIZE;
    uint file_size = f->ip->size;
    if (off + fetch_size > file_size) {
      fetch_size = file_size - off;
    }
    if (readi(f->ip, 0, ka, off, fetch_size) != fetch_size) {
      panic("mmap_trap_handler: read failed");
    }
    iunlock(f->ip);
    end_op();
    int perm = PTE_U;
    if (curr_vma->prot & PROT_READ) {
      perm |= PTE_R;
    }
    if (curr_vma->prot & PROT_WRITE) {
      perm |= PTE_W;
    }
    // printf("mmap_trap_handler: mappage: va=%p, ka=%p, perm=%d\n", va, ka, perm);
    if (mappages(p->pagetable, va, PGSIZE, ka, perm) != 0) {
      // printf("mmap_trap_handler: mappages failed\n");
      kfree((void*) ka);
      return -1;
    }
  }
  return 0;
}