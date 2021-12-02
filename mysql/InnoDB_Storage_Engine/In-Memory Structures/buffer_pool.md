Buffer Pool相关的源代码在buf目录下，主要包括LRU List，Flu List，Double write buffer, 预读预写，Buffer Pool预热，压缩页内存管理等模块

基本概念
Buffer Pool Instance
    大小等于innodb_buffer_pool_size/innodb_buffer_pool_instances，每个instance都有自己的锁，信号量，物理块、链表，各个instance之间没有竞争关系，可以并发读取与写入，且一个page只会被存放在一个固定的Instance中。所有instance的物理块(Buffer chunks)在数据库启动的时候被分配，直到数据库关闭内存才予以释放。当innodb_buffer_pool_size(srv_buf_pool_size)小于1GB时候，innodb_buffer_pool_instances被重置为1，主要是防止有太多小的instance从而导致性能问题。Mysql8.0默认最大支持64个Instance。每个Buffer Pool Instance有一个page hash链表，使用space_id和page_no就能快速找到已经被读入内存的数据页，而不用线性遍历LRU List去查找。注意这个hash表不是InnoDB的自适应哈希，自适应哈希是为了减少Btree的扫描，而page hash是为了避免扫描LRU List。

Buffer Chunks
    Buffer chunks是每个Buffer Pool Instance中实际的物理存储块数组，一个Buffer Pool Instance中有一个或多个chunk，每个chunk的大小默认为128MB，最小为1MB，且这个值在8.0中时可以动态调整生效的。每个Buffer chunk中包含一个buf_block_t的blocks数组（即Page），Buffer chunk主要存储数据页和数据页控制体，blocks数组中的每个buf_block_t是一个数据页控制体，其中包含了一个指向具体数据页的*frame指针，以及具体的控制体buf_page_t，后面在数据结构中详细阐述他们的关系。
包括两部分：数据页和数据页对应的控制体，控制体中有指针指向数据页。Buffer Chunks是最低层的物理块，在启动阶段从操作系统申请，直到数据库关闭才释放。通过遍历chunks可以访问几乎所有的数据页，有两种状态的数据页除外：没有被解压的压缩页(BUF_BLOCK_ZIP_PAGE)以及被修改过且解压页已经被驱逐的压缩页(BUF_BLOCK_ZIP_DIRTY)。此外数据页里面不一定都存的是用户数据，开始是控制信息，比如行锁，自适应哈希等。



Free Page Buffer
Pool中未被使用的Page，位于Free List中。



Clean Page
Buffer Pool中已经使用的Page，但Page记录未被修改，位于LRU List中



Dirty Page
Buffer Pool中已经使用的Page，页面已经被修改，其数据和磁盘上的数据已经不一致。当脏页上的数据写入磁盘后，内存数据和磁盘数据一致，那么该Page 就变成了干净页。脏页 同时存在于LRU 链表和Flush 链表。



Mutex
为保证各个页链表访问时的互斥，Buffer Pool中提供了对几个List的Mutex，如LRU_list_mutex用来保护LRU List的访问，free_list_mutex用来保护Free List的访问，flush_list_mutex用来保护Flush List的访问。

Page_hash 在每个Buffer Pool Instance中都会包含一个独立的Page_hash，其作用主要是为了避免对LRU List的全链表扫描，通过使用space_id和page_no就能快速找到已经被读入Buffer Pool的Page。



Free List
在执行SQL的过程中，每次成功load 页面到内存后，会判断Free 链表的页面是否够用。如果不够用的话，就flush LRU 链表和Flush 链表来释放空闲页。如果够用，就从Free 链表里面删除对应的页面，在LRU 链表增加页面，保持总数不变。



LRU（least recently used） List
LRU链表被分成两部分，一部分是New Sublist(Young 链表)，用来存放经常被读取的页的地址，另外一部分是Old Sublist(Old 链表)，用来存放较少被使用的页面。每部分都有对应的头部 和尾部。

默认情况下

Old 链表占整个LRU 链表的比例是3/8。该比例由innodb_old_blocks_pct控制，默认值是37（3/8*100）。该值取值范围为5~95，为全局动态变量。
当新的页被读取到Buffer Pool里面的时候，和传统的LRU算法插入到LRU链表头部不同，Innodb LRU算法是将新的页面插入到Yong 链表的尾部和Old 链表的头部中间的位置，这个位置叫做Mid Point。
频繁访问一个Buffer Pool的页面，会促使页面往Young链表的头部移动。如果一个Page在被读到Buffer Pool后很快就被访问，那么该Page会往Young List的头部移动，但是如果一个页面是通过预读的方式读到Buffer Pool，且之后短时间内没有被访问，那么很可能在下次访问之前就被移动到Old List的尾部，而被驱逐了。
随着数据库的持续运行，新的页面被不断的插入到LRU链表的Mid Point，Old 链表里的页面会逐渐的被移动Old链表的尾部。同时，当经常被访问的页面移动到LRU链表头部的时候，那些没有被访问的页面会逐渐的被移动到链表的尾部。最终，位于Old 链表尾部的页面将被驱逐。
如果一个数据页已经处于Young 链表，当它再次被访问的时候，只有当其处于Young 链表长度的1/4(大约值)之后，才会被移动到Young 链表的头部。这样做的目的是减少对LRU 链表的修改，因为LRU 链表的目标是保证经常被访问的数据页不会被驱逐出去。


innodb_old_blocks_time 控制的Old 链表头部页面的转移策略。该Page需要在Old 链表停留超过innodb_old_blocks_time 时间，之后再次被访问，才会移动到Young 链表。这么操作是避免Young 链表被那些只在innodb_old_blocks_time时间间隔内频繁访问，之后就不被访问的页面塞满，从而有效的保护Young 链表。

在全表扫描或者全索引扫描的时候，Innodb会将大量的页面写入LRU 链表的Mid Point位置，并且只在短时间内访问几次之后就不再访问了。设置innodb_old_blocks_time的时间窗口可以有效的保护Young List，保证了真正的频繁访问的页面不被驱逐。

innodb_old_blocks_time 单位是毫秒，默认值是1000。调大该值提高了从Old链表移动到Young链表的难度，会促使更多页面被移动到Old 链表，老化，从而被驱逐。

当扫描的表很大，Buffer Pool都放不下时，可以将innodb_old_blocks_pct设置为较小的值，这样只读取一次的数据页就不会占据大部分的Buffer Pool。例如，设置innodb_old_blocks_pct = 5，会将仅读取一次的数据页在Buffer Pool的占用限制为5％。



Flush List
Flush 链表里面保存的都是脏页，也会存在于LRU 链表。
当有页面访被修改的时候，mtr commit时，对应的page进入Flush 链表
如果当前页面已经是脏页，就不需要再次加入Flush list，否则是第一次修改，需要加入Flush 链表
当Page Cleaner线程执行flush操作的时候，从尾部开始scan，将一定的脏页写入磁盘，推进检查点，减少recover的时间
Unzip LRU List
这个链表中存储的数据页都是解压页，也就是说，这个数据页是从一个压缩页通过解压而来的。

Zip Clean List
这个链表只在Debug模式下有，主要是存储没有被解压的压缩页。这些压缩页刚刚从磁盘读取出来，还没来的及被解压，一旦被解压后，就从此链表中删除，然后加入到Unzip LRU List中。



Zip Free
压缩页有不同的大小，比如8K，4K，InnoDB使用了类似内存管理的伙伴系统来管理压缩页。Zip Free可以理解为由5个链表构成的一个二维数组，每个链表分别存储了对应大小的内存碎片，例如8K的链表里存储的都是8K的碎片，如果新读入一个8K的页面，首先从这个链表中查找，如果有则直接返回，如果没有则从16K的链表中分裂出两个8K的块，一个被使用，另外一个放入8K链表中。



数据结构

Buffer Pool主要包含三个核心的数据结构buf_pool_t、buf_block_t和buf_page_t，其定义都在include/buf0buf.h中，简述如下：

but_pool_t 存储Buffer Pool Instance级别的控制信息，例如整个Buffer Pool Instance的mutex，instance_no, page_hash等。还存储了各种逻辑链表的链表根节点。Zip Free这个二维数组也在其中。
buf_block_t 这个就是数据页的控制体，用来描述数据页部分的信息(大部分信息在buf_page_t中)。buf_block_t中第一字段就是buf_page_t，这个不是随意放的，是必须放在第一字段，因为只有这样buf_block_t和buf_page_t两种类型的指针可以相互转换。第二个字段是frame字段，指向真正存数据的数据页。buf_block_t还存储了Unzip LRU List链表的根节点。另外一个比较重要的字段就是block级别的mutex。
buf_page_t 理解为数据页的另外一个控制体，大部分的数据页信息存在其中，例如space_id, page_no, page state, newest_modification，oldest_modification，access_time以及压缩页的所有信息等。压缩页的信息包括压缩页的大小，压缩页的数据指针(真正的压缩页数据是存储在由伙伴系统分配的数据页上)。这里需要注意一点，如果某个压缩页被解压了，解压页的数据指针是存储在buf_block_t的frame字段里。


这里重点阐述下buf_page_t中buf_fix_count和io_fix两个变量，这两个变量主要用来做并发控制，减少mutex加锁的范围。当从buffer pool读取一个数据页时候，会其加读锁，然后递增buf_page_t::buf_fix_count，同时设置buf_page_t::io_fix为BUF_IO_READ，然后即可以释放读锁。后续如果其他线程在驱逐数据页(或者刷脏)的时候，需要先检查一下这两个变量，如果buf_page_t::buf_fix_count不为零且buf_page_t::io_fix不为BUF_IO_NONE，则不允许驱逐(buf_page_can_relocate)。这里的技巧主要是为了减少数据页控制体上mutex的争抢，而对数据页的内容，读取的时候依然要加读锁，修改时加写锁。

struct buf_pool_t { //保存Buffer Pool Instance级别的信息
    ...
    ulint instance_no; //当前buf_pool所属instance的编号
    ulint curr_pool_size; //当前buf_pool大小
    buf_chunk_t *chunks; //当前buf_pool中包含的chunks
    hash_table_t *page_hash; //快速检索已经缓存的Page
    UT_LIST_BASE_NODE_T(buf_page_t) free; //空闲Page链表
    UT_LIST_BASE_NODE_T(buf_page_t) LRU; //Page缓存链表，LRU策略淘汰
    UT_LIST_BASE_NODE_T(buf_page_t) flush_list; //还未Flush磁盘的脏页保存链表
    BufListMutex XXX_mutex; //各个链表的互斥Mutex
    ...
}
 
struct buf_block_t { //Page控制体
    buf_page_t page; //这个字段必须要放到第一个位置，这样才能使得buf_block_t和buf_page_t的指针进行                      转换
    byte *frame; //指向真正存储数据的Page
    BPageLock lock; //buffer frame 读写锁，可视为页锁，控制对page的读写
    BPageMutex mutex; //block级别的mutex，控制block、page内成员变量的读写
    ...
}
 
class buf_page_t {
    ...
    page_id_t id; //page id
    page_size_t size; //page 大小
    ib_uint32_t buf_fix_count; //读写引用计数，用于并发控制
    buf_io_fix io_fix; //page io状态，用于并发控制
    buf_page_state state; //当前Page所处的状态，后续会详细介绍
    lsn_t newest_modification; //当前Page最新修改lsn
    lsn_t oldest_modification; //当前Page最老修改lsn，即第一条修改lsn
    ...
}
buf_page_state标识了每个Page所处的状态，在读取和访问时都会对应不同的状态转换，这是一个枚举类型，一个有八种状态，如果理解了这八种状态以及其之间的转换关系，那么阅读Buffer pool的代码细节就会更加游刃有余。

enum buf_page_state {
  BUF_BLOCK_POOL_WATCH, //Purge使用的
  BUF_BLOCK_ZIP_PAGE, //压缩Page状态
  BUF_BLOCK_ZIP_DIRTY, //压缩页脏页状态
  BUF_BLOCK_NOT_USED, //保存在Free List中的Page
  BUF_BLOCK_READY_FOR_USE, //当调用到buf_LRU_get_free_block获取空闲Page，此时被分配的Page就处于                           这个状态
  BUF_BLOCK_FILE_PAGE, //正常被使用的状态，LRU List中的Page都是这个状态
  BUF_BLOCK_MEMORY, //用于存储非用户数据，比如系统数据的Page处于这个状态
  BUF_BLOCK_REMOVE_HASH //在Page从LRU List和Flush List中被回收并加入Free List时，需要先从                           Page_hash中移除，此时Page处于这个状态
};
BUF_BLOCK_POOL_WATCH
这种类型的page是提供给purge线程用的。InnoDB为了实现多版本，需要把之前的数据记录在undo log中，如果没有读请求再需要它，就可以通过purge线程删除。换句话说，purge线程需要知道某些数据页是否被读取，现在解法就是首先查看page hash，看看这个数据页是否已经被读入，如果没有读入，则获取(启动时候通过malloc分配，不在Buffer Chunks中)一个BUF_BLOCK_POOL_WATCH类型的哨兵数据页控制体，同时加入page_hash但是没有真正的数据(buf_blokc_t::frame为空)并把其类型置为BUF_BLOCK_ZIP_PAGE(表示已经被使用了，其他purge线程就不会用到这个控制体了)，相关函数buf_pool_watch_set，如果查看page hash后发现有这个数据页，只需要判断控制体在内存中的地址是否属于Buffer Chunks即可，如果是表示对应数据页已经被其他线程读入了，相关函数buf_pool_watch_occurred。另一方面，如果用户线程需要这个数据页，先查看page hash看看是否是BUF_BLOCK_POOL_WATCH类型的数据页，如果是则回收这个BUF_BLOCK_POOL_WATCH类型的数据页，从Free List中(即在Buffer Chunks中)分配一个空闲的控制体，填入数据。这里的核心思想就是通过控制体在内存中的地址来确定数据页是否还在被使用。

BUF_BLOCK_ZIP_PAGE
当压缩页从磁盘读取出来的时候，先通过malloc分配一个临时的buf_page_t，然后从伙伴系统中分配出压缩页存储的空间，把磁盘中读取的压缩数据存入，然后把这个临时的buf_page_t标记为BUF_BLOCK_ZIP_PAGE状态(buf_page_init_for_read)，只有当这个压缩页被解压了，state字段才会被修改为BUF_BLOCK_FILE_PAGE，并加入LRU List和Unzip LRU List(buf_page_get_gen)。如果一个压缩页对应的解压页被驱逐了，但是需要保留这个压缩页且压缩页不是脏页，则这个压缩页被标记为BUF_BLOCK_ZIP_PAGE(buf_LRU_free_page)。所以正常情况下，处于BUF_BLOCK_ZIP_PAGE状态的不会很多。前述两种被标记为BUF_BLOCK_ZIP_PAGE的压缩页都在LRU List中。另外一个用法是，从BUF_BLOCK_POOL_WATCH类型节点中，如果被某个purge线程使用了，也会被标记为BUF_BLOCK_ZIP_PAGE。

BUF_BLOCK_ZIP_DIRTY
如果一个压缩页对应的解压页被驱逐了，但是需要保留这个压缩页且压缩页是脏页，则被标记为BUF_BLOCK_ZIP_DIRTY(buf_LRU_free_page)，如果该压缩页又被解压了，则状态会变为BUF_BLOCK_FILE_PAGE。因此BUF_BLOCK_ZIP_DIRTY也是一个比较短暂的状态。这种类型的数据页都在Flush List中。

BUF_BLOCK_NOT_USED
当链表处于Free List中，状态就为此状态。是一个能长期存在的状态。

BUF_BLOCK_READY_FOR_USE
当从Free List中，获取一个空闲的数据页时，状态会从BUF_BLOCK_NOT_USED变为BUF_BLOCK_READY_FOR_USE(buf_LRU_get_free_block)，也是一个比较短暂的状态。处于这个状态的数据页不处于任何逻辑链表中。

BUF_BLOCK_FILE_PAGE
正常被使用的数据页都是这种状态。LRU List中，大部分数据页都是这种状态。压缩页被解压后，状态也会变成BUF_BLOCK_FILE_PAGE。

BUF_BLOCK_MEMORY
Buffer Pool中的数据页不仅可以存储用户数据，也可以存储一些系统信息，例如InnoDB行锁，自适应哈希索引以及压缩页的数据等，这些数据页被标记为BUF_BLOCK_MEMORY。处于这个状态的数据页不处于任何逻辑链表中

BUF_BLOCK_REMOVE_HASH
当加入Free List之前，需要先把page hash移除。因此这种状态就表示此页面page hash已经被移除，但是还没被加入到Free List中，是一个比较短暂的状态。

总体来说，大部分数据页都处于BUF_BLOCK_NOT_USED(全部在Free List中)和BUF_BLOCK_FILE_PAGE(大部分处于LRU List中，LRU List中还包含除被purge线程标记的BUF_BLOCK_ZIP_PAGE状态的数据页)状态，少部分处于BUF_BLOCK_MEMORY状态，极少处于其他状态。前三种状态的数据页都不在Buffer Chunks上，对应的控制体都是临时分配的，InnoDB把他们列为invalid state(buf_block_state_valid)。
在不考虑压缩Page的情况下，buf_page_state的状态转换一般为：





函数、流程分析
1、分配block
函数调用堆栈

buf_LRU_get_free_block
 |    | ==> block = buf_LRU_get_free_only(buf_pool)
 |    | ==> freed = buf_LRU_scan_and_free_block(buf_pool, n_iterations > 0)
 |    | ==> buf_flush_single_page_from_LRU(buf_pool)
 |
函数说明：

buf_LRU_get_free_block 分配一个空闲page，分配动作包含三个函数，
buf_LRU_get_free_only 从free list中分配一个空闲page，如果满足条件，则返回。只能从free list中分配page，若free list为空，需要调用下面的两个函数释放空间，将page加入到free list；
buf_LRU_scan_and_free_block 尝试从LRU链表中淘汰非脏page到free list；
buf_flush_single_page_from_LRU 从LRU链表中刷一个脏页，加入到free list；
buf_block_t *buf_LRU_get_free_block(buf_pool_t *buf_pool) {
 ...
  ut_ad(!mutex_own(&buf_pool->LRU_list_mutex));
loop:
  buf_LRU_check_size_of_non_data_objects(buf_pool);
  ...
  //从free list中获取page空间
  block = buf_LRU_get_free_only(buf_pool);
  ...
  if (buf_pool->try_LRU_scan || n_iterations > 0) {
    //从LRU链表中获取非脏的page，即从LRU链表中驱逐非脏page加入到free list中
    //在从free list中获取block
    freed = buf_LRU_scan_and_free_block(buf_pool, n_iterations > 0);
 
    ...
  }
 
  if (freed) {
    goto loop;
  }
  ...
  //通知page cleaner线程刷脏，休眠一会以获取空闲page空间
  if (!srv_read_only_mode) {
    os_event_set(buf_flush_event);
  }
 
  if (n_iterations > 1) {
    MONITOR_INC(MONITOR_LRU_GET_FREE_WAITS);
    os_thread_sleep(10000);
  }
  //从LRU链表中刷一个脏页
  if (!buf_flush_single_page_from_LRU(buf_pool)) {
    MONITOR_INC(MONITOR_LRU_SINGLE_FLUSH_FAILURE_COUNT);
    ++flush_failures;
  }
  srv_stats.buf_pool_wait_free.add(n_iterations, 1);
  n_iterations++;
  goto loop;
}


2、释放Page加入到Free list
buf_LRU_scan_and_free_block(buf_pool, n_iterations > 0)
|    | ==> //尝试从解压缩页中获取空间,最终还是调用函数buf_LRU_free_page
|    | ==> buf_LRU_free_from_unzip_LRU_list(buf_pool, scan_all)//尝试从解压缩页中获取空间
|    | ==> buf_LRU_free_from_common_LRU_list(buf_pool, scan_all)//从LRU中获取非脏页面(clean page)
|        | ==> //使用lru_scan_itr(Hazard Pointer)遍历LRU链表
|        | ==> for (buf_page_t *bpage = buf_pool->lru_scan_itr.start(); bpage != NULL && !freed &&
                    (scan_all || scanned < BUF_LRU_SEARCH_SCAN_THRESHOLD); ++scanned, bpage = buf_pool->lru_scan_itr.get()) {
|        | ==> accessed = buf_page_is_accessed(bpage);//获取page首次访问时间
|        | ==> freed = buf_LRU_free_page(bpage, true);//从LRU链表中转移一个PAGE到FREE LIST
|            | ==> if (!buf_page_can_relocate(bpage)) exit//判断页面是否有引用或者在IO过程中
|            | ==> bpage->oldest_modification //判断是否脏页
|            | ==> //从LRU链表中删除一个PAGE，如果是BUF_BLOCK_FILE_PAGE类型的PAGE，要在外面调用函数加入FREELIST
|            | ==> buf_LRU_block_remove_hashed(bpage, zip)
|                | ==> buf_LRU_remove_block(bpage);
|                    | ==> buf_LRU_adjust_hp(buf_pool, bpage);//删除page前 调整 bf中hazard pointers
|                    | ==> UT_LIST_REMOVE(LRU, buf_pool->LRU, bpage);//从LRU链表删除节点
|                    | ==> //若page在unzip LRU链表中，需要删除
|                    | ==> buf_unzip_LRU_remove_block_if_needed(bpage);
|                    | ==> buf_LRU_old_adjust_len(buf_pool)//调整LRU_old指针及old部分长度            
|                | ==> HASH_DELETE(buf_page_t, hash, buf_pool->page_hash, fold, bpage);//从HASH表中删除
|            | ==> buf_LRU_block_free_hashed_page((buf_block_t*) bpage);//把Page加入到FREE LIST
|                | ==> buf_LRU_block_free_non_file_page(block);
|                    | ==> //将page加入到free list头部
|                    | ==> UT_LIST_ADD_FIRST(list, buf_pool->free, (&block->page));
|
关键数据结构说明：

BUF_LRU_OLD_MIN_LEN 当LRU链表长度超过这个阀值后，就初始化该结构体的LRU_old对象，LRU链表的old端小于这个阀值时，LRU_old=NULL、LRU_old_len=0；
//从LRU链表释放一个page，如果bpage是压缩页的描述符，则描述符也需要释放
bool buf_LRU_free_page(buf_page_t *bpage, bool zip) {
  ...
  //page是否有引用或io
  if (!buf_page_can_relocate(bpage)) {
    /* Do not free buffer fixed and I/O-fixed blocks. */
    return (false);
  }
  ...
  //压缩页修改过 oldest_modification
  if (zip || !bpage->zip.data) {
    if (bpage->oldest_modification) {
      return (false);
    }
  } else if (bpage->oldest_modification > 0 &&
             buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE) {
    //压缩脏页            
    ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_DIRTY);
    return (false);
 
  } else if (buf_page_get_state(bpage) == BUF_BLOCK_FILE_PAGE) {
    //bpage在unzip_LRU上 需要获取描述符 
    b = buf_page_alloc_descriptor();
    ut_a(b);
  }
  ...
  mutex_exit(block_mutex);
  //获取block锁 及hash lock
  rw_lock_x_lock(hash_lock);
  mutex_enter(block_mutex);
 
  if (!buf_page_can_relocate(bpage) ||
      ((zip || bpage->zip.data == NULL) && bpage->oldest_modification > 0)) {
  not_freed:
    rw_lock_x_unlock(hash_lock);
 
    if (b != NULL) {
      buf_page_free_descriptor(b);
    }
 
    return (false);
 
  } else if (bpage->oldest_modification > 0 &&
             buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE) {
    ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_DIRTY);
 
    goto not_freed;
 
  } else if (b != NULL) {
    new (b) buf_page_t(*bpage);
  }
  ...
  //从LRU链表中删除一个PAGE，如果是BUF_BLOCK_FILE_PAGE类型的PAGE，
  //要在外面调用函数加入FREELIST;如果是BUF_BLOCK_ZIP_PAGE类型的PAGE，
  //block已经归还给buddy allocator
  if (!buf_LRU_block_remove_hashed(bpage, zip, false)) {
    mutex_exit(&buf_pool->LRU_list_mutex);
 
    if (b != NULL) {
      buf_page_free_descriptor(b);
    }
 
    return (true);
  }
  ...
  //BUF_BLOCK_FILE_PAGE已经释放； 如果b！= NULL，则它是一个具有未压缩帧的压缩页面，我们仅对释放未压缩帧感兴趣。
  //必须重新插入压缩的页面描述符到LRU和page_hash（可能还有flush_list）。
  // 如果b == NULL，则它是已释放的常规页面
  if (b != NULL) {
    ...
  }
 
  mutex_exit(&buf_pool->LRU_list_mutex);
  ...
  UNIV_MEM_VALID(((buf_block_t *)bpage)->frame, UNIV_PAGE_SIZE);
  btr_search_drop_page_hash_index((buf_block_t *)bpage);
  UNIV_MEM_INVALID(((buf_block_t *)bpage)->frame, UNIV_PAGE_SIZE);
  ...
  //把Page加入到FREE LIST
  buf_LRU_block_free_hashed_page((buf_block_t *)bpage);
  return (true);
}
 
void buf_LRU_remove_block(buf_page_t *bpage) {
  ...
  //删除page前调整bf中hazard pointers
  buf_LRU_adjust_hp(buf_pool, bpage);
  ...
  //page 是LRU_old时，调整LRU_old指针
  if (bpage == buf_pool->LRU_old) {
    buf_page_t *prev_bpage = UT_LIST_GET_PREV(LRU, bpage);
    buf_pool->LRU_old = prev_bpage;
    buf_page_set_old(prev_bpage, TRUE);
    buf_pool->LRU_old_len++;
  }
 
  //从LRU链表删除page
  UT_LIST_REMOVE(buf_pool->LRU, bpage);
  ...
  //若page在unzip LRU链表中，则删除
  buf_unzip_LRU_remove_block_if_needed(bpage);
 
  //删除page后，若LRU链表长度小于512，LRU_old置NULL，page的old标识置false
  if (UT_LIST_GET_LEN(buf_pool->LRU) < BUF_LRU_OLD_MIN_LEN) {
    for (buf_page_t *bpage = UT_LIST_GET_FIRST(buf_pool->LRU); bpage != NULL;
         bpage = UT_LIST_GET_NEXT(LRU, bpage)) {
      /* This loop temporarily violates the
      assertions of buf_page_set_old(). */
      bpage->old = FALSE;
    }
 
    buf_pool->LRU_old = NULL;
    buf_pool->LRU_old_len = 0;
 
    return;
  }
 
  //若删除的page在LRU old部分，调整old len
  if (buf_page_is_old(bpage)) {
    buf_pool->LRU_old_len--;
  }
 
  //调整bp LRU_old指针及old部分长度
  buf_LRU_old_adjust_len(buf_pool);
}


3、LRU链表刷一个脏页
从LRU链表中找一个page，若是脏页刷脏，从page_hash、LRU list中删除，加入到free list。该函数流程需要持有LRU_list_mutex锁，page操作需要持有block_mutex

buf_flush_single_page_from_LRU(buf_pool)
|    | ==> for (bpage = buf_pool->single_scan_itr.start(), scanned = 0, freed = false;
                bpage != NULL; ++scanned, bpage = buf_pool->single_scan_itr.get()) {
|    | ==> //判断页面是否有修改且是否在IO中，即是否可以替换
|    | ==> if (buf_flush_ready_for_replace(bpage))
|    | ==> buf_LRU_free_page(bpage, true) //从LRU链表中释放一个PAGE到FREE LIST
|    | ==> //判断页面是否修改过且可进行flush操作
|    | ==> if (buf_flush_ready_for_flush(bpage, BUF_FLUSH_SINGLE_PAGE)
|    | ==> //刷脏页（同步IO）
|    | ==> buf_flush_page(buf_pool, bpage, BUF_FLUSH_SINGLE_PAGE, true);
|        | ==> buf_page_set_io_fix(bpage, BUF_IO_WRITE);
|        | ==> buf_dblwr_flush_buffered_writes();//如果是BUF_FLUSH_LIST，此处有三个且条件
|        | ==> buf_flush_write_block_low(bpage, flush_type, sync);//同步写一个bf page
|            | ==> //关闭DBLWR场景
|            | ==> fil_io(request, sync, bpage->id, bpage->size, 0, bpage->size.physical(), frame, bpage)
|            | ==> if (flush_type == BUF_FLUSH_SINGLE_PAGE) buf_dblwr_write_single_page(bpage, sync);
|                | ==> //持久化dblwr
|                | ==> fil_flush(TRX_SYS_SPACE);
|                | ==> //持久化数据页
|                | ==> buf_dblwr_write_block_to_datafile(bpage, sync);
|                    | ==> //调IO接口
|                    | ==> fil_io(flags, sync, buf_block_get_space(block), 0,
                                    buf_block_get_page_no(block), 0, UNIV_PAGE_SIZE,
                                    (void*) block->frame, (void*) block)
|                        | ==> //调AIO接口
|                        | ==> ret = os_aio(type, mode | wake_later, node->name, node->handle, buf, offset, len, node, message);
|                            | ==> os_aio_func
|                                | ==> //如果同步IO，读调用os_file_read_func，写调用os_file_write_func
|                                | ==> //如果异步IO，走异步IO系统
|                                | ==> os_aio_simulated_wake_handler_thread
|                        | ==> //如果是同步IO，进行IO_WAIT
|                        | ==> fil_node_complete_io(node, fil_system, type);
|            | ==> //其他场景，通过dblwr持久化数据页
|            | ==> buf_dblwr_add_to_batch(bpage);
|            | ==> //同步IO场景，执行IO完成后操作
|            | ==> if (sync) {
|            | ==> fil_flush(buf_page_get_space(bpage));
|            | ==> buf_page_io_complete(bpage);
|                | ==> buf_flush_write_complete(bpage)
|                    | ==> //从FLU链表中移出
|                    | ==> buf_flush_remove(bpage);
|            | ==> }
|    | ==> }


trx_t::flush_observer
flush_observer可以认为是一个标记，当某种操作完成时，对于带这种标记的page(buf_page_t::flush_observer)，需要保证完全刷到磁盘上。

该变量主要解决早期5.7版本建索引耗时太久的bug#74472：为表增加索引的操作非常慢，即使表上没几条数据。原因是InnoDB在5.7版本修正了建索引的方式，采用自底向上的构建方式，并在创建索引的过程中关闭了redo，因此在做完加索引操作后，必须将其产生的脏页完全写到磁盘中，才能认为索引构建完毕，所以发起了一次完全的checkpoint，但如果buffer pool中存在大量脏页时，将会非常耗时。
为了解决这一问题，引入了flush_observer，在建索引之前创建一个FlushObserver并分配给事务对象(trx_set_flush_observer)，同时传递给BtrBulk::m_flush_observer。在构建索引的过程中产生的脏页，通过mtr_commit将脏页转移到flush_list上时，顺便标记上flush_observer（add_dirty_page_to_flush_list —> buf_flush_note_modification）。
当做完索引构建操作后，由于bulk load操作不记redo，需要保证DDL产生的所有脏页都写到磁盘，因此调用FlushObserver::flush，将脏页写盘（buf_LRU_flush_or_remove_pages）。在做完这一步后，才开始apply online DDL过程中产生的row log(row_log_apply)。如果DDL被中断（例如session被kill），也需要调用FlushObserver::flush，将这些产生的脏页从内存移除掉，无需写盘。



4、 buf_LRU_flush_or_remove_pages

清空指定表空间LRU页，分成了两个基本分支，一条是BUF_REMOVE_ALL_NO_WRITE方式，一条是BUF_REMOVE_FLUSH_NO_WRITE，这两个值是枚举元素enum buf_remove_t中的对象。

BUF_REMOVE_ALL_NO_WRITE代表的就是清除全部缓存页的动作，函数buf_LRU_drop_page_hash_for_tablespace用于清除全部缓存页hash入口，buf_LRU_remove_all_pages函数完成LRU链表中给定表空间页的清理动作，实际上该函数里面隐藏了几个细节，会根据buf_page_t的oldest_modification值是否为0决定是否调用buf_flush_remove函数（flush模块的清除函数），如果oldest_modification！=0就说明做出过修改需要进行flush链表的清理；否则直接调用buf_LRU_block_remove_hashed_page

BUF_REMOVE_FLUSH_NO_WRITE，仅清理flush链表中对应要删除的表空间的缓存页

bool buf_flush_single_page_from_LRU(buf_pool_t *buf_pool) {
  ...
  //获取LRU_list_mutex
  mutex_enter(&buf_pool->LRU_list_mutex);
 
  for (bpage = buf_pool->single_scan_itr.start(), scanned = 0, freed = false;
       bpage != NULL; ++scanned, bpage = buf_pool->single_scan_itr.get()) {
    ...
    //获取页锁 block_mutex
    block_mutex = buf_page_get_mutex(bpage);
    mutex_enter(block_mutex);
    //LRU上有可驱逐的非脏页(clean page)
    if (buf_flush_ready_for_replace(bpage)) {
      if (buf_LRU_free_page(bpage, true)) {
        freed = true;
        break;
      } else {
        mutex_exit(block_mutex);
      }
 
    } else if (buf_flush_ready_for_flush(bpage, BUF_FLUSH_SINGLE_PAGE)) {
      //同步刷LRU中的一个脏页，该函数在page clean中异步调用，是修改的重点
      freed = buf_flush_page(buf_pool, bpage, BUF_FLUSH_SINGLE_PAGE, true);
      ...
    }
    ...
  }
  ...
  return (freed);
}
//从bf刷一个page到file,sync标识同步、异步IO
ibool buf_flush_page(buf_pool_t *buf_pool, buf_page_t *bpage,
                     buf_flush_t flush_type, bool sync) {
  ...
  block_mutex = buf_page_get_mutex(bpage);
  ...
  //设置是否需要flush
  if (!is_uncompressed) {
    flush = TRUE;
    rw_lock = NULL;
  } else if (!(no_fix_count || flush_type == BUF_FLUSH_LIST) ||
             (!no_fix_count && srv_shutdown_state <= SRV_SHUTDOWN_CLEANUP &&
              fsp_is_system_temporary(bpage->id.space()))) {
    flush = FALSE;
  } else {
    rw_lock = &reinterpret_cast<buf_block_t *>(bpage)->lock;
    if (flush_type != BUF_FLUSH_LIST) {
      flush = rw_lock_sx_lock_nowait(rw_lock, BUF_IO_WRITE);
    } else {
      /* Will SX lock later */
      flush = TRUE;
    }
  }
 
  if (flush) {
    //flush 需要持有flush_state_mutex
    mutex_enter(&buf_pool->flush_state_mutex);
    //设置BUF_IO_WRITE
    buf_page_set_io_fix(bpage, BUF_IO_WRITE);
 
    buf_page_set_flush_type(bpage, flush_type);
 
    if (buf_pool->n_flush[flush_type] == 0) {
      os_event_reset(buf_pool->no_flush[flush_type]);
    }
 
    ++buf_pool->n_flush[flush_type];
 
    if (bpage->oldest_modification > buf_pool->max_lsn_io) {
      buf_pool->max_lsn_io = bpage->oldest_modification;
    }
    //非系统临时表空间，记录page id表示在track_page_lsn后修改过的page。
    //主要针对redo 归档会创建一个后台线程archiver_thread()，
    //将内存记录的page id flush到disk上
    if (!fsp_is_system_temporary(bpage->id.space()) &&
        buf_pool->track_page_lsn != LSN_MAX) {
      page_t *frame;
      lsn_t frame_lsn;
 
      frame = bpage->zip.data;
 
      if (!frame) {
        frame = ((buf_block_t *)bpage)->frame;
      }
      frame_lsn = mach_read_from_8(frame + FIL_PAGE_LSN);
 
      arch_page_sys->track_page(bpage, buf_pool->track_page_lsn, frame_lsn,
                                false);
    }
 
    mutex_exit(&buf_pool->flush_state_mutex);
 
    mutex_exit(block_mutex);
 
    if (flush_type == BUF_FLUSH_SINGLE_PAGE) {
      mutex_exit(&buf_pool->LRU_list_mutex);
    }
 
    if (flush_type == BUF_FLUSH_LIST && is_uncompressed &&
        !rw_lock_sx_lock_nowait(rw_lock, BUF_IO_WRITE)) {
      if (!fsp_is_system_temporary(bpage->id.space())) {
        //非系统临时表空间，避免死锁，需要刷一次dblwr buffer缓存残留数据
        buf_dblwr_flush_buffered_writes();
      } else {
        //系统临时表空间 批量flush dblwr buffer数据 
        buf_dblwr_sync_datafiles();
      }
 
      rw_lock_sx_lock_gen(rw_lock, BUF_IO_WRITE);
    }
    //通知flush_observer刷脏（即创建索引流程不记录redo log，
    //在索引创建完成后保证DDL产生的所有脏页都写到磁盘，调用FlushObserver::flush，
    //将脏页写盘（buf_LRU_flush_or_remove_pages）
    if (bpage->flush_observer != NULL) {
      bpage->flush_observer->notify_flush(buf_pool, bpage);
    }
 
    //写一次page buffer
    buf_flush_write_block_low(bpage, flush_type, sync);
  }
 
  return (flush);
}


5、 buf_page_get_gen
从buffer pool读取一个page，这里需介绍下Page_fetch mode这个参数，表示读取的方式，这是一个强制枚举类型，共有七种，其中前三种使用比较多，类型意义如下

NORMAL 默认获取数据页方式，如果page不在bp中，则从磁盘读取数据页；如果在bp中，判断是否需要加入到LRU List的yong区域以及判断是否需要进行线性预读。如果是读取则加读锁，修改则加写锁。
SCAN 类似Normal，是大范围扫描的获取
IF_IN_POOL 只在bp中查找这个数据页，如果在则判断是否要把它加入到young区域中以及判断是否需要进行线性预读。如果不在则直接返回空。加锁方式与NORMAL类似。
PEEK_IF_IN_POOL 与IF_IN_POOL类似，即使满足条件也不会加入到LRU List的yong区域以及线性预读
NO_LATCH 对数据页的读写修改都不加锁，其他类似NORMAL
IF_IN_POOL_OR_WATCH
POSSIBLY_FREED
buf_page_get_gen
|    |==> fetch.single_page()
|        |==> {//下面是一个死循环，获取一个page成功后才跳    
|        |==> if (static_cast<T *>(this)->get(block) == DB_NOT_FOUND) exit //Buf_fetch_normal::get(buf_block_t *&block)
|            |==> buf_block_t *Buf_fetch<T>::lookup() //page 在buf pool中
|                |==> 从hash table中获取一个page
|                |==>  buf_page_hash_get_low(m_buf_pool, m_page_id));
|            |==> read_page(); //page 不在buf pool
|        |==> (check_state(block)
|            |==> switch (buf_block_get_state(block)) { //page状态检查
|            |==> case BUF_BLOCK_FILE_PAGE //正常的在LRU中的Page
|            |==> //如果该Page正处于被Flush的状态
|            |==> if (m_is_temp_space && buf_page_get_io_fix_unlocked(bpage) != BUF_IO_NONE)
|            |==> }//switch 结束
|        |==> } //死循环


buf_page_get_gen函数堆栈

buf_page_get_gen
|    |==> fetch.single_page()
|        |==> {//下面是一个死循环，获取一个page成功后才跳    
|        |==> if (static_cast<T *>(this)->get(block) == DB_NOT_FOUND) exit //Buf_fetch_normal::get(buf_block_t *&block)
|            |==> buf_block_t *Buf_fetch<T>::lookup() //page 在buf pool中
|                |==> 从hash table中获取一个page
|                |==>  buf_page_hash_get_low(m_buf_pool, m_page_id));
|            |==> read_page(); //page 不在buf pool
|        |==> (check_state(block)
|            |==> switch (buf_block_get_state(block)) { //page状态检查
|            |==> case BUF_BLOCK_FILE_PAGE //正常的在LRU中的Page
|            |==> //如果该Page正处于被Flush的状态
|            |==> if (m_is_temp_space && buf_page_get_io_fix_unlocked(bpage) != BUF_IO_NONE)
|            |==> }//switch 结束
|        |==> } //死循环
