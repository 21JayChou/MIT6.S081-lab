## Lab 6: File System

1. ### Large Files

   实验要求实现一个doubly-indirect块来扩充文件的最大大小到65803，使得能够对大文件进行存储。

   #### 代码实现：

   为了实现题目要求，需要先将NDIRECT改为11，表示直接数据块的个数为11，然后dinode和inode结构体中的addrs数组的大小改为NDIRECT+2，第NDIRECT位保存singly-indirect块的地址，第NDIRECT+1位保存doubly-indirect块的地址。同时MAXFILE大小也需改为65803。

   ```c
   #define NDIRECT 11
   #define NINDIRECT (BSIZE / sizeof(uint))
   #define MAXFILE (NDIRECT + NINDIRECT+NINDIRECT*NINDIRECT)
   
   // On-disk inode structure
   struct dinode {
     short type;           // File type
     short major;          // Major device number (T_DEVICE only)
     short minor;          // Minor device number (T_DEVICE only)
     short nlink;          // Number of links to inode in file system
     uint size;            // Size of file (bytes)
     uint addrs[NDIRECT+2];   // Data block addresses
   };
   
   ```

   ```c
   struct inode {
     uint dev;           // Device number
     uint inum;          // Inode number
     int ref;            // Reference count
     struct sleeplock lock; // protects everything below here
     int valid;          // inode has been read from disk?
   
     short type;         // copy of disk inode
     short major;
     short minor;
     short nlink;
     uint size;
     uint addrs[NDIRECT+2];
   };
   ```

   随后需要对bmap进行修改。bmap根据inode(ip)查询传入的数据块的块号(bn)在磁盘上的块地址，并且若对应块不存在则会为其分配一个块。在初始的bmap中，若块号小于NDIRECT，则直接从ip->addrs中查询地址。若块号大于NDIRECT，则使用bread返回一个包含singly-indirect块数据的buf，然后从该buf的数据中读取对应的块地址。

   那么要实现bigfile，当bn大于NINDIRECT+NDIRECT时，需要首先用bread返回包含doubly-indirect块数据的buf,该buf中包含的是单级间接块。然后用bn大于NINDIRECT+NDIRECT的部分对NINDIRECT进行除法取整，决定从哪个单级间接块中读取数据，并且再次使用bread返回包含该块数据的buf，最后再用bn大于NINDIRECT+NDIRECT的部分对NINDIRECT进行模运算，决定最终读取的地址。代码实现如下：

   ```c
      bn -=NINDIRECT;
      if(bn<NINDIRECT*NINDIRECT)
      {
         if((addr = ip->addrs[NDIRECT+1]) == 0){
           addr = balloc(ip->dev);
           if(addr == 0)
           return 0;
           ip->addrs[NDIRECT+1] = addr;
         }
         bp1 = bread(ip->dev, addr);
         a = (uint*)bp1->data;
         if((addr = a[bn/NINDIRECT])==0){
           addr = balloc(ip->dev);
           if(addr == 0)return 0;
         if(addr){
           a[bn/NINDIRECT] = addr;
           log_write(bp1);
         }
         }
         bp2 = bread(ip->dev, addr);
         a=(uint*)bp2->data;
         if((addr = a[bn%NINDIRECT])== 0){
           addr = balloc(ip->dev);
         if(addr){
           a[bn%NINDIRECT] = addr;
           log_write(bp2);
         }
         }
         brelse(bp2);
         brelse(bp1);
         return addr;
      }
   
   ```

   最后是需要在itrunc中实现对doubly-indirect块的释放。释放过程与查找数据过程类似，先根据间接块找到所有的数据块进行释放，再释放单级块，最后释放doubly-indirect块。

   代码如下：

   ```c
     if(ip->addrs[NDIRECT+1]){
      bp1 = bread(ip->dev,ip->addrs[NDIRECT+1]);
      a = (uint*)bp1->data;
      for(i=0; i< NINDIRECT; i++)
      {
        if(a[i]){
         bp2=bread(ip->dev,a[i]);
         uint* a1=(uint*)bp2->data;
         for(j = 0;j< NINDIRECT;j++){
           if(a1[j])
           bfree(ip->dev,a1[j]);
         }
         brelse(bp2);
         bfree(ip->dev,a[i]);
        }
      }
      brelse(bp1);
      bfree(ip->dev,ip->addrs[NINDIRECT+1]);
      ip->addrs[NINDIRECT+1]=0;
     }
   ```

2. ### Symbolic links

   该实验要求实现一个系统调用来建立文件间的符号连接。

   #### 代码实现：

   为了实现一个系统调用，首先在user.h中添加函数声明，并且在user.pl中添加entry，然后在syscall.h中添加一个系统调用号。

   根据提示需要添加一个新的文件类型T_SYMLINK:

   ```c
   #define T_DIR     1   // Directory
   #define T_FILE    2   // File
   #define T_DEVICE  3   // Device
   #define T_SYMLINK 4   // Symbolic link
   ```

   在fcntl.h中添加一个新的标记，O_NOFOLLOW,表示该文件若是T_SYMLINK类型，不去追踪它所连接的文件。为了不与其他标志出现冲突，将其值设为0x800。

   ```c
   #define O_NOFOLLOW 0x800
   ```

   然后是实现函数sys_symlink。用户会传入两个参数target和path,两个都是字符串类型，所以都使用argstr从寄存器中读取。为了创建从path到target的一个symbolic link，可以用create函数为path创建一个inode，使用writei函数向该inode写入target的路径名。代码如下：

   ```c
   uint64
   sys_symlink(void)
   {
     char target[MAXPATH],path[MAXPATH];
     struct inode *ip;
     if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0)
       return -1;
     begin_op();
     if((ip=create(path,T_SYMLINK,0,0)) == 0){
       end_op();
       return -1;
     }
     writei(ip,0,(uint64)target,0,MAXPATH);
     iunlockput(ip);
     end_op();
     return 0;
   }
   
   ```

   然后需要修改sys_open函数。当打开的inode(ip)的type为T_SYMLINK并且O_NOFOLLOW标记位为0时，应该使用readi读取ip中的数据到path中，并且根据该path使用namei函数更新ip。当ip->type仍然为T_SYMLINK时，重复以上步骤知道ip->type不为T_SYMLINK。同时记录一个变量depth，将其初始化为0。以上步骤每进行一次将其加一，当depth大于10时终止循环并退出sys_open函数，防止symbolic link 连接的文件形成的环导致的死循环。代码如下：

   ```c
       ilock(ip);
       int depth=0;
       while(ip->type==T_SYMLINK&&!(omode & O_NOFOLLOW))
     {
         readi(ip,0,(uint64)path,0,MAXPATH);
         iunlockput(ip);
          if((ip = namei(path)) == 0){
         end_op();
         return -1;
       }
       ilock(ip);
         depth++;
         if(depth>10){
         end_op();
         return -1;
         }
     }
       if(ip->type == T_DIR && omode != O_RDONLY){
         iunlockput(ip);
         end_op();
         return -1;
       }
     }
   
   ```

   最后在syscall函数调用数组中添加sys_symlink:

   ```c
   [SYS_symlink] sys_symlink,
   ```

   
