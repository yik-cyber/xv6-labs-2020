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
} bcache;

struct {
  struct spinlock lock;
  struct buf head;
} bucket[NBUCKET];


void
binit(void)
{
  struct buf *b;
  int i;

  // initialize cache block
  initlock(&bcache.lock, "bcache");
  for(i = 0; i < NBUCKET; ++i){
    initlock(&bucket[i].lock, "bcache");
    bucket[i].head.prev = bucket[i].head.next = &bucket[i].head;
  }

  // printf("init.\n");
  
  // initialize buffer block and insert buffer into the bucket
  for(i = 0, b = bcache.buf; b < bcache.buf + NBUF; ++b, i = (i+1) % NBUCKET){
    initsleeplock(&b->lock, "buffer");
    b->next = bucket[i].head.next;
    b->prev = &bucket[i].head;
    bucket[i].head.next->prev = b;
    bucket[i].head.next = b;
  }

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b, *head;

  int i = blockno % NBUCKET;
  
  // if the buf has been cached
  acquire(&bucket[i].lock);
  head = &bucket[i].head;
  for(b = head->next; b != head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // not cached
  uint min_time_stamp = (uint)-1;
  struct buf *replace = 0;
  acquire(&bcache.lock); // take advantage of the lock to serialize eviction
  for(b = head->next; b != head; b = b->next){
    if(b->refcnt == 0){
      if(b->time_stamp < min_time_stamp){
        min_time_stamp = b->time_stamp;
        replace = b;
      }
    }
  }

  if(replace)
    goto find;

  int rj = -1;
  for(int j = 0; j < NBUCKET; ++j){
    if(j == i)
      continue;
    struct buf *other_head = &bucket[j].head;
    for(b = other_head->next; b != other_head; b = b->next){
      if(b->refcnt == 0){
        if(b->time_stamp < min_time_stamp){
          min_time_stamp = b->time_stamp;
          replace = b;
          rj = j;
        }
      }
    }
  }

  if(replace){
    // get the replace buf out of origional bucket and then insert it into the new bucket
    acquire(&bucket[rj].lock);
    replace->prev->next = replace->next;
    replace->next->prev = replace->prev;
    release(&bucket[rj].lock);

    replace->next = head->next;
    replace->prev = head;
    head->next = head->next->prev = replace;

    goto find;
  }

  panic("bget: no buffers");

  find:
  release(&bcache.lock);
  replace->dev = dev;
  replace->blockno = blockno;
  replace->valid = 0;
  replace->refcnt = 1;
  release(&bucket[i].lock);
  acquiresleep(&replace->lock);
  return replace;  
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

  int i = b->blockno % NBUCKET;

  acquire(&bucket[i].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->prev->next = b->next;
    b->next->prev = b->prev;

    b->next = bucket[i].head.next;
    b->prev = &bucket[i].head;

    b->next->prev = b->prev->next = b;
  }
  
  release(&bucket[i].lock);
}

void
bpin(struct buf *b) {
  int i = b->blockno % NBUCKET;
  acquire(&bucket[i].lock);
  b->refcnt++;
  release(&bucket[i].lock);
}

void
bunpin(struct buf *b) {
  int i = b->blockno % NBUCKET;
  acquire(&bucket[i].lock);
  b->refcnt--;
  release(&bucket[i].lock);
}


