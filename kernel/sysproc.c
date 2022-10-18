#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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

uint64
sys_trace(void)
{
   int mask;
   argint(0,&mask);
   if(mask==-1)return -1;
   myproc()->mask=mask;
   return 0;
}
uint64
sys_sysinfo(void)
{
   uint64 sysinfo;
   struct sysinfo copy_info;
   struct proc *p=myproc();
   argaddr(0,&sysinfo);
   if(sysinfo<0)return -1;
   copy_info.freemem=free_memory_amount();
   copy_info.nproc=not_unused_proc_amount();
   copy_info.id="20300290027";
   printf("my student number is %s\n",copy_info.id);
   if(copyout(p->pagetable,sysinfo,(char*)&copy_info,sizeof(copy_info))<0)
   return -1;
   
   return 0;
}
