// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int ref[(PHYSTOP - KERNBASE) / PGSIZE];
} page_ref;

#define PA2IDX(pa) (((uint64)pa - KERNBASE) / PGSIZE)
void page_ref_inc(uint64 pa){
  acquire(&page_ref.lock);
  ++page_ref.ref[PA2IDX(pa)];
  release(&page_ref.lock);
}
void page_ref_one(uint64 pa){
  acquire(&page_ref.lock);
  page_ref.ref[PA2IDX(pa)] = 1;
  release(&page_ref.lock);
}
void page_ref_dec(uint64 pa){
  acquire(&page_ref.lock);
  --page_ref.ref[PA2IDX(pa)];
  release(&page_ref.lock);
}

void page_ref_lock(){
  acquire(&page_ref.lock);
}
void page_ref_unlock(){
  release(&page_ref.lock);
}
int page_ref_getcnt(uint64 pa){
  return page_ref.ref[PA2IDX(pa)];
}
void page_ref_inc_wl(uint64 pa){
  ++page_ref.ref[PA2IDX(pa)];
}
void page_ref_dec_wl(uint64 pa){
  --page_ref.ref[PA2IDX(pa)];
}

void
kinit()
{
  // debug 1: not initialize
  initlock(&page_ref.lock, "page_ref");
  acquire(&page_ref.lock);
  memset(page_ref.ref, 0, sizeof(page_ref.ref));
  release(&page_ref.lock);

  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_init(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  acquire(&page_ref.lock);
  int idx = PA2IDX(pa);
  if(--page_ref.ref[idx] > 0){
    release(&page_ref.lock);
    return;
  }
  page_ref.ref[idx] = 0; // when kinit, the value will be -1
  release(&page_ref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void
kfree_init(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    acquire(&page_ref.lock);
    page_ref.ref[PA2IDX(r)] = 1;
    release(&page_ref.lock);
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void *
kalloc_noref(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
