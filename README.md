# Lab 5 ：Multithreading

1. ### Uthread: switching between threads

   本实验要求为用户级线程系统设计上下文切换机制，然后实现。

   #### 代码实现：

   首先为了能够保存上下文，需要在thread结构里加入一个context字段：

   ```c
   struct thread {
     char       stack[STACK_SIZE]; /* the thread's stack */
     int        state;             /* FREE, RUNNING, RUNNABLE */
     struct context context;
   };
   ```

   然后在创建线程时，为了后续线程切换后，切换的线程能够执行在它被创建时传入thread_create的函数，需要将函数地址存储在context的ra中，同时需要将线程的栈的栈顶地址放在sp中，因此thread_create实现如下：

   ```c
   void 
   thread_create(void (*func)())
   {
     struct thread *t;
   
     for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
       if (t->state == FREE) break;
     }
     t->state = RUNNABLE;
     // YOUR CODE HERE
     t->context.ra=(uint64)func;
     t->context.sp=(uint64)t->stack+STACK_SIZE;
   }
   ```

   在实现thread_schedule之前，需要实现thread_switch，它保存当前进程的上下文并且切换到下一个进程的上下文。根据内核中swtch.s的代码,thread_switch代码实现如下：

   ```assembly
   thread_switch:
   	/* YOUR CODE HERE */
   	    sd ra, 0(a0)
           sd sp, 8(a0)
           sd s0, 16(a0)
           sd s1, 24(a0)
           sd s2, 32(a0)
           sd s3, 40(a0)
           sd s4, 48(a0)
           sd s5, 56(a0)
           sd s6, 64(a0)
           sd s7, 72(a0)
           sd s8, 80(a0)
           sd s9, 88(a0)
           sd s10, 96(a0)
           sd s11, 104(a0)
   
           ld ra, 0(a1)
           ld sp, 8(a1)
           ld s0, 16(a1)
           ld s1, 24(a1)
           ld s2, 32(a1)
           ld s3, 40(a1)
           ld s4, 48(a1)
           ld s5, 56(a1)
           ld s6, 64(a1)
           ld s7, 72(a1)
           ld s8, 80(a1)
           ld s9, 88(a1)
           ld s10, 96(a1)
           ld s11, 104(a1)
   
   	ret    /* return to ra */
   
   ```

   随后在thread_schedule中只需添加对thread_switch的调用：

   ```c
   void 
   thread_schedule(void)
   {
     struct thread *t, *next_thread;
   
     /* Find another runnable thread. */
     next_thread = 0;
     t = current_thread + 1;
     for(int i = 0; i < MAX_THREAD; i++){
       if(t >= all_thread + MAX_THREAD)
         t = all_thread;
       if(t->state == RUNNABLE) {
         next_thread = t;
         break;
       }
       t = t + 1;
     }
   
     if (next_thread == 0) {
       printf("thread_schedule: no runnable threads\n");
       exit(-1);
     }
   
     if (current_thread != next_thread) {         /* switch threads?  */
       next_thread->state = RUNNING;
       t = current_thread;
       current_thread = next_thread;
       /* YOUR CODE HERE
        * Invoke thread_switch to switch from t to next_thread:
        * thread_switch(??, ??);
        */
       thread_switch(&t->context,&current_thread->context);
     } else
       next_thread = 0;
   }
   
   ```

   

   

1. ### using threads

   该实验要求通过加锁来防止线程之间对一个哈希表的访问冲突。

   #### 代码实现：

   首先是哈希表的组成：

   ```c
   #define NBUCKET 5
   #define NKEYS 100000
   
   struct entry {
     int key;
     int value;
     struct entry *next;
   };
   struct entry *table[NBUCKET];
   int keys[NKEYS];
   int nthread = 1;
   ```

   然后是表的插入操作：

   ```c
   static void
   insert(int key, int value, struct entry **p, struct entry *n)
   {
     struct entry *e = malloc(sizeof(struct entry));
     e->key = key;
     e->value = value;
     e->next = n;
     *p = e;
   }
   
   static
   void put(int key, int value)
   {
     int i = key % NBUCKET;
     pthread_mutex_lock(&lock[i]);//加上与桶对应的锁
     // is the key already present?
     struct entry *e = 0;
     for (e = table[i]; e != 0; e = e->next) {
       if (e->key == key)
         break;
     }
     if(e){
       // update the existing key.
       e->value = value;
     } else {
       // the new is new.
       insert(key, value, &table[i], table[i]);
     }
     pthread_mutex_unlock(&lock[i]);//释放锁
   
   }
   ```

   这个操作也是访问冲突的发生位置，当两个线程先后进行插入操作而前一个线程操作没有完成而后一个线程就开始进行插入时，两次插入的表头就是相同的，会导致先插入的数据丢失。

   为避免该错误，只需在put函数调用前加锁,如上面代码所示。

   至于为什么不在insert函数调用前加锁，是由于put会先遍历哈希表查找是否存在相应的键，如果两个线程先后进入put函数，第一个线程先查询哈希表，发现没有相应的键，那么它就会在表头插入该键值对，而第二个线程这时如果还在遍历哈希表，它不知道第一个线程已经插入，它也不会找到相应的键，那么它又会执行一次插入操作。这种情况虽然不会导致数据丢失，但是导致了键的重复，这是不符合哈希表的定义的。

2. ### barrier

   该实验需要实现一个barrier，即每个线程都要在barrier处等待所有线程到达barrier之后才能继续运行。

   #### 代码实现：

   运用锁和条件变量的机制，每次线程到来就将线程数增加1，并且该线程进入等待，当最后一个线程到达时，用broadcast唤醒所有的线程。

   ```c
   static void
   barrier()
   {
     // YOUR CODE HERE
     //
     // Block until all threads have called barrier() and
     // then increment bstate.round.
     //
     pthread_mutex_lock(&bstate.barrier_mutex);
     bstate.nthread++;
     if(bstate.nthread==nthread)
     {
     bstate.nthread=0;
     bstate.round++;
     pthread_cond_broadcast(&bstate.barrier_cond);
     }
     else
     {
        pthread_cond_wait(&bstate.barrier_cond,&bstate.barrier_mutex);
     }
     pthread_mutex_unlock(&bstate.barrier_mutex);
   }
   
   ```
