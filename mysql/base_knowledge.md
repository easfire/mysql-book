Mysql's World

#### 学习阿里月报的大神们文章的同时，也整理一些自己的读书笔记
http://mysql.taobao.org/monthly/2017/05/01/
http://mysql.taobao.org/monthly/2020/08/06/
http://mysql.taobao.org/monthly/2016/02/01/


## 为什么要使用如此复杂的内存操作
用户对数据库的最基本要求就是能高效的读取和存储数据，但是读写数据都涉及到与低速的设备交互，为了弥补两者之间的速度差异，所有数据库都有缓存池，用来管理相应的数据页，提高数据库的效率，当然也因为引入了这一中间层，数据库对内存的管理变得相对比较复杂


## 事务ACID: 

   ### 原子性: (整个事务要么全部成功，要么全部失败)
	指的是整个事务要么全部成功，要么全部失败，对InnoDB来说，只要client收到server发送过来的commit成功报文，那么这个事务一定是成功的。
	如果收到的是rollback的成功报文，那么整个事务的所有操作一定都要被回滚掉，就好像什么都没执行过一样。
	另外，如果连接中途断开或者server crash事务也要保证会滚掉。InnoDB通过undolog保证rollback的时候能找到之前的数据。

	
   ### 一致性: (保证不会读到中间状态的数据)
	指的是在任何时刻，包括数据库正常提供服务的时候，数据库从异常中恢复过来的时候，数据都是一致的，保证不会读到中间状态的数据。
	在InnoDB中，主要通过crash recovery和double write buffer的机制保证数据的一致性。

   ### 隔离性: 
    指的是多个事务可以同时对数据进行修改，但是相互不影响。InnoDB中，依据不同的业务场景，有四种隔离级别可以选择。
	默认是RR隔离级别，因为相比于RC，InnoDB的 RR性能更加好。

   ### 持久性: 
    值的是事务commit的数据在任何情况下都不能丢。在内部实现中，InnoDB通过redolog保证已经commit的数据一定不会丢失。


多版本控制: 


	指的是一种提高并发的技术。

	最早的数据库系统，只有读读之间可以并发，读写，写读，写写都要阻塞。

	引入多版本之后，只有写写之间相互阻塞，其他三种操作都可以并行，这样大幅度提高了InnoDB的并发度。

	在内部实现中，与Postgres在数据行上实现多版本不同，InnoDB是在undolog中实现的，通过undolog可以找回数据的历史版本。
	找回的数据历史版本可以提供给用户读 (按照隔离级别的定义，有些读请求只能看到比较老的数据版本)，也可以在回滚的时候覆盖数据页上的数据。

	在InnoDB内部中，会记录一个全局的活跃读写事务数组，其主要用来判断事务的可见性。 trx_sys_t->rw_trx_list


回滚段: 


	可以理解为数据页的修改链，链表最前面的是最老的一次修改，最后面的最新的一次修改，从链表尾部逆向操作可以恢复到数据最老的版本。

	在InnoDB中，与之相关的还有

	undo tablespace, 
	undo segment, 
	undo slot, 
	undo page,
	undo log. 这几个概念

	undo log是最小的粒度，所在的数据页称为undo page，然后若干个undo page构成一个undo slot。

	一个事务最多可以有两个undo slot，
		一个是insert undo slot，用来存储这个事务的insert undo，里面主要记录了主键的信息，方便在回滚的时候快速找到这一行。
		一个是update undo slot，用来存储这个事务delete/update产生的undo，里面详细记录了被修改之前每一列的信息，便于在读请求需要的时候构造。

	1024个undo slot构成了一个undo segment。

	然后若干个undo segment 构成了undo tablespace。



历史链表: 


	insert undo可以在事务提交/回滚后直接删除，没有事务会要求查询新插入数据的历史版本，
	但是update undo则不可以，因为其他读请求可能需要使用update undo构建之前的历史版本。

	因此，在事务提交的时候，会把update undo加入到一个全局链表(history list)中，链表按照事务提交的顺序排序，保证最先提交的事务的update undo在前面，这样Purge线程就可以从最老的事务开始做清理。

	这个链表如果太长说明有很多记录没被彻底删除，也有很多undolog没有被清理，这个时候就需要去看一下是否有个长事务没提交导致Purge线程无法工作。

	在InnoDB具体实现上，history list其实只是undo segment维度的，全局的history list采用最小堆来实现，最小堆的元素是某个undo segment中最小事务no对应的undopage。当这个undolog被Purge清理后，通过history list找到次小的，然后替换掉最小堆元素中的值，来保证下次Purge的顺序的正确性。


回滚点: 


	又称为savepoint，事务回滚的时候可以指定回滚点，这样可以保证回滚到指定的点，而不是回滚掉整个事务，对开发者来说，这是一个强大的功能。
	在InnoDB内部实现中，每打一个回滚点，其实就是保存一下当前的 undo_no，回滚的时候直接回滚到这个undo_no点就可以了。



trx_t
storage/innobase/include/trx0trx.h

   trx_t: ***整个结构体每个连接持有一个，也就是在创建连接后执行第一个事务开始，整个结构体就被初始化了***，后续这个连接的所有事务一直复用里面的数据结构，直到这个连接断开。

	1、同时，事务启动后，就会把这个结构体加入到全局事务链表中(trx_sys->mysql_trx_list)，所有的事务都在 mysql_trx_list 链表内。
	2、如果是读写事务，还会加入到全局读写事务链表中(trx_sys->rw_trx_list)，活跃或者已提交到内存的事务，都在这个链表内，按trxid 排序，大号事务在前；
	回滚事务一定在这个链表。
	3、在事务提交的时候，还会加入到全局提交事务链表中(trx_sys->serialisation_list)。

	state字段记录了事务四种状态:TRX_STATE_NOT_STARTED, TRX_STATE_ACTIVE, TRX_STATE_PREPARED, TRX_STATE_COMMITTED_IN_MEMORY。
	
	这里有两个字段值得区分一下，分别是id和no字段。
	1、id是在事务刚创建的时候分配的(只读事务永远为0，读写事务通过一个全局id产生器产生，非0)，目的就是为了区分不同的事务(只读事务通过指针地址来区分)
	2、no字段是在事务提交前，通过同一个全局id生产器产生的，主要目的是为了确定事务提交的顺序，保证加入到history list中的update undo有序，方便purge线程清理。
	3、此外，trx_t结构体中还有自己的read_view用来表示当前事务的可见范围。
	4、分配的insert undo slot和update undo slot。
	5、如果是只读事务，read_only也会被标记为true。




trx_sys_t
storage/innobase/include/trx0sys.h

   ***这个结构体用来维护系统的事务信息，全局只有一个，在数据库启动的时候初始化。***

	1、max_trx_id，这个字段表示系统当前还未分配的最小事务id，如果有一个新的事务，直接把这个值作为新事务的id，然后这个字段递增即可。
	2、rw_trx_ids，这个是一个数组，里面存放着当前所有活跃的读写事务id，当需要开启一个readview的时候，就从这个字段里面拷贝一份，用来判断记录的对事务的可见性。
	3、rw_trx_list，这个主要是用来存放当前系统的所有读写事务，包括活跃的和已经提交的事务。按照事务id排序，此外，奔溃恢复后产生的事务和系统的事务也放在上面。
	4、mysql_trx_list，这里面存放所有用户创建的事务，系统的事务和奔溃恢复后的事务不会在这个链表上，但是这个链表上可能会有还没开始的用户事务。
	5、serialisation_list，按照事务no(trx_t->no) 排序的，当前活跃的读写事务。
	6、trx_rseg_t* rseg_array[TRX_SYS_N_RSEGS]，这个指向系统所有可以用的回滚段(undo segments)，当某个事务需要回滚段的时候，就从这里分配。
	7、rseg_history_len， 所有提交事务的update undo的长度，也就是上文提到的历史链表的长度（trx_rseg_t->update_undo_list），
	   具体的update undo链表是存放在这个undo log中以文件指针的形式管理起来。
	8、MVCC 通过MVCC管理当前系统所有的ReadView 所有开启的readview的事务都会把自己的readview放在这个上面，按照事务no排序。



ReadView: 

	
	InnDB为了判断某条记录是否对当前事务可见，需要对此记录进行可见性判断，这个结构体就是用来辅助判断的。
	每个连接的trx_t 里面都有一个readview，在事务需要一致性的读时候(不同隔离级别不同)，会被初始化，在读结束的时候会释放(缓存)。

	m_low_limit_no，这个主要是给purge线程用，readview创建的时候，会把当前最小的提交事务id赋值给 low_limit_no，这样Purge线程就可以把所有已经提交的事务的undo日志给删除。

	m_low_limit_id, 所有大于等于此值的记录都不应该被此readview看到，可以理解为high water mark。

	m_up_limit_id, 所有小于此值的记录都应该被此readview看到，可以理解为low water mark。

	m_ids, 这是一个数组，里面存了readview创建时候 活跃的全局读写事务id，除了事务自己做的变更外，此readview应该看不到 m_ids中事务所做的变更。

	m_view_list，每个readview都会被加入到trx_sys中的全局readview链表中。



trx_purge_t: 

	
   ***Purge线程使用的结构体，全局只有一个，在系统启动的时候初始化。***
   
	view，是一个readview，Purge线程不会尝试删除所有大于 view->low_limit_no 的undolog。

	limit，所有小于这个值的 undolog都可以被 truncate掉，因为标记的日志已经被删除且不需要用他们构建之前的历史版本。

	此外，还有rseg，page_no, offset，hdr_page_no, hdr_offset 这些字段，主要用来保存最后一个还未被purge的undolog。


trx_rseg_t: 
	
	undo segment 内存中的结构体。每个undo segment都对应一个。
	update_undo_list 表示已经被分配出去的正在使用的update undo链表，
	insert_undo_list 表示已经被分配出去的正在使用的insert undo链表。
	update_undo_cached和insert_undo_cached表示缓存起来的undo链表，主要为了快速使用。
	last_page_no, last_offset, last_trx_no, last_del_marks表示这个undo segment中最后没有被 Purge的undolog。



事务启动：

b innobase_trx_allocate (storage/innobase/handler/ha_innodb.cc)

	在开始第一个事务前，建立连接时，在InnoDB层会调用函数innobase_trx_allocate分配和初始化trx_t对象

b trx_set_rw_mode (storage/innobase/trx/trx0trx.cc)

	只有接受到修改数据的SQL后，才调用 trx_set_rw_mode 函数把只读事务提升为读写事务
	(InnoDB接收到才行。因为在 start transaction read only模式下，DML/DDL都被Serve层挡掉了) 

b trx_start_low (storage/innobase/trx/trx0trx.cc)

	如果当前的语句只是一条只读语句，则先以只读事务的形式开启事务，否则按照读写事务的形式，这就需要分配事务id，分配回滚段等。


事务提交:

b innobase_commit 
<TODO>



### UNDO LOG
<为什么喜欢跟undo log，因为每次启动mysql，都会初始化到undolog的Segment创建的断点。。>

 mysql启动， 检查需要操作的undolog，
	b innobase_init
		b innobase_start_or_create_for_mysql
			b recv_recovery_rollback_active 


1、rseg0预留在系统表空间ibdata中;
2、rseg 1~rseg 32这32个回滚段存放于临时表的系统表空间中 (ibtmp1);
3、rseg33~ 则根据配置存放到独立undo表空间中（如果没有打开独立Undo表空间，则存放于ibdata中）



# 初始化innodb时，会分配undo table space 空间

		// 创建 table space
		b srv_undo_tablespaces_init
			b srv_undo_tablespace_create (初始化数据目录已经创建好)

		// 分配 rollback segment 回滚段
		// 事务默认都为只读事务，遇到update/insert语句，会通过 trx_set_rw_mode 转换为读写事务
		b trx_set_rw_mode
			b trx_assign_rseg_low
				b get_next_redo_rseg

		// 分配 或者使用 回滚段 undo slot
		b trx_undo_report_row_operation
			b trx_undo_assign_undo
				b trx_undo_create
					b trx_undo_seg_create

		// 分配 或者写使用 undo log 
		b trx_undo_report_row_operation
			b trx_undo_page_report_insert
			b trx_undo_page_report_modify


b srv_undo_tablespaces_init
b fsp_header_init
b buf_LRU_add_block_low

run --defaults-file=/data/mysql57/my.cnf


FIL_PAGE_SPACE_OR_CHKSUM
FIL_PAGE_OFFSET
FIL_PAGE_PREV
FIL_PAGE_NEXT
FIL_PAGE_LSN
FIL_PAGE_TYPE
FIL_PAGE_FILE_FLUSH_LSN
FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID


FIL_PAGE_DATA


TRX_RSEG_SLOT_PAGE_NO
TRX_RSEG_SLOT_SIZE
TRX_RSEG
TRX_RSEG_MAX_SIZE
TRX_RSEG_HISTORY_SIZE
TRX_RSEG_HISTORY (FLST_BASE_NODE_SIZE 16)
TRX_RSEG_FSEG_HEADER (FSEG_HEADER_SIZE 10)
TRX_RSEG_UNDO_SLOTS (TRX_RSEG_SLOT_SIZE(4)xn)




1、The FIX Rules
修改一个页需要获得该页的x-latch

访问一个页是需要获得该页的s-latch或者x-latch

持有该页的latch直到修改或者访问该页的操作完成

2、Write-Ahead Log
持久化一个数据页之前，必须先将内存中相应的日志页持久化
每个页有一个LSN,每次页修改需要维护这个LSN,当一个页需要写入到持久化设备时，要求内存中小于该页LSN的日志先写入到持久化设备中

3、Force-log-at-commit
一个事务可以同时修改了多个页，Write-AheadLog单个数据页的一致性，无法保证事务的持久性
Force -log-at-commit要求当一个事务提交时，其产生所有的mini-transaction日志必须刷到持久设备中
这样即使在页数据刷盘的时候宕机，也可以通过日志进行redo恢复


b fseg_create
b fseg_alloc_free_page_low



#0  buf_LRU_add_block_low (bpage=0x7fff7adf6828, old=0) at /data/mysql-5.7.35/storage/innobase/buf/buf0lru.cc:1782
#1  0x0000000001b8a15a in buf_LRU_add_block (bpage=0x7fff7adf6828, old=0) at /data/mysql-5.7.35/storage/innobase/buf/buf0lru.cc:1822
#2  0x0000000001b6a115 in buf_page_create (page_id=..., page_size=..., mtr=0x7fffffff3f70) at /data/mysql-5.7.35/storage/innobase/buf/buf0buf.cc:5377
#3  0x0000000001bfad32 in fsp_header_init (space_id=27, size=768, mtr=0x7fffffff3f70) at /data/mysql-5.7.35/storage/innobase/fsp/fsp0fsp.cc:1083
#4  0x0000000001aa2ce8 in srv_open_tmp_tablespace (create_new_db=false, tmp_space=0x2d5f520 <srv_tmp_space>) at /data/mysql-5.7.35/storage/innobase/srv/srv0start.cc:1204
#5  0x0000000001aa5c68 in innobase_start_or_create_for_mysql () at /data/mysql-5.7.35/storage/innobase/srv/srv0start.cc:2441
#6  0x00000000018d29c6 in innobase_init (p=0x2dcd9d0) at /data/mysql-5.7.35/storage/innobase/handler/ha_innodb.cc:4119                                          
#7  0x0000000000f4f82f in ha_initialize_handlerton (plugin=0x2f08048) at /data/mysql-5.7.35/sql/handler.cc:848
#8  0x0000000001580279 in plugin_initialize (plugin=0x2f08048) at /data/mysql-5.7.35/sql/sql_plugin.cc:1233                                                           
#9  0x0000000001580ec9 in plugin_register_builtin_and_init_core_se (argc=0x2ceb510 <remaining_argc>, argv=0x2dcb318) at /data/mysql-5.7.35/sql/sql_plugin.cc:1596
#10 0x0000000000ec4da8 in init_server_components () at /data/mysql-5.7.35/sql/mysqld.cc:4081
#11 0x0000000000ec630b in mysqld_main (argc=34, argv=0x2dcb318) at /data/mysql-5.7.35/sql/mysqld.cc:4766
#12 0x0000000000ebde76 in main (argc=2, argv=0x7fffffffe028) at /data/mysql-5.7.35/sql/main.cc:32




#0  fsp_init_file_page (block=0x7fff7adf6e78, mtr=0x7fffffff3e20) at /data/mysql-5.7.35/storage/innobase/fsp/fsp0fsp.cc:799
#1  0x0000000001bfcec3 in fsp_page_create (page_id=..., page_size=..., rw_latch=RW_SX_LATCH, mtr=0x7fffffff3e20, init_mtr=0x7fffffff3e20)
    at /data/mysql-5.7.35/storage/innobase/fsp/fsp0fsp.cc:1908
#2  0x0000000001bfd4e8 in fsp_alloc_free_page (space=27, page_size=..., hint=0, rw_latch=RW_SX_LATCH, mtr=0x7fffffff3e20, init_mtr=0x7fffffff3e20)
    at /data/mysql-5.7.35/storage/innobase/fsp/fsp0fsp.cc:2031
#3  0x0000000001c00c67 in fseg_alloc_free_page_low (space=0x96d1888, page_size=..., seg_inode=0x7fff7b4c8032 "", hint=0, direction=111 'o', rw_latch=RW_SX_LATCH, mtr=0x7fffffff3e20, 
    init_mtr=0x7fffffff3e20, has_done_reservation=0) at /data/mysql-5.7.35/storage/innobase/fsp/fsp0fsp.cc:3110
#4  0x0000000001bff54c in fseg_create_general (space_id=27, page=0, byte_offset=62, has_done_reservation=0, mtr=0x7fffffff3e20) at /data/mysql-5.7.35/storage/innobase/fsp/fsp0fsp.cc:2687
#5  0x0000000001bff732 in fseg_create (space=27, page=0, byte_offset=62, mtr=0x7fffffff3e20) at /data/mysql-5.7.35/storage/innobase/fsp/fsp0fsp.cc:2747
#6  0x0000000001ae63ac in trx_rseg_header_create (space=27, page_size=..., max_size=18446744073709551614, rseg_slot_no=1, mtr=0x7fffffff3e20)
    at /data/mysql-5.7.35/storage/innobase/trx/trx0rseg.cc:77
#7  0x0000000001ae71a2 in trx_rseg_create (space_id=27, nth_free_slot=0) at /data/mysql-5.7.35/storage/innobase/trx/trx0rseg.cc:423
#8  0x0000000001ae9dfa in trx_sys_create_noredo_rsegs (n_nonredo_rseg=32) at /data/mysql-5.7.35/storage/innobase/trx/trx0sys.cc:857
#9  0x0000000001ae9f07 in trx_sys_create_rsegs (n_spaces=4, n_rsegs=128, n_tmp_rsegs=32) at /data/mysql-5.7.35/storage/innobase/trx/trx0sys.cc:890


b trx_rseg_create
	b trx_rseg_header_create
		b fseg_create
			b fseg_create_general




## 通常，我们在使用Mysql时，Mysql将数据文件都封装为了逻辑语义Database和Table，用户只需要感知并操作Database和Table就能完成对数据库的CRUD操作，但实际这一系列的访问请求最终都会转化为实际的文件操作，那这些过程具体是如何完成的呢，具体的Database和Table与文件的真实映射关系又是怎样的呢

LET'T GO!!

从物理文件的分类来看，有日志文件、主系统表空间文件ibdata、undo tablespace文件、临时表空间文件、用户表空间。

日志文件主要用于记录redo log，InnoDB采用循环使用的方式，你可以通过参数指定创建文件的个数和每个文件的大小。默认情况下，日志是以512字节的block单位写入。由于现代文件系统的block size通常设置到4k，InnoDB提供了一个选项，可以让用户将写入的redo日志填充到4KB，以避免read-modify-write的现象；而Percona Server则提供了另外一个选项，支持直接将redo日志的block size修改成指定的值。

ibdata是InnoDB最重要的系统表空间文件，它记录了InnoDB的核心信息，包括事务系统信息、元数据信息，记录InnoDB change buffer的btree，防止数据损坏的double write buffer等等关键信息。我们稍后会展开描述。

undo独立表空间是一个可选项，通常默认情况下，undo数据是存储在ibdata中的，但你也可以通过配置选项 innodb_undo_tablespaces 来将undo 回滚段分配到不同的文件中，目前开启undo tablespace 只能在install阶段进行。在主流版本进入5.7时代后，我们建议开启独立undo表空间，只有这样才能利用到5.7引入的新特效：online undo truncate。

MySQL 5.7 新开辟了一个临时表空间，默认的磁盘文件命名为ibtmp1，所有非压缩的临时表都存储在该表空间中。由于临时表的本身属性，该文件在重启时会重新创建。对于云服务提供商而言，通过ibtmp文件，可以更好的控制临时文件产生的磁盘存储。

用户表空间，顾名思义，就是用于自己创建的表空间，通常分为两类，一类是一个表空间一个文件，另外一种则是5.7版本引入的所谓General Tablespace，在满足一定约束条件下，可以将多个表创建到同一个文件中。除此之外，InnoDB还定义了一些特殊用途的ibd文件，例如全文索引相关的表文件。而针对空间数据类型，也构建了不同的数据索引格式R-tree。



## InnoDB 内存中对”.ibd”文件的管理


在InnoDB运行过程中，在内存中会保存所有Tablesapce的Space_id，Space_name以及相应的”.ibd”文件的映射，这个结构都存储在InnoDB的Fil_system这个对象中，在Fil_system这个对象中又包含64个shard，每个shard又会管理多个Tablespace，整体的关系为：Fil_system -> shard -> Tablespace。

在这64个shard中，一些特定的Tablesapce会被保存在特定的shard中，shard0是被用于存储系统表的Tablespace，58-61的shard被用于存储Undo space，最后一个，也就是shard63被用于存储redo，而其余的Tablespace都会根据Space_ID来和UNDO_SHARDS_START取模，来保存其Tablespace，具体可以查看shard_by_id()函数。



之前提到InnoDB会将Tablesapce的Space_id，Space_name以及相应的”.ibd”文件的映射一直保存在内存中，实际就是在shard的m_space和m_names中，
但这两个结构并非是在InnoDB启动的时候就把所有的Tablespace和对应的”.ibd”文件映射都保存下来，而是只按需去open，

比如去初始化redo和Undo等，而用户表只有在crash recovery中解析到了对应Tablespace的redo log，才会去调用fil_tablespace_open_for_recovery，在scan出mdirs中找到对应的”.ibd”文件来打开，并将其保存在m_space和m_names中，方便下次查找。

也就是说，在crash recovery阶段，实际在Fil_system并不会保存全量的”.ibd”文件映射，这个概念一定要记住，在排查crash recovery阶段ddl问题的时非常重要。



在crash recovery结束后，InnoDB的启动就已经基本结束了，而此时在启动阶段scan出的保存在mdirs中的”.ibd”文件就可以清除了，此时会通过ha_post_recovery()函数最终释放掉所有scan出的”.ibd”文件。

那此时就会有小伙伴要问了，如果不保存全量的文件映射，难不成用户的读请求进来时，还需要重新去查找ibd文件并打开嘛？

这当然不会，实际在InnoDB启动之后，
还会去初始化Data Dictionary Table（数据字典，简称DD，后文中的DD代称数据字典），
在DD的初始化过程中，会把DD中所保存的Tablesapce全部进行validate check一遍，用于检查是否有丢失ibd文件或者数据有残缺等情况，在这个过程中，会把所有保存在DD中的Tablespace信息，且在crash recovery中未能open的Tablespace全部打开一遍，并保存在Fil_system中，至此，整个InnoDB中所有的Tablespace的映射信息都会加载到内存中。

具体的调用逻辑为：

sql/dd/impl/bootstrapper.cc
|--initialize
	|--initialize_dictionary
		|--DDSE_dict_recover
storage/innobase/handler/ha_innodb.cc
			|--innobase_dict_recover
				|--boot_tablespaces
					|--Validate_files.validate
						|--alidate_files::check
							|--fil_ibd_open


## 数据字典（DD）和”.ibd”文件的关系

数据字典是有关数据库对象的信息的集合，例如作为表，视图，存储过程等，也称为数据库的元数据信息。
换一种说法来讲，数据字典存储了有关例如表结构，其中每个表具有的列，索引等信息。
数据字典还将有关表的信息存储在INFORMATION_SCHEMA中 和PERFORMANCE_SCHEMA中，这两个表都只是在内存中，他们有InnoDB运行过程中动态填充，并不会持久化在存储中引擎中。 
从Mysql 8.0开始，数据字典不在使用MyISAM作为默认存储引擎，而是直接存储在InnoDB中，所以现在DD表的写入和更新都是支持ACID的。


每当我们执行show databases或show tables时，此时就会查询数据字典，更准确的说是会从数据字典的cache中获取出相应的表信息，但show create table并不是访问数据字典的cache，这个操作或直接访问到schema表中的数据，这就是为什么有时候我们会遇到一些表在show tables能看到而show create table却看不到的问题，通常都是因为一些bug使得在DD cache中还保留的是旧的表信息导致的。

当我们执行一条SQL访问一个表时，在Mysql中首先会尝试去Open table，这个过程首先会访问到DD cache，
通过表名从中获取Tablespace的信息，
如果DD cache中没有，就会尝试从DD表中读取，
一般来说DD cache和DD表中的数据，以及InnoDB内部的Tablespace是完全对上的。


在我们执行DDL操作的时候，一般都会触发清理DD cache的操作，这个过程是必须要先持有整个Tablespace的MDL X锁，在对DDL操作完成之后，最终还会修改DD表中的信息，而在用户发起下一次读取的时候会将该信息从DD表中读取出来并缓存在DD cache中。


## 索引页




SMO（Structure Modification Operation）包括索引页的分裂（split）和删除（shrink）

























## 比较算乱，主要是在学习阿里大神们文章的同时，编辑graffle时产生的草稿，后面有时间也整理一下

默认使用16KB的Page Size来阐述文件的物理结构

为了管理Page，Extent这些数据块，在文件中记录了许多的节点以维持具有某些特征的链表，例如在在文件头维护的inode page链表，空闲、用满以及碎片化的Extent链表等等

FLST_LEN
FLST_FIRST
FLST_LAST


FLST_PREV
FLST_NEXT

FIL_ADDR_PAGE
FIL_ADDR_BYTE



FIL_PAGE_TYPE_FSP_HDR (SPACE HEADER)

FSP_SPACE_ID	4	该文件对应的space id
FSP_NOT_USED	4	如其名，保留字节，当前未使用
FSP_SIZE	4	当前表空间总的PAGE个数，扩展文件时需要更新该值（fsp_try_extend_data_file_with_pages）
FSP_FREE_LIMIT	4	当前尚未初始化的最小Page No。从该Page往后的都尚未加入到表空间的FREE LIST上。
FSP_SPACE_FLAGS	4	当前表空间的FLAG信息，见下文
FSP_FRAG_N_USED	4	FSP_FREE_FRAG 链表上已被使用的Page数，用于快速计算该链表上可用空闲Page数
FSP_FREE	16	当一个Extent中所有page都未被使用时，放到该链表上，可以用于随后的分配
FSP_FREE_FRAG	16	FREE_FRAG 链表的Base Node，通常这样的Extent中的Page可能归属于不同的segment，用于segment frag array page的分配（见下文）
FSP_FULL_FRAG	16	Extent中所有的page都被使用掉时，会放到该链表上，当有Page从该Extent释放时，则移回FREE_FRAG链表
FSP_SEG_ID	8	当前文件中最大Segment ID + 1，用于段分配时的seg id计数器
FSP_SEG_INODES_FULL	16	已被完全用满的Inode Page链表
FSP_SEG_INODES_FREE	16	至少存在一个空闲Inode Entry的Inode Page被放到该链表上


FSP_SPACE_FLAGS

FSP_FLAGS_POS_ZIP_SSIZE	压缩页的block size，如果为0表示非压缩表
FSP_FLAGS_POS_ATOMIC_BLOBS	使用的是compressed或者dynamic的行格式
FSP_FLAGS_POS_PAGE_SSIZE	Page Size
FSP_FLAGS_POS_DATA_DIR	如果该表空间显式指定了data_dir，则设置该flag
FSP_FLAGS_POS_SHARED	是否是共享的表空间，如5.7引入的General Tablespace，可以在一个表空间中创建多个表
FSP_FLAGS_POS_TEMPORARY	是否是临时表空间
FSP_FLAGS_POS_ENCRYPTION	是否是加密的表空间，MySQL 5.7.11引入
FSP_FLAGS_POS_UNUSED	未使用的位


FIL_PAGE_TYPE_XDES

除了上述描述信息外，其他部分的数据结构和XDES PAGE（FIL_PAGE_TYPE_XDES）都是相同的，使用连续数组的方式，每个XDES PAGE最多存储256个XDES Entry，每个Entry占用40个字节，描述64个Page（即一个Extent）。

Macro	bytes	Desc
XDES_ID	8	如果该Extent归属某个segment的话，则记录其ID
XDES_FLST_NODE	12(FLST_NODE_SIZE)	维持Extent链表的双向指针节点
XDES_STATE	4	该Extent的状态信息，包括：XDES_FREE，XDES_FREE_FRAG，XDES_FULL_FRAG，XDES_FSEG，详解见下文
XDES_BITMAP	16	总共16x8= 128个bit，用2个bit表示Extent中的一个page，一个bit表示该page是否是空闲的(XDES_FREE_BIT)，另一个保留位，尚未使用（XDES_CLEAN_BIT）


XDES_STATE 表示该Extent的四种不同状态：

通过XDES_STATE信息，我们只需要一个FLIST_NODE节点就可以维护每个Extent的信息，是处于全局表空间的链表上，还是某个btree segment的链表上。

Macro	Desc
XDES_FREE(1)	存在于FREE链表上
XDES_FREE_FRAG(2)	存在于FREE_FRAG链表上
XDES_FULL_FRAG(3)	存在于FULL_FRAG链表上
XDES_FSEG(4)	该Extent归属于ID为XDES_ID记录的值的SEGMENT。

IBUF BITMAP PAGE
第2个page类型为 FIL_PAGE_IBUF_BITMAP ，主要用于跟踪随后的每个page的change buffer信息，使用4个bit来描述每个page的change buffer信息。

Macro	bits	Desc
IBUF_BITMAP_FREE	2	使用2个bit来描述page的空闲空间范围：0（0 bytes）、1（512 bytes）、2（1024 bytes）、3（2048 bytes）
IBUF_BITMAP_BUFFERED	1	是否有ibuf操作缓存
IBUF_BITMAP_IBUF	1	该Page本身是否是Ibuf Btree的节点

由于bitmap page的空间有限，同样每隔256个Extent Page之后，也会在XDES PAGE之后创建一个ibuf bitmap page。




INODE PAGE
FILE SEGMENT INODE

数据文件的第3个page的类型为 FIL_PAGE_INODE ，用于管理数据文件中的segement，
每个索引占用2个segment，分别用于管理叶子节点和非叶子节点。

每个inode页可以存储 FSP_SEG_INODES_PER_PAGE （默认为85）个记录。

fseg_inode_t

Macro	bits	Desc

FSEG_INODE_PAGE_NODE	12	INODE页的链表节点，记录前后Inode Page的位置，BaseNode记录在头Page的 FSP_SEG_INODES_FULL 或者 FSP_SEG_INODES_FREE 字段。
Inode Entry 0	192	Inode记录
Inode Entry 1	 	 
……	 	 
Inode Entry 84	 	

每个Inode Entry的结构如下表所示：

众所周知，InnodDB采用Btree作为存储结构，当用户创建一个Table的时候，就会根据显示或隐式定义的主键构建了一棵Btree，
而构成Btree的叶子节点被称为Page，默认大小为16KB，每个Page都有一个独立的Page_no。

Segment可以简单理解为是一个逻辑的概念，在每个Tablespace创建之初，就会初始化两个Segment，
其中 Leaf node segment可以理解为 InnoDB中的INode Entry，
而 Extent是一个物理概念，每次Btree的扩容都是以Extent为单位来扩容的，默认一次扩容不超过4个Extent。

加了一个 Segment 的概念，Extent可以属于某个 Segment，slot 又是什么？
物理层上，只有 Extent = Page(16kb) * 64

Macro	bits	Desc
FSEG_ID	8	该Inode归属的Segment ID，若值为0 表示该slot未被使用
FSEG_NOT_FULL_N_USED	8	FSEG_NOT_FULL 链表上被使用的Page数量
FSEG_FREE	16	这个Segment的 free extent链表
FSEG_NOT_FULL	16	至少有一个 page分配给当前 Segment的 Extent链表，全部用完时，转移到 FSEG_FULL 上，全部释放时，则归还给当前表空间 FSP_FREE 链表
FSEG_FULL	16	分配给当前segment且Page完全使用完的Extent链表
FSEG_MAGIC_N	4	Magic Number
FSEG_FRAG_ARR 0	4	属于该Segment的独立Page。总是先从全局分配独立的Page，当填满32个数组项时，就在每次分配时都分配一个完整的Extent，并在 XDES PAGE中将其Segment ID设置为当前值
……	……	 
FSEG_FRAG_ARR 31	4	总共存储32个记录项

InnoDB通过Inode Entry来管理每个Segment占用的Page，Inode Entry所在的inode page有可能存放满，因此在Page0中维护了Inode Page链表.


FILE_ID,FILE_NAME,FILE_TYPE,TABLESPACE_NAME,ENGINE,FREE_EXTENTS,TOTAL_EXTENTS,EXTENT_SIZE,AUTOEXTEND_SIZE,DATA_FREE,STATUS

select hex(a.sp) from (select distinct(space) as sp from innodb_buffer_page) a;


索引最基本的页类型为 FIL_PAGE_INDEX 。可以划分为下面几个部分。

Page Header 首先不管任何类型的数据页都有38个字节来描述头信息（FIL_PAGE_DATA, or PAGE_HEADER），包含如下信息：

Macro	bytes	Desc
FIL_PAGE_SPACE_OR_CHKSUM	4	在MySQL4.0之前存储space id，之后的版本用于存储checksum
FIL_PAGE_OFFSET	4	当前页的page no
FIL_PAGE_PREV	4	通常用于维护btree同一level的双向链表，指向链表的前一个page，没有的话则值为FIL_NULL
FIL_PAGE_NEXT	4	和FIL_PAGE_PREV类似，记录链表的下一个Page的Page No
FIL_PAGE_LSN	8	最近一次修改该page的LSN
FIL_PAGE_TYPE	2	Page类型
FIL_PAGE_FILE_FLUSH_LSN	8	只用于系统表空间的第一个Page，记录在正常shutdown时安全checkpoint到的点，对于用户表空间，这个字段通常是空闲的，但在5.7里，FIL_PAGE_COMPRESSED 类型的数据页则另有用途。下一小节单独介绍
FIL_PAGE_SPACE_ID	4	存储page所在的space id



PAGE_HEADER

Index Header 紧随 FIL_PAGE_DATA 之后的是索引信息，这部分信息是索引页独有的。

Macro	bytes	Desc

PAGE_N_DIR_SLOTS	2	Page directory中的slot个数 （见下文关于Page directory的描述）
PAGE_HEAP_TOP	2	指向当前Page内已使用的空间的末尾位置，即free space的开始位置
PAGE_N_HEAP	2	Page内所有记录个数，包含用户记录，系统记录以及标记删除的记录，同时当第一个bit设置为1时，表示这个page内是以Compact格式存储的
PAGE_FREE	2	指向标记删除的记录链表的第一个记录
PAGE_GARBAGE	2	被删除的记录链表上占用的总的字节数，属于可回收的垃圾碎片空间
PAGE_LAST_INSERT	2	指向最近一次插入的记录偏移量，主要用于优化顺序插入操作
PAGE_DIRECTION	2	用于指示当前记录的插入顺序以及是否正在进行顺序插入，每次插入时，PAGE_LAST_INSERT会和当前记录进行比较，以确认插入方向，据此进行插入优化,PAGE_LEFT
PAGE_N_DIRECTION	2	当前以相同方向的顺序插入记录个数
PAGE_N_RECS	2	Page上有效的未被标记删除的用户记录个数
PAGE_MAX_TRX_ID	8	最近一次修改该page记录的事务ID，主要用于辅助判断二级索引记录的可见性。
PAGE_LEVEL	2	该Page所在的btree level，根节点的level最大，叶子节点的level为0
PAGE_INDEX_ID	8	该Page归属的索引ID


Segment Info 随后20个字节描述段信息，仅在Btree的root Page中被设置，其他Page都是未使用的。

Macro	bytes	Desc
PAGE_BTR_SEG_LEAF	10(FSEG_HEADER_SIZE)	leaf segment在inode page中的位置
PAGE_BTR_SEG_TOP	10(FSEG_HEADER_SIZE)	non-leaf segment在inode page中的位置

10个字节的inode信息包括：

Macro	bytes	Desc
FSEG_HDR_SPACE	4	描述该segment的inode page所在的space id （目前的实现来看，感觉有点多余…）
FSEG_HDR_PAGE_NO	4	描述该segment的inode page的page no
FSEG_HDR_OFFSET	2	inode page内的页内偏移量
通过上述信息，我们可以找到对应segment在inode page中的描述项，进而可以操作整个segment。



系统记录 之后是两个系统记录，分别用于描述该page上的极小值和极大值，这里存在两种存储方式，分别对应旧的InnoDB文件系统，及新的文件系统（compact page）

Macro	bytes	Desc
REC_N_OLD_EXTRA_BYTES + 1	7	固定值，见 infimum_supremum_redundant 的注释
PAGE_OLD_INFIMUM	8	“infimum\0”
REC_N_OLD_EXTRA_BYTES + 1	7	固定值，见infimum_supremum_redundant的注释
PAGE_OLD_SUPREMUM	9	“supremum\0”
Compact的系统记录存储方式为：

Macro	bytes	Desc
REC_N_NEW_EXTRA_BYTES	5	固定值，见infimum_supremum_compact的注释
PAGE_NEW_INFIMUM	8	“infimum\0”
REC_N_NEW_EXTRA_BYTES	5	固定值，见infimum_supremum_compact的注释
PAGE_NEW_SUPREMUM	8	“supremum”，这里不带字符0
两种格式的主要差异在于不同行存储模式下，单个记录的描述信息不同。在实际创建page时，系统记录的值已经初始化好了，对于老的格式(REDUNDANT)，对应代码里的infimum_supremum_redundant，对于新的格式(compact)，对应infimum_supremum_compact。infimum记录的固定heap no为0，supremum记录的固定Heap no 为1。page上最小的用户记录前节点总是指向infimum，page上最大的记录后节点总是指向supremum记录。

具体参考索引页创建函数：page_create_low





用户记录 在系统记录之后就是真正的用户记录了，heap no 从2（PAGE_HEAP_NO_USER_LOW）开始算起。注意Heap no仅代表物理存储顺序，不代表键值顺序。

根据不同的类型，用户记录可以是非叶子节点的Node指针信息，也可以是只包含有效数据的叶子节点记录。而不同的行格式存储的行记录也不同，例如在早期版本中使用的redundant格式会被现在的compact格式使用更多的字节数来描述记录，例如描述记录的一些列信息，在使用compact格式时，可以改为直接从数据词典获取。因为redundant属于渐渐被抛弃的格式，本文的讨论中我们默认使用Compact格式。在文件rem/rem0rec.cc的头部注释描述了记录的物理结构。

每个记录都存在rec header，描述如下（参阅文件include/rem0rec.ic）

bytes	Desc
变长列长度数组	如果列的最大长度为255字节，使用1byte；否则，0xxxxxxx (one byte, length=0..127), or 1exxxxxxxxxxxxxx (two bytes, length=128..16383, extern storage flag)
SQL-NULL flag	标示值为NULL的列的bitmap，每个位标示一个列，bitmap的长度取决于索引上可为NULL的列的个数(dict_index_t::n_nullable)，这两个数组的解析可以参阅函数rec_init_offsets
下面5个字节（REC_N_NEW_EXTRA_BYTES）描述记录的额外信息	….

REC_NEW_INFO_BITS (4 bits)	目前只使用了两个bit，一个用于表示该记录是否被标记删除(REC_INFO_DELETED_FLAG)，另一个bit(REC_INFO_MIN_REC_FLAG)如果被设置，表示这个记录是当前level最左边的page的第一个用户记录
REC_NEW_N_OWNED (4 bits)	当该值为非0时，表示当前记录占用page directory里一个slot，并和前一个slot之间存在这么多个记录
REC_NEW_HEAP_NO (13 bits)	该记录的heap no
REC_NEW_STATUS (3 bits)	记录的类型，包括四种：REC_STATUS_ORDINARY(叶子节点记录)， REC_STATUS_NODE_PTR（非叶子节点记录），REC_STATUS_INFIMUM(infimum系统记录)以及REC_STATUS_SUPREMUM(supremum系统记录)
REC_NEXT (2bytes)	指向按照键值排序的page内下一条记录数据起点，这里存储的是和当前记录的相对位置偏移量（函数rec_set_next_offs_new）

在记录头信息之后的数据视具体情况有所不同：

对于聚集索引记录，数据包含了事务id，回滚段指针；
对于二级索引记录，数据包含了二级索引键值以及聚集索引键值。如果二级索引键和聚集索引有重合，则只保留一份重合的，例如pk (col1, col2)，sec key(col2, col3)，在二级索引记录中就只包含(col2, col3, col1);
对于非叶子节点页的记录，聚集索引上包含了其子节点的最小记录键值及对应的page no；二级索引上有所不同，除了二级索引键值外，还包含了聚集索引键值，再加上page no三部分构成。


PAGE DIRECTORY

Page directory 为了加快页内的数据查找，会按照记录的顺序，每隔4~8个数量（PAGE_DIR_SLOT_MIN_N_OWNED ~ PAGE_DIR_SLOT_MAX_N_OWNED）的用户记录，就分配一个slot （每个slot占用2个字节，PAGE_DIR_SLOT_SIZE），存储记录的页内偏移量，可以理解为在页内构建的一个很小的索引(sparse index)来辅助二分查找。

Page Directory的slot分配是从Page末尾（倒数第八个字节开始）开始逆序分配的。在查询记录时。先根据page directory 确定记录所在的范围，然后在据此进行线性查询。



Innodb change buffer介绍

/*--------------------------------------*/
#define FSP_XDES_OFFSET			0	/* !< extent descriptor */
#define FSP_IBUF_BITMAP_OFFSET		1	/* !< insert buffer bitmap */
				/* The ibuf bitmap pages are the ones whose
				page number is the number above plus a
				multiple of XDES_DESCRIBED_PER_PAGE */

#define FSP_FIRST_INODE_PAGE_NO		2	/*!< in every tablespace */
				/* The following pages exist
				in the system tablespace (space 0). */
#define FSP_IBUF_HEADER_PAGE_NO		3	/*!< insert buffer
						header page, in
						tablespace 0 */
#define FSP_IBUF_TREE_ROOT_PAGE_NO	4	/*!< insert buffer
						B-tree root page in
						tablespace 0 */
				/* The ibuf tree root page number in
				tablespace 0; its fseg inode is on the page
				number FSP_FIRST_INODE_PAGE_NO */
#define FSP_TRX_SYS_PAGE_NO		5	/*!< transaction
						system header, in
						tablespace 0 */
#define	FSP_FIRST_RSEG_PAGE_NO		6	/*!< first rollback segment
						page, in tablespace 0 */
#define FSP_DICT_HDR_PAGE_NO		7	/*!< data dictionary header
						page, in tablespace 0 */



FSP_TRX_SYS_PAGE_NO/
FSP_FIRST_RSEG_PAGE_NO (first rollback segment page, in tablespace 0) 

ibdata的第6个page，记录了InnoDB重要的事务系统信息，主要包括：

Macro	bytes	Desc

TRX_SYS	38	每个数据页都会保留的文件头字段
TRX_SYS_TRX_ID_STORE	8	持久化的最大事务ID，这个值不是实时写入的，而是256次递增写一次
TRX_SYS_FSEG_HEADER	10	指向用来管理事务系统的segment所在的位置

TRX_SYS_RSEGS	128 * 8	用于存储128个回滚段位置，包括space id及page no。每个回滚段包含一个文件segment（trx_rseg_header_create）
// the start of the array of rollback segment specification slots

……	以下是Page内UNIV_PAGE_SIZE - 1000的偏移位置	 

TRX_SYS_MYSQL_LOG_MAGIC_N_FLD	4	Magic Num ，值为873422344
TRX_SYS_MYSQL_LOG_OFFSET_HIGH	4	事务提交时会将其binlog位点更新到该page中，这里记录了在binlog文件中偏移量的高位的4字节
TRX_SYS_MYSQL_LOG_OFFSET_LOW	4	同上，记录偏移量的低4位字节
TRX_SYS_MYSQL_LOG_NAME	4	记录所在的binlog文件名

……	以下是Page内UNIV_PAGE_SIZE - 200 的偏移位置	 

TRX_SYS_DOUBLEWRITE_FSEG	10	包含double write buffer的fseg header
TRX_SYS_DOUBLEWRITE_MAGIC	4	Magic Num
TRX_SYS_DOUBLEWRITE_BLOCK1	4	double write buffer的第一个block(占用一个Extent)在ibdata中的开始位置，连续64个page
TRX_SYS_DOUBLEWRITE_BLOCK2	4	第二个dblwr block的起始位置
TRX_SYS_DOUBLEWRITE_REPEAT	12	重复记录上述三个字段，即MAGIC NUM, block1, block2，防止发生部分写时可以恢复
TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED	4	用于兼容老版本，当该字段的值不为TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N时，需要重置dblwr中的数据





InnoDB最多可以创建128个回滚段，每个回滚段需要单独的Page来维护其拥有的undo slot，Page类型为FIL_PAGE_TYPE_SYS。描述如下：

Macro	bytes	Desc
TRX_RSEG	38	保留的Page头
TRX_RSEG_MAX_SIZE	4	回滚段允许使用的最大Page数，当前值为ULINT_MAX
TRX_RSEG_HISTORY_SIZE	4	在history list上的undo page数，这些page需要由purge线程来进行清理和回收
TRX_RSEG_HISTORY	FLST_BASE_NODE_SIZE(16)	history list的base node
TRX_RSEG_FSEG_HEADER	(FSEG_HEADER_SIZE)10	指向当前管理当前回滚段的 inode entry
TRX_RSEG_UNDO_SLOTS	1024 * 4	undo slot数组，共1024个slot，值为FIL_NULL表示未被占用，否则记录占用该slot的第一个undo page


实际存储undo记录的Page类型为FIL_PAGE_UNDO_LOG，undo header结构如下

Macro	bytes	Desc
TRX_UNDO_PAGE_HDR	38	Page 头
TRX_UNDO_PAGE_TYPE	2	记录Undo类型，是TRX_UNDO_INSERT还是TRX_UNDO_UPDATE
TRX_UNDO_PAGE_START	2	事务所写入的最近的一个undo log在page中的偏移位置
TRX_UNDO_PAGE_FREE	2	指向当前undo page中的可用的空闲空间起始偏移量
TRX_UNDO_PAGE_NODE	12	链表节点，提交后的事务，其拥有的undo页会加到history list上


#define	TRX_UNDO_SEG_HDR	(TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE)
/** Undo log segment header */
/* @{ */
/*-------------------------------------------------------------*/
#define	TRX_UNDO_STATE		0	/*!< TRX_UNDO_ACTIVE, ... */

#ifndef UNIV_INNOCHECKSUM

#define	TRX_UNDO_LAST_LOG	2	/*!< Offset of the last undo log header
					on the segment header page, 0 if
					none */
#define	TRX_UNDO_FSEG_HEADER	4	/*!< Header for the file segment which
					the undo log segment occupies */
#define	TRX_UNDO_PAGE_LIST	(4 + FSEG_HEADER_SIZE)
					/*!< Base node for the list of pages in
					the undo log segment; defined only on
					the undo log segment's first page */
/*-------------------------------------------------------------*/
/** Size of the undo log segment header */
#define TRX_UNDO_SEG_HDR_SIZE	(4 + FSEG_HEADER_SIZE + FLST_BASE_NODE_SIZE)
/* @} */


FLST_NODE_SIZE

/* States of an undo log segment */
#define TRX_UNDO_ACTIVE		1	/* contains an undo log of an active
					transaction */
#define	TRX_UNDO_CACHED		2	/* cached for quick reuse */
#define	TRX_UNDO_TO_FREE	3	/* insert undo segment can be freed */
#define	TRX_UNDO_TO_PURGE	4	/* update undo segment will not be
					reused: it can be freed in purge when
					all undo data in it is removed */
#define	TRX_UNDO_PREPARED	5	/* contains an undo log of an
					prepared transaction */


/** The undo log header. There can be several undo log headers on the first
page of an update undo log segment. */
/* @{ */
/*-------------------------------------------------------------*/
#define	TRX_UNDO_TRX_ID		0	/*!< Transaction id */
#define	TRX_UNDO_TRX_NO		8	/*!< Transaction number of the
					transaction; defined only if the log
					is in a history list */
#define TRX_UNDO_DEL_MARKS	16	/*!< Defined only in an update undo
					log: TRUE if the transaction may have
					done delete markings of records, and
					thus purge is necessary */
#define	TRX_UNDO_LOG_START	18	/*!< Offset of the first undo log record
					of this log on the header page; purge
					may remove undo log record from the
					log start, and therefore this is not
					necessarily the same as this log
					header end offset */
#define	TRX_UNDO_XID_EXISTS	20	/*!< TRUE if undo log header includes
					X/Open XA transaction identification
					XID */
#define	TRX_UNDO_DICT_TRANS	21	/*!< TRUE if the transaction is a table
					create, index create, or drop
					transaction: in recovery
					the transaction cannot be rolled back
					in the usual way: a 'rollback' rather
					means dropping the created or dropped
					table, if it still exists */
#define TRX_UNDO_TABLE_ID	22	/*!< Id of the table if the preceding
					field is TRUE */
#define	TRX_UNDO_NEXT_LOG	30	/*!< Offset of the next undo log header
					on this page, 0 if none */
#define	TRX_UNDO_PREV_LOG	32	/*!< Offset of the previous undo log
					header on this page, 0 if none */
#define TRX_UNDO_HISTORY_NODE	34	/*!< If the log is put to the history
					list, the file list node is here */
/*-------------------------------------------------------------*/
/** Size of the undo log header without XID information */
#define TRX_UNDO_LOG_OLD_HDR_SIZE (34 + FLST_NODE_SIZE)

/* Note: the writing of the undo log old header is coded by a log record
MLOG_UNDO_HDR_CREATE or MLOG_UNDO_HDR_REUSE. The appending of an XID to the
header is logged separately. In this sense, the XID is not really a member
of the undo log header. TODO: do not append the XID to the log header if XA
is not needed by the user. The XID wastes about 150 bytes of space in every
undo log. In the history list we may have millions of undo logs, which means
quite a large overhead. */

/** X/Open XA Transaction Identification (XID) */
/* @{ */
/** xid_t::formatID */
#define	TRX_UNDO_XA_FORMAT	(TRX_UNDO_LOG_OLD_HDR_SIZE)
/** xid_t::gtrid_length */
#define	TRX_UNDO_XA_TRID_LEN	(TRX_UNDO_XA_FORMAT + 4)
/** xid_t::bqual_length */
#define	TRX_UNDO_XA_BQUAL_LEN	(TRX_UNDO_XA_TRID_LEN + 4)
/** Distributed transaction identifier data */
#define	TRX_UNDO_XA_XID		(TRX_UNDO_XA_BQUAL_LEN + 4)
/*--------------------------------------------------------------*/
#define TRX_UNDO_LOG_XA_HDR_SIZE (TRX_UNDO_XA_XID + XIDDATASIZE)
					/*!< Total size of the undo log header
					with the XA XID */
/* @} */



select hex(a.sp) from (select distinct(space) as sp from innodb_buffer_page) a;
select distinct hex(space), page_type from innodb_buffer_page;

TRX_SYS_FSEG_HEADER
TRX_RSEG_FSEG_HEADER
TRX_UNDO_PAGE_HDR


TRX_UNDO_LAST_LOG
TRX_UNDO_PAGE_LIST

TRX_UNDO_NEXT_LOG
TRX_UNDO_TRX_ID

TRX_UNDO_PREV_LOG






trx_undo_page_report_insert

trx_undo_page_report_modify


#define	TRX_UNDO_INSERT_REC	11	/* fresh insert into clustered index */
#define	TRX_UNDO_UPD_EXIST_REC	12	/* update of a non-delete-marked
					record */
#define	TRX_UNDO_UPD_DEL_REC	13	/* update of a delete marked record to
					a not delete marked record; also the
					fields of the record can change */
#define	TRX_UNDO_DEL_MARK_REC	14	/* delete marking of a record; fields
					do not change */
#define	TRX_UNDO_CMPL_INFO_MULT	16	/* compilation info is multiplied by
					this and ORed to the type above */
#define	TRX_UNDO_UPD_EXTERN	128	/* This bit can be ORed to type_cmpl
					to denote that we updated external
					storage fields: used by purge to
					free the external storage */


rec_get_info_bits 
DATA_TRX_ID
DATA_ROLL_PTR


trx_undo_page_report_modify_ext


/* States of an undo log segment */
#define TRX_UNDO_ACTIVE		1	/* contains an undo log of an active
					transaction */
#define	TRX_UNDO_CACHED		2	/* cached for quick reuse */
#define	TRX_UNDO_TO_FREE	3	/* insert undo segment can be freed */
#define	TRX_UNDO_TO_PURGE	4	/* update undo segment will not be
					reused: it can be freed in purge when
					all undo data in it is removed */
#define	TRX_UNDO_PREPARED	5	/* contains an undo log of an
					prepared transaction */





# TODO

### 事务回滚
如果事务因为异常或者被显式的回滚了，那么所有数据变更都要改回去。这里就要借助回滚日志中的数据来进行恢复了。

入口函数为： row_undo_step --> row_undo

操作也比较简单，析取老版本记录，做逆向操作即可：对于标记删除的记录清理标记删除标记；对于in-place更新，将数据回滚到最老版本；对于插入操作，直接删除聚集索引和二级索引记录（row_undo_ins）。

具体的操作中，先回滚二级索引记录（row_undo_mod_del_mark_sec、row_undo_mod_upd_exist_sec、row_undo_mod_upd_del_sec），再回滚聚集索引记录（row_undo_mod_clust）。这里不展开描述，可以参阅对应的函数。

### 多版本控制
InnoDB的多版本使用undo来构建，这很好理解，undo记录中包含了记录更改前的镜像，如果更改数据的事务未提交，对于隔离级别大于等于read commit的事务而言，它不应该看到已修改的数据，而是应该给它返回老版本的数据。

入口函数： row_vers_build_for_consistent_read

由于在修改聚集索引记录时，总是存储了回滚段指针和事务id，可以通过该指针找到对应的undo 记录，通过事务Id来判断记录的可见性。当旧版本记录中的事务id对当前事务而言是不可见时，则继续向前构建，直到找到一个可见的记录或者到达版本链尾部。（关于事务可见性及read view，可以参阅我们之前的月报）

Tips 1：构建老版本记录（trx_undo_prev_version_build）需要持有page latch，因此如果Undo链太长的话，其他请求该page的线程可能等待时间过长导致crash，最典型的就是备库备份场景：

当备库使用innodb表存储复制位点信息时（relay_log_info_repository=TABLE），逻辑备份显式开启一个read view并且执行了长时间的备份时，这中间都无法对slave_relay_log_info表做purge操作，导致版本链极其长；当开始备份slave_relay_log_info表时，就需要去花很长的时间构建老版本；复制线程由于需要更新slave_relay_log_info表，因此会陷入等待Page latch的场景，最终有可能导致信号量等待超时，实例自杀。 （bug#74003）

Tips 2：在构建老版本的过程中，总是需要创建heap来存储旧版本记录，实际上这个heap是可以重用的，无需总是重复构建（bug#69812）

Tips 3：如果回滚段类型是INSERT，就完全没有必要去看Undo日志了，因为一个未提交事务的新插入记录，对其他事务而言总是不可见的。

Tips 4: 对于聚集索引我们知道其记录中存有修改该记录的事务id，我们可以直接判断是否需要构建老版本(lock_clust_rec_cons_read_sees)，但对于二级索引记录，并未存储事务id，而是每次更新记录时，同时更新记录所在的page上的事务id（PAGE_MAX_TRX_ID），如果该事务id对当前事务是可见的，那么就无需去构建老版本了，否则就需要去回表查询对应的聚集索引记录，然后判断可见性（lock_sec_rec_cons_read_sees）。

### Purge清理操作
从上面的分析我们可以知道：update_undo产生的日志会放到history list中，当这些旧版本无人访问时，需要进行清理操作；另外页内标记删除的操作也需要从物理上清理掉。后台Purge线程负责这些工作。

入口函数：srv_do_purge --> trx_purge

确认可见性

在开始尝试purge前，purge线程会先克隆一个最老的活跃视图（trx_sys->mvcc->clone_oldest_view），所有在readview开启之前提交的事务所做的事务变更都是可以清理的。

获取需要purge的undo记录（trx_purge_attach_undo_recs）

从history list上读取多个Undo记录，并分配到多个purge线程的工作队列上（(purge_node_t) thr->child->undo_recs），默认一次最多取300个undo记录，可通过参数innodb_purge_batch_size参数调整。

Purge工作线程

当完成任务的分发后，各个工作线程（包括协调线程）开始进行purge操作 入口函数： row_purge_step -> row_purge -> row_purge_record_func

主要包括两种：一种是记录直接被标记删除了，这时候需要物理清理所有的聚集索引和二级索引记录（row_purge_record_func）；另一种是聚集索引in-place更新了，但二级索引上的记录顺序可能发生变化，而二级索引的更新总是标记删除 + 插入，因此需要根据回滚段记录去检查二级索引记录序是否发生变化，并执行清理操作（row_purge_upd_exist_or_extern）。

清理history list

从前面的分析我们知道，insert undo在事务提交后，Undo segment 就释放了。而update undo则加入了history list，为了将这些文件空间回收重用，需要对其进行truncate操作；默认每处理128轮Purge循环后，Purge协调线程需要执行一次purge history List操作。

入口函数：trx_purge_truncate --> trx_purge_truncate_history

从回滚段的HISTORY 文件链表上开始遍历释放Undo log segment，由于history 链表是按照trx no有序的，因此遍历truncate直到完全清除，或者遇到一个还未purge的undo log（trx no比当前purge到的位置更大）时才停止。

关于Purge操作的逻辑实际上还算是比较复杂的代码模块，这里只是简单的介绍了下，以后有时间再展开描述。

### 崩溃恢复
当实例从崩溃中恢复时，需要将活跃的事务从undo中提取出来，对于ACTIVE状态的事务直接回滚，对于Prepare状态的事务，如果该事务对应的binlog已经记录，则提交，否则回滚事务。

实现的流程也比较简单，首先先做redo (recv_recovery_from_checkpoint_start)，undo是受redo 保护的，因此可以从redo中恢复（临时表undo除外，临时表undo是不记录redo的）。

在redo日志应用完成后，初始化完成数据词典子系统（dict_boot），随后开始初始化事务子系统（trx_sys_init_at_db_start），undo 段的初始化即在这一步完成。

在初始化undo段时(trx_sys_init_at_db_start -> trx_rseg_array_init -> ... -> trx_undo_lists_init)，会根据每个回滚段page中的slot是否被使用来恢复对应的undo log，读取其状态信息和类型等信息，创建内存结构，并存放到每个回滚段的undo list上。

当初始化完成undo内存对象后，就要据此来恢复崩溃前的事务链表了(trx_lists_init_at_db_start)，根据每个回滚段的insert_undo_list来恢复插入操作的事务(trx_resurrect_insert)，根据update_undo_list来恢复更新事务(tex_resurrect_update)，如果既存在插入又存在更新，则只恢复一个事务对象。另外除了恢复事务对象外，还要恢复表锁及读写事务链表，从而恢复到崩溃之前的事务场景。

当从Undo恢复崩溃前活跃的事务对象后，会去开启一个后台线程来做事务回滚和清理操作（recv_recovery_rollback_active -> trx_rollback_or_clean_all_recovered），对于处于ACTIVE状态的事务直接回滚，对于既不ACTIVE也非PREPARE状态的事务，直接则认为其是提交的，直接释放事务对象。但完成这一步后，理论上事务链表上只存在PREPARE状态的事务。

随后很快我们进入XA Recover阶段，MySQL使用内部XA，即通过Binlog和InnoDB做XA恢复。在初始化完成引擎后，Server层会开始扫描最后一个Binlog文件，搜集其中记录的XID（MYSQL_BIN_LOG::recover），然后和InnoDB层的事务XID做对比。如果XID已经存在于binlog中了，对应的事务需要提交；否则需要回滚事务。

Tips：为何只需要扫描最后一个binlog文件就可以了？ 因为在每次rotate到一个新的binlog文件之前，总是要保证前一个binlog文件中对应的事务都提交并且sync redo到磁盘了，也就是说，前一个binlog文件中的事务在崩溃恢复时肯定是出于提交状态的。

## double write buffer 

## change buffer 


## 刷脏 ？？ 




CREATE TABLE user (
    id int(11) NOT NULL AUTO_INCREMENT,
    name varchar(10) DEFAULT NULL,
    age int(11) DEFAULT NULL,
    gender smallint(6) DEFAULT NULL,
    create_time date DEFAULT NULL,
    PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;



insert into world.user(name,age,gender,create_time)  values('aaa',18,1,now());

