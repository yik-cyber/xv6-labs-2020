#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);

  backtrace();
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


uint64 sys_sigalarm(void){
  int per_tick_num;
  uint64 handler_adr;

  if(argint(0, &per_tick_num) < 0){
    return -1;
  }
  if(argaddr(1, &handler_adr) < 0){
    return -1;
  }
  
  struct proc* p = myproc();
  p->per_tick_num = per_tick_num;
  p->handler_adr = handler_adr;
  p->in_handler = 0;

  return 0;

}

uint64 sys_sigreturn(void){
  struct proc* p = myproc();
  p->trapframe->epc = p->handler_frame->epc;

  uint64 start = (uint64)p->handler_frame + 8, end = start + 256,
         tf = (uint64)p->trapframe + 40;
  while(start < end){
    *(uint64*)tf = *(uint64*)start;
    tf += 8;
    start += 8;
  }
  p->in_handler = 0;
  
  // printf("return\n");
  // printf("ra: %p\n", p->trapframe->ra);
  return 0;
}