# Lab 4 ：Multithreading

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

   
