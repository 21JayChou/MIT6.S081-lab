### Lab 5 : traps

1. ### RISC-V assembly

   #### 问题回答：

   1. **函数的参数包含在哪些寄存器中？例如在 main 对 printf 的调用中，哪个 寄存器保存 13？**

      函数参数包含在a0-a7中，若参数数量过多则超过的部分保存在内存中。在main对printf的调用中，寄存器a2保存13：![image-20221108092859709](C:\Users\25061\AppData\Roaming\Typora\typora-user-images\image-20221108092859709.png)

   2. **Main 的汇编代码中对函数 f 的调用在哪里？对 g 的调用在哪里？**

      main没有调用这两个函数，函数g被内联到f，f被内联到g。从下图可以看出main直接将12传递给printf,说明汇编代码进行了内联优化：

      ![image-20221108093359065](C:\Users\25061\AppData\Roaming\Typora\typora-user-images\image-20221108093359065.png)

   3. **函数 printf 位于哪个地址？**

      ![image-20221108093523552](C:\Users\25061\AppData\Roaming\Typora\typora-user-images\image-20221108093523552.png)

      由图知printf在地址0x65a处。

   4. **在 jalr 到 main 中的 printf 之后，寄存器 ra 中存储的值是？**

      ![image-20221108100529060](C:\Users\25061\AppData\Roaming\Typora\typora-user-images\image-20221108100529060.png)

      jalr的作用是跳转到指定的指令处，并且将下一条指令地址保存到ra中，由图可知下一条指令地址为0x38，所以ra中存储的值为0x38.

   5. **运行以下代码：**

      ```c
      unsigned int i = 0x00646c72;
      printf("H%x Wo%s", 57616, &i);
      ```

      **输出是什么？**

      输出为：

      ![image-20221108102421065](C:\Users\25061\AppData\Roaming\Typora\typora-user-images\image-20221108102421065.png)

      57616对应16进制为e110，i以小端方式存储时，顺序存储的值为72 6c 64 00，分别对应字符rld。如果RISC-V是big-endian，则应将i设为0x726c6400才能使得输出结果相同。

   6. **在下面的代码中，会打印出什么？（注意：答案不是特定值）为什么会 发生这种情况？**

      ```c
      printf("x=%d y=%d", 3)
      ```

      y对应的值不确定，因为第二个%d会传入a2寄存器中的值，而a2寄存器中的值没有指定，无法确定其中具体值。

2. #### Backtrace

   bactrace 需要遍历stack并且打印在每个帧栈中保存的返回地址，而每个帧栈的保存返回地址的地址为帧指针地址减8。那么就可以根据当前帧栈的帧指针地址得到保存的返回地址。而保存的上一个帧指针的地址为当前帧指针的地址减16，那么就可以通过当前帧指针找到上一个帧指针，如此递归便可实现stack的遍历。由于栈是从高地址向低地址扩张，而内核的栈是在PAGE地址对齐处分配一个页面，因此当帧指针遍历到栈对应的页的最高地址处，说明栈已遍历完毕。

   #### 代码实现：

   首先需要获取当前帧指针，根据提示将以下函数添加到kernel/riscv.h:

   ```c
   static inline uint64
   r_fp()
   {
    uint64 x;
    asm volatile("mv %0, s0" : "=r" (x) );
    return x;
   }
   ```

   该函数从s0中读取帧指针。

   backtrace实现如下：

   ```c
   void backtrace(void)
   {
     uint64 fp=r_fp();
     printf("backtrace:\n");
     while(fp!=PGROUNDUP(fp))
     {
          printf("%p\n",* (uint64*)(fp-8));
          fp=*(uint64*)(fp-16);
     }
   }
   
   ```

   首先调用r_fp得到帧指针并存储在fp中，然后进行循环遍历，`PGROUNDUP(fp)`对应的是栈对应页的顶部地址，当fp与之相等时说明遍历完毕。

   在循环中为了得到保存的返回地址，首先将fp-8得到保存的返回地址的地址，然后将其强制转化为指针类型，然后加上*进行取值操作即得到保存的返回地址，将其打印。

   随后需要让fp指向上一个帧指针，操作与上面类似，因为保存的帧指针的地址为fp-16，所以取地址为fp-16对应的值赋给fp即可。

   最后将backtrace加入到sys_sleep和panic中。

3. ### Alarm

   本实验中将向 xv6 中添加一个功能，该功能会在进程使用 CPU 时定期提醒它。实验需要添加一个新的 sigalarm(interval,handler)系统调用。如果一个应用程 序调用了 sigalarm(n,fn)，那么在它消耗了 n 个 CPU "ticks"之后（在 n 个时钟 中断后），将执行 handler 函数。当 handler 通过调用 sigreturn()返回时，应用程 序应该从中断的地方继续。Tick 由硬件计时器产生中断的频率决定。如果应用 程序调用 sigalarm(0,0)，则停止产生周期性调用

   1. #### Test0

      #### 代码实现：

      test0先注重实现sys_sigalarm ，因此让sys_sigreturn 返回0.

      sys_sigalarm需要接受两个参数，一个是用户定义的n，一个是函数指针handler。n是用户给定的警报间隔，handler是警报函数。因此需要在struct proc中记录这两个参数，同时需要额外一个字段记录距离上次调用handler经过了多少个时钟。

      ```c
      int ticks;                   // Alarm interval
        int passed_ticks;            //passed ticks since last the last call
        uint64 handler;              //Pointer to handler function
      ```

      在allocproc中进行初始化：

      ```c
      p->pid = allocpid();
        p->state = USED;
        p->handler=0;
        p->ticks=0;
        p->passed_ticks=0;
      ```

      在sys_sigalarm中需要接受并设置相应的参数：

      ```c
      uint64
      sys_sigalarm(void)
      {
        int ticks;
        uint64 handler;
        argint(0,&ticks);
        argaddr(1,&handler);
        struct proc *p=myproc();
        p->ticks=ticks;
        p->handler=handler;
        return 0;
      }
      
      ```

      随后的警报函数的调用需要在usertrap中实现，因为它的调用需要在时钟中断时进行。代码如下：

      ```c
      if(which_dev == 2){
        if(p->ticks!=0)
        {
           p->passed_ticks++;
           if(p->passed_ticks==p->ticks)
           {
             p->trapframe->epc=p->handler;
             p->passed_ticks=0;
           }
        }
        yield();
      }
      ```

      当usertrap检测到时钟中断时(which_dev==2)，首先判断p->ticks是否为0，若不为0，证明进程正在进行周期性警报函数调用。然后p->passed_ticks++代表距离上次调用警报函数又增加了一个时钟间隔。随后判断过去的时钟间隔数与指定的调用周期是否相等，如果相等则需要调用警报函数。由于函数的调用需要用户来进行，因此将p->trapframe中的epc设置为警报函数的地址值，使得返回用户空间时，进程的下一个指令就是执行警报函数。最后需要将passed_ticks置0来重新计数。

   2. #### Test1/Test2/Test3

      #### 代码实现：

      上述实现中在返回到用户态时，寄存器中的内容并未恢复，因此handler函数调用完成后，不会返回到时钟中断处的位置，而我们需要避免这一点使得handler能够被周期性地调用。

      为了在计时器到期时能够保存足够的状态，可以在struct proc中再定义一个trapframe字段（save_trapframe)，让它完全复制当前进程的trapframe中的所有值。

      ```c
      struct trapframe *save_trapframe;//used by sigreturn
      ```

      然后在allocproc中为其分配物理页：

      ```c
        if((p->save_trapframe = (struct trapframe *)kalloc()) == 0){
         freeproc(p);
          release(&p->lock);
          return 0;
        }
      ```

      在freeproc中释放：

      ```c
      if(p->save_trapframe)
          kfree((void*)p->save_trapframe);
      ```

      最后只需在将p->trapframe的epc设置为handler的地址之前，将trapframe保存到save_trapframe中即可：

      ```c
       *(p->save_trapframe)=*(p->trapframe);
       p->trapframe->epc=p->handler;
      ```

      当系统调用sigreturn 时，只需再将save_trapframe中的数据拷贝到trapframe中，因此sys_sigreturn的实现如下：

      ```c
      uint64
      sys_sigreturn(void)
      {
        struct proc *p=myproc();
        *(p->trapframe)=*(p->save_trapframe);
        return 0;
      }
      
      ```

      实验还要求，需要防止handler函数的重入调用。那么可以在proc中定义一个字段flag，初始值设置为0：

      ```c
      int flag;                    //prevent re-entrant calls to handler
      ```

      当计数器到期并且flag为0时才能进行handler的调用，在开始进行handler调用之前将flag设置为1，当handler返回后再恢复为0。

      flag的设置放在usertrap中，最终usertrap的实现如下：

      ```c
       if(which_dev == 2){
           if(p->ticks!=0)
         {
            p->passed_ticks++;
            if(p->passed_ticks==p->ticks&&p->flag==0)
            {
              p->flag=1;
              *(p->save_trapframe)=*(p->trapframe);
              p->trapframe->epc=p->handler;
              p->passed_ticks=0;
            }
         }
         yield();
        
        }
      ```

      flag的恢复可以放在syscall中实现，同时由于要求调用sigreturn时，a0的值也被还原，而a0被设置为系统调用返回值的操作也在syscall中进行，因此在syscall中也可进行a0的还原。最终syscall的代码如下：

      ```c
      void
      syscall(void)
      {
        int num;
        struct proc *p = myproc();
      
        num = p->trapframe->a7;
        if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
          // Use num to lookup the system call function for num, call it,
          // and store its return value in p->trapframe->a0
          p->trapframe->a0 = syscalls[num]();
          // restore a0 if sigreturn
          if(num==SYS_sigreturn){
          p->trapframe->a0=p->save_trapframe->a0;
          p->flag=0;
          }
        } else {
          printf("%d %s: unknown sys call %d\n",
                  p->pid, p->name, num);
          p->trapframe->a0 = -1;
        }
      }
      ```

      当num对应sigreturn 的编号时，从save_trapframe中还原a0，并且恢复flag为0。

   
