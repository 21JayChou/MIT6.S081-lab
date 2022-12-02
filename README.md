# Lab 2 system calls

1. ###  system call tracing

   本题要求实现一个trace的系统调用，用于追踪系统调用的信息。它接受一个参数，该参数的二进制表示中，哪一位为1，从该位的位数找到对应的系统调用编号，进而找到要追踪的系统调用。它追踪的信息包括对应进程的进程号、系统调用名称和系统调用的返回值。

   #### 代码实现：

   首先在user.h文件中添加函数声明：

   ```c
   // system calls
   int fork(void);
   int exit(int) __attribute__((noreturn));
   int wait(int*);
   int pipe(int*);
   int write(int, const void*, int);
   int read(int, void*, int);
   int close(int);
   int kill(int);
   int exec(const char*, char**);
   int open(const char*, int);
   int mknod(const char*, short, short);
   int unlink(const char*);
   int fstat(int fd, struct stat*);
   int link(const char*, const char*);
   int mkdir(const char*);
   int chdir(const char*);
   int dup(int);
   int getpid(void);
   char* sbrk(int);
   int sleep(int);
   int uptime(void);
   int trace(int);
   int sysinfo(struct sysinfo*);
   ```

   然后在usys.pl中添加stub,并在syscall.h中为sys_trace定义一个编号：

   ```c
   // System call numbers
   #define SYS_fork    1
   #define SYS_exit    2
   #define SYS_wait    3
   #define SYS_pipe    4
   #define SYS_read    5
   #define SYS_kill    6
   #define SYS_exec    7
   #define SYS_fstat   8
   #define SYS_chdir   9
   #define SYS_dup    10
   #define SYS_getpid 11
   #define SYS_sbrk   12
   #define SYS_sleep  13
   #define SYS_uptime 14
   #define SYS_open   15
   #define SYS_write  16
   #define SYS_mknod  17
   #define SYS_unlink 18
   #define SYS_link   19
   #define SYS_mkdir  20
   #define SYS_close  21
   #define SYS_trace  22
   #define SYS_sysinfo 23
   ```

   在结构proc中定义变量mask用于记录被跟踪的系统调用.

   然后在sysproc.c中实现sys_trace:

   ```c
   uint64
   sys_trace(void)
   {
      int mask;
      argint(0,&mask);
      if(mask==-1)return -1;
      myproc()->mask=mask;
      return 0;
   }
   
   ```

   在syscall.c中的入口函数数组中添加 sys_trace并定义一个字符串数组存储所有系统调用名称：

   ```c
   // An array mapping syscall numbers from syscall.h
   // to the function that handles the system call.
   static uint64 (*syscalls[])(void) = {
   [SYS_fork]    sys_fork,
   [SYS_exit]    sys_exit,
   [SYS_wait]    sys_wait,
   [SYS_pipe]    sys_pipe,
   [SYS_read]    sys_read,
   [SYS_kill]    sys_kill,
   [SYS_exec]    sys_exec,
   [SYS_fstat]   sys_fstat,
   [SYS_chdir]   sys_chdir,
   [SYS_dup]     sys_dup,
   [SYS_getpid]  sys_getpid,
   [SYS_sbrk]    sys_sbrk,
   [SYS_sleep]   sys_sleep,
   [SYS_uptime]  sys_uptime,
   [SYS_open]    sys_open,
   [SYS_write]   sys_write,
   [SYS_mknod]   sys_mknod,
   [SYS_unlink]  sys_unlink,
   [SYS_link]    sys_link,
   [SYS_mkdir]   sys_mkdir,
   [SYS_close]   sys_close,
   [SYS_trace]   sys_trace,
   [SYS_sysinfo] sys_sysinfo,
   };
   static char *syscall_name[]={"","fork","exit","wait","pipe",
   "read","kill","exec","fstat","chdir","dup","getpid","sbrk",
   "sleep","uptime","open","write","mknod","unlink","link","mkdir",
   "close","trace","sysinfo"};
   
   ```

   最后修改syscall让它可以打印相应信息：

   ```c
   void
   syscall(void)
   {
     int num;
     struct proc *p = myproc();
   
     num = p->trapframe->a7;
     if(num > 0 && num < NELEM(syscalls)&&syscalls[num]) {
       // Use num to lookup the system call function for num, call it,
       // and store its return value in p->trapframe->a0
       p->trapframe->a0 = syscalls[num]();
       if((p->mask>>num)&1)
       printf("%d: syscall %s -> %d\n",p->pid,syscall_name[num],p->trapframe->a0);
     } else {
       printf("%d %s: unknown sys call %d\n",
               p->pid, p->name, num);
       p->trapframe->a0 = -1;
     }
   }
   ```

   2. ### sysinfo

      该实验增加系统调用isysinfo，用于得到剩余空闲内存的大小，和不处于UNUSED状态的进程总数。

      #### 代码实现：

      用户层声明和trace类似，下面介绍内核层函数实现。

      首先在proc.c中实现一个统计进程数的函数。

      ```c
      //return the amount of not UNUSED processes
      int 
      not_unused_proc_amount(void)
      {
         struct proc *p;
         int nproc=0;
         for(p=proc;p<&proc[NPROC];p++)
         {
           acquire(&p->lock);
           if(p->state!=UNUSED) nproc++;
           release(&p->lock);
         }
         return nproc;
      }
      
      ```

      然后在kalloc.c中实现一个计算空闲内存的函数。

      ```c
      uint64
      free_memory_amount(void)
      {
        uint64 free_memory=0;
         struct run *r=kmem.freelist;
         while(r)
         {
          free_memory+=PGSIZE;
          r=r->next;
         }
         return free_memory;
      }
      
      ```

      最后实现sys_sysinfo,使用copyout函数将存储信息的sysinfo传回到用户给定的参数地址。

      ```c
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
      
      ```

      
