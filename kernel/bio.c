// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct spinlock bin_lock[NBUCKETS];
  struct buf bin_head[NBUCKETS];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

inline uint hash_fn(uint x) {
  uint key = x % NBUCKETS;
  return key;
}

void assert(int cond, char *msg) {
  if (!cond) {
    char buf[100];
    snprintf(buf, 100, "assert failed: %s", msg);
    panic(buf);
  }
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.bin_lock[i], "bcache.bucket");
    bcache.bin_head[i].prev = &bcache.bin_head[i];
    bcache.bin_head[i].next = &bcache.bin_head[i];
  }

  // Create linked list of buffers
  struct buf *insert_head = &bcache.bin_head[0];
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = insert_head->next;
    b->prev = insert_head;
    initsleeplock(&b->lock, "buffer");
    insert_head->next->prev = b;
    insert_head->next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // first phase: find
  uint key = hash_fn(blockno);
  // assert(key < NBUCKETS, "key out of bound");
  struct buf *head = &bcache.bin_head[key];
  acquire(&bcache.bin_lock[key]);
  for(b = head->next; b != head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bin_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bin_lock[key]);

  // second phase: atomic find and insert
  acquire(&bcache.lock);
  // Is the block already cached?
  acquire(&bcache.bin_lock[key]);
  for(b = head->next; b != head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bin_lock[key]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bin_lock[key]);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // scan for the available LRU buf

  struct buf *curr_buf = 0;
  uint curr_timestamp = 0xFFFFFFFFU;
  int curr_bin = -1;

  for (int i = 0; i < NBUCKETS; i++) {
    acquire(&bcache.bin_lock[i]);
    for(b = bcache.bin_head[i].prev; b != &bcache.bin_head[i]; b = b->prev){
      if(b->refcnt > 0)
        continue;
      if (b->timestamp < curr_timestamp) {
        // replace the LRU buf
        // release the previous lock if existing
        if (curr_bin >= 0) {
          release(&bcache.bin_lock[curr_bin]);
        }
        curr_timestamp = b->timestamp;
        curr_buf = b;
        curr_bin = i;
        break;
      }
    }
    if (curr_bin != i) {
      release(&bcache.bin_lock[i]);
    }
  }

  if (curr_buf == 0) {
    panic("bget: no buffers");
  }

  // now holding the bcache and bin_lock[curr_bin]
  b = curr_buf;
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;
  if (curr_bin == key) {
    release(&bcache.bin_lock[key]);
  } else {
    // move curr_buf from old bin to new one
    b->prev->next = b->next;
    b->next->prev = b->prev;
    release(&bcache.bin_lock[curr_bin]);
    acquire(&bcache.bin_lock[key]);
    // insert to head
    struct buf *insert_head = &bcache.bin_head[key];
    b->next = insert_head->next;
    b->prev = insert_head;
    insert_head->next->prev = b;
    insert_head->next = b;
    release(&bcache.bin_lock[key]);
  }
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = hash_fn(b->blockno);
  acquire(&bcache.bin_lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    struct buf *insert_head = &bcache.bin_head[key];
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = insert_head->next;
    b->prev = insert_head;
    insert_head->next->prev = b;
    insert_head->next = b;
  }
  release(&bcache.bin_lock[key]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


