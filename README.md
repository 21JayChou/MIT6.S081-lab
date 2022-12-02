# Lab 3: Page tables

1. ### Speed up system calls

   #### 代码实现：

   首先为了让进程有能记录syscall相关信息的页，需要将usyscall结构体加入struct proc中。

   ```c
   struct proc {
     struct spinlock lock;
   
     // p->lock must be held when using these:
     enum procstate state;        // Process state
     void *chan;                  // If non-zero, sleeping on chan
     int killed;                  // If non-zero, have been killed
     int xstate;                  // Exit status to be returned to parent's wait
     int pid;                     // Process ID
   
     // wait_lock must be held when using this:
     struct proc *parent;         // Parent process
   
     // these are private to the process, so p->lock need not be held.
     uint64 kstack;               // Virtual address of kernel stack
     uint64 sz;                   // Size of process memory (bytes)
     pagetable_t pagetable;       // User page table
     struct trapframe *trapframe; // data page for trampoline.S
     struct usyscall  *usyscall;  // syscall page
     struct context context;      // swtch() here to run process
     struct file *ofile[NOFILE];  // Open files
     struct inode *cwd;           // Current directory
     char name[16];               // Process name (debugging)
   };
   ```

   然后在proc_pagetable 函数中使用mappages函数为USYSCALL与一个物理页构建一个映射，在该物理页的开始存储进程的结构usyscall。

   ```c
   pagetable_t
   proc_pagetable(struct proc *p)
   {
     pagetable_t pagetable;
   
     // An empty page table.
     pagetable = uvmcreate();
     if(pagetable == 0)
       return 0;
   
     // map the trampoline code (for system call return)
     // at the highest user virtual address.
     // only the supervisor uses it, on the way
     // to/from user space, so not PTE_U.
     if(mappages(pagetable, TRAMPOLINE, PGSIZE,
                 (uint64)trampoline, PTE_R | PTE_X) < 0){
       uvmfree(pagetable, 0);
       return 0;
     }
   
     // map the trapframe page just below the trampoline page, for
     // trampoline.S.
     if(mappages(pagetable, TRAPFRAME, PGSIZE,
                 (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
       uvmunmap(pagetable, TRAMPOLINE, 1, 0);
       uvmfree(pagetable, 0);
       return 0;
     }
     if(mappages(pagetable,USYSCALL,PGSIZE,
                 (uint64)(p->usyscall),PTE_R|PTE_U)<0){
         uvmunmap(pagetable,TRAPFRAME,1,0);
         uvmunmap(pagetable, TRAMPOLINE, 1, 0);
       uvmfree(pagetable, 0);
       return 0;
     }
   
     return pagetable;
   }
   ```

   参考原代码中TRAMPOLINE和TRAPFRAME映射的构建，由于要求分配一个只读页并且需要用户层能够访问，传入参数"PTE_R|PTE_U"使得物理页的只读位和用户可读为设为1。同时当mappages的函数返回值小于0，说明映射构建失败，那么需要释放之前对TRAMPOLINE和TRAPFRAME分配的物理页，再释放创建的页表。

   随后需要在allocproc函数中对usyscall分配内存并初始化

   代码如下:

   ```c
   if((p->usyscall=(struct usyscall *)kalloc())==0){
       freeproc(p);
       release(&p->lock);
       return 0;
     }
   ```

   然后是在freeproch函数中对usyscall的内存进行释放：

   ```c
    if(p->usyscall)
       kfree((void*)p->usyscall);
     p->usyscall=0;
   ```

   最后在对页表进行释放操作时，需要在proc_freepagetable函数中移除USYSCALL到页表对应位置的映射。使用uvmunmap函数实现。

   ```c
   void
   proc_freepagetable(pagetable_t pagetable, uint64 sz)
   {
     uvmunmap(pagetable, TRAMPOLINE, 1, 0);
     uvmunmap(pagetable, TRAPFRAME, 1, 0);
     uvmunmap(pagetable,USYSCALL,1,0);
     uvmfree(pagetable, sz);
   }
   ```

2. ### Print a page talbe

   #### 代码实现：

   先参考freewalk代码如何遍历所有的页表项。

   ```c
   void
   freewalk(pagetable_t pagetable)
   {
     // there are 2^9 = 512 PTEs in a page table.
     for(int i = 0; i < 512; i++){
       pte_t pte = pagetable[i];
       if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
         // this PTE points to a lower-level page table.
         uint64 child = PTE2PA(pte);
         freewalk((pagetable_t)child);
         pagetable[i] = 0;
       } else if(pte & PTE_V){
         panic("freewalk: leaf");
       }
     }
     kfree((void*)pagetable);
   }
   ```

   首先用for循环遍历给定页表，对于每一个页表项，如果它的有效位为1且其PTE_R、PTE_W、PTE_X位都不为1，说明它不是叶子页表项，也就是说它并不是映射到最终的物理地址，而是映射到它的子页表。因此就递归调用freewalk函数来遍历它对应的子页表。整个freewalk相当于采用dfs的思想遍历并释放页表，可以以同样的思想来编写vmprint。

   ```c
   void
   vmprint(pagetable_t pagetable,int depth)
   {
     if(depth==0)
     printf("page table %p\n",(uint64)pagetable);
     for(int i=0;i<512;i++){
       pte_t pte=pagetable[i];
       if(pte&PTE_V){
         for(int j=0;j<=depth;j++)printf(" ..");
         printf("%d: pte %p pa %p\n",i,(uint64)pte,PTE2PA(pte));
         uint64 child=PTE2PA(pte);
         if((pte&(PTE_R|PTE_W|PTE_X))==0)
         vmprint((pagetable_t)child,depth+1);
       }
     }
   }
   ```

   在代码实现中，用depth记录页表的层数，当depth为0，首先打印第一个页表的地址。在循环中，每遍历到一个页表项，先判断其是否有效，若有效就根据depth打印对应数量的" .."和页表项的信息和其映射的物理地址。然后判断pte的PTE_R、PTE_W、PTE_X位是否为1，若都为0，说明还不是叶子页表，需要递归调用函数。

   然后需要在exec.c中使用vmprint打印进程号为1的进程的页表:

   ```c
   if(p->pid==1)vmprint(p->pagetable,0);
   ```

3. ### Detect which pages have been accessed

   #### 代码实现：

   首先浏览pgaccess_test代码观察pgaccess如何使用：

   ```c
   void
   pgaccess_test()
   {
     char *buf;
     unsigned int abits;
     printf("pgaccess_test starting\n");
     testname = "pgaccess_test";
     buf = malloc(32 * PGSIZE);
     if (pgaccess(buf, 32, &abits) < 0)
       err("pgaccess failed");
     buf[PGSIZE * 1] += 1;
     buf[PGSIZE * 2] += 1;
     buf[PGSIZE * 30] += 1;
     if (pgaccess(buf, 32, &abits) < 0)
       err("pgaccess failed");
     if (abits != ((1 << 1) | (1 << 2) | (1 << 30)))
       err("incorrect access bits set");
     free(buf);
     printf("pgaccess_test: OK\n");
   }
   ```

   可以看到pgaccess有三个形参，第一个形参是要检查的页表的首虚拟地址，第二个形参是检查页表的页数，第三个形参是用于记录哪些页表被访问过。测试程序先对位于第1,2,30个页表的数据进行访问，然后调用pgaccess,判断第三个参数的返回值是否与预期相同来检验程序的正确性。可以看到它是判断返回值的对应位是否为1来检查的。因此在内核层实现函数时，可以定义一个int型变量，每找到一个被访问过的页表，就将其页号对应的二进制位置1。

   然后是确定PTE_A的位数，通过查阅资料：

   ![image-20221101083059710](C:\Users\25061\AppData\Roaming\Typora\typora-user-images\image-20221101083059710.png)

   可知PTE_A应设置在第六位，当页表被访问时该位被置1。在riscv.h中定义如下：

   ```c
   #define PTE_A (1L << 6)
   ```

   最后是对sys_pgaccess的实现：

   ```c
   int
   sys_pgaccess(void)
   {
      uint64 buf;
      int size;
      int abits;
      argaddr(0,&buf);
      argint(1,&size);
      argint(2,&abits);
      unsigned int bitmask=0;
      struct proc *p=myproc();
      for(int i=0;i<size;i++)
      {
        pte_t *t=walk(p->pagetable,buf,0);
        if(*t==0)return -1;
        if(*t&PTE_A) 
        {
          bitmask|=1L<<i;
          *t-=1L<<6;
        }
        buf+=PGSIZE;
      }
      copyout(p->pagetable,abits,(char*)&bitmask,sizeof(bitmask));
     return 0;
   }
   ```

   首先用户传入的参数分别被放在寄存器0,1,2中，分别根据不同参数类型调用argaddr和argint将这些参数取出。定义一个变量bitmask用于记录被访问过的页表。进入for循环（循环次数由给定的需检查的页数决定），首先需要获取给定虚拟地址对应的页表项，这由walk函数实现，它对进程的页表逐级查找，最终返回对应的页表项地址。获取页表项后，判断它的PTE_A位是否为1，若为1说明它被访问过，那么将bitmask的对应位置1，并且将该页表项的PTE_A置0，否则在之后的pgaceess查找中，该页表将一直是被访问过的状态，而一般应该认为若在两次pgaccess查找中一个页表若没有被访问过，他就不应该被后面那次查找所记录。循环的最后将buf地址加上PGSIZE，对下一页进行查找。在函数最后，我们需要将bitmask记录的值传回到第三个参数中，调用copyout函数即可实现。

   

