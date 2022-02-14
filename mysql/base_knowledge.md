Mysql's World

#### 学习阿里月报的大神们文章的同时，也整理一些自己的笔记
http://mysql.taobao.org/monthly/2017/05/01/
http://mysql.taobao.org/monthly/2020/08/06/
http://mysql.taobao.org/monthly/2016/02/01/


## 为什么要使用如此复杂的内存操作
用户对数据库的最基本要求就是能高效的读取和存储数据，但是读写数据都涉及到与低速的设备交互，为了弥补两者之间的速度差异，所有数据库都有缓存池，用来管理相应的数据页，提高数据库的效率，当然也因为引入了这一中间层，数据库对内存的管理变得相对比较复杂

但随着硬件技术的革新，相对于传统的HDD及SSD，NVM最大的优势在于：
接近内存的高性能
顺序访问和随机访问差距不大
按字节寻址而不是Block 
有可能需要重新考虑现有的内存管理方案。

## Page类型
 SYSTEM            
 INODE             
 IBUF_INDEX        
 TRX_SYSTEM        
 UNDO_LOG          
 FILE_SPACE_HEADER 
 INDEX             
 UNKNOWN           
 LOB_FIRST         
 RSEG_ARRAY        
 IBUF_BITMAP       
 LOB_DATA          
 LOB_INDEX         


## 事务ACID: 

   ### 原子性(Failure Atomic): (整个事务要么全部成功，要么全部失败)
	指的是整个事务要么全部成功，要么全部失败，对InnoDB来说，
	只要client收到server发送过来的commit成功报文，那么这个事务一定是成功的。
	如果收到的是rollback的成功报文，那么整个事务的所有操作一定都要被回滚掉，就好像什么都没执行过一样。
	另外，如果连接中途断开或者server crash事务也要保证会滚掉。InnoDB通过undolog保证rollback的时候能找到之前的数据。

	
   ### 一致性: (保证不会读到中间状态的数据)
	指的是在任何时刻，包括数据库正常提供服务的时候，数据库从异常中恢复过来的时候，数据都是一致的，保证不会读到中间状态的数据。
	在InnoDB中，主要通过crash recovery和double write buffer的机制保证数据的一致性。

   ### 隔离性: 
    指的是多个事务可以同时对数据进行修改，但是相互不影响。InnoDB中，依据不同的业务场景，有四种隔离级别可以选择。
	默认是RR隔离级别，因为相比于RC，InnoDB的 RR性能更加好。

   ### 持久性(Durability of Updates): 
    值的是事务commit的数据在任何情况下都不能丢。在内部实现中，InnoDB通过redolog保证已经commit的数据一定不会丢失。



## 多版本控制: 


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
	undo log
	undo record. 这几个概念

	undo log是最小的粒度，所在的数据页称为undo page，然后若干个undo page构成一个undo slot。

	一个事务最多可以有两个undo slot，segment
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

### MTR

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

InnoDB会将事务执行过程拆分为若干个Mini Transaction（mtr），每个mtr包含一系列如加锁，写数据，写redo，放锁等操作。举个例子：

	void btr_truncate(const dict_index_t *index) {
	  
	  ... ...
	      
	  page_size_t page_size(space->flags);
	  const page_id_t page_id(space_id, root_page_no);
	  mtr_t mtr;
	  buf_block_t *block;

	  mtr.start();

	  mtr_x_lock(&space->latch, &mtr);

	  block = buf_page_get(page_id, page_size, RW_X_LATCH, &mtr);

	  page_t *page = buf_block_get_frame(block);
	  ut_ad(page_is_root(page));

	  /* Mark that we are going to truncate the index tree
	  We use PAGE_MAX_TRX_ID as it should be always 0 for clustered
	  index. */
	  mlog_write_ull(page + (PAGE_HEADER + PAGE_MAX_TRX_ID), IB_ID_MAX, &mtr);

	  mtr.commit();
	    
	  ... ...
	      
	}

btr_truncate函数将一个BTree truncate到只剩root节点，具体实现这里暂不用关心，不过上面有一步是需要将root page header的PAGE_MAX_TRX_ID修改为IB_ID_MAX，这个操作算是修改page上的数据，这里就需要通过mtr来做，具体：

	mtr.start() 开启一个mini transaction
	mtr_x_lock() 加锁，这个操作分成两步，1. 对space->latch加X锁；2. 将space->latch放入mtr_t::m_impl::memo中（这样在mtr.commit()后就可以将mtr之前加过的锁放掉）
	mlog_write_ull 写数据，这个操作也分成两步，1. 直接修改page上的数据；2. 将该操作的redo log写入mtr::m_impl::m_log中
	mtr.commit() 写redo log + 放锁，这个操作会将上一步m_log中的内容写入redo log file，并且在最后放锁

	以上就是一个mtr大致的执行过程。



### 通常，我们在使用Mysql时，Mysql将数据文件都封装为了逻辑语义Database和Table，用户只需要感知并操作Database和Table就能完成对数据库的 CRUD 操作，但实际这一系列的访问请求最终都会转化为实际的文件操作，那这些过程具体是如何完成的呢，具体的Database和Table与文件的真实映射关系又是怎样的呢


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


// Transaction states (trx_t::state)
enum trx_state_t {
	TRX_STATE_NOT_STARTED,
	TRX_STATE_ACTIVE,
	TRX_STATE_PREPARED,
	TRX_STATE_COMMITTED_IN_MEMORY
};


可以看出的只有Undo或Redo的问题，主要来自于对Commit标记及Data落盘顺序的限制，而这种限制归根结底来源于Log信息中对新值或旧值的缺失。因此Redo-Undo采用同时记录新值和旧值的方式，来消除Commit和Data之间刷盘顺序的限制。

数据库系统运行过程中可能遇到的故障类型主要包括，Transaction Failure，Process Failure，System Failure以及Media Failure。其中Transaction Failure可能是主动回滚或者冲突后强制Abort；Process Failure指的是由于各种原因导致的进程退出，进程内存内容会丢失；System Failure来源于操作系统或硬件故障；而Media Failure则是存储介质的不可恢复损坏。数据库系统需要正确合理的处理这些故障，从而保证系统的正确性。为此需要提供两个特性：

Durability of Updates：已经Commit的事务的修改，故障恢复后仍然存在；
Failure Atomic：失败事务的所有修改都不可见。
因此，故障恢复的问题描述为：即使在出现故障的情况下，数据库依然能够通过提供Durability及Atomic特性，保证恢复后的数据库状态正确。然而，要解决这个问题并不是一个简单的事情，为了不显著牺牲数据库性能，长久以来人们对故障恢复机制进行了一系列的探索。

Redo-Undo Logging

In this paper we present a simple and efficient method, called ARIES ( Algorithm for Recouery
and Isolation Exploiting Semantics), 


ARIES逐步成为磁盘数据库实现故障恢复的标配，ARIES本质是一种Redo-Undo的WAL实现。 Normal过程：修改数据之前先追加Log记录，Log内容同时包括Redo和Undo信息，每个日志记录产生一个标记其在日志中位置的递增LSN（Log Sequence Number）；数据Page中记录最后修改的日志项LSN，以此来判断Page中的内容的新旧程度，实现幂等。故障恢复阶段需要通过Log中的内容恢复数据库状态，为了减少恢复时需要处理的日志量，ARIES会在正常运行期间周期性的生成Checkpoint，Checkpoint中除了当前的日志LSN之外，还需要记录当前活跃事务的最新LSN，以及所有脏页，供恢复时决定重放Redo的开始位置。需要注意的是，由于生成Checkpoint时数据库还在正常提供服务（Fuzzy Checkpoint），其中记录的活跃事务及Dirty Page信息并不一定准确，因此需要Recovery阶段通过Log内容进行修正。

Recover过程：故障恢复包含三个阶段：Analysis，Redo和Undo。Analysis阶段的任务主要是利用Checkpoint及Log中的信息确认后续Redo和Undo阶段的操作范围，通过Log修正Checkpoint中记录的Dirty Page集合信息，并用其中涉及最小的LSN位置作为下一步Redo的开始位置RedoLSN。同时修正Checkpoint中记录的活跃事务集合（未提交事务），作为Undo过程的回滚对象；Redo阶段从Analysis获得的RedoLSN出发，重放所有的Log中的Redo内容，注意这里也包含了未Commit事务；最后Undo阶段对所有未提交事务利用Undo信息进行回滚，通过Log的PrevLSN可以顺序找到事务所有需要回滚的修改。

除此之外，ARIES还包含了许多优化设计，例如通过特殊的日志记录类型CLRs避免嵌套Recovery带来的日志膨胀，支持细粒度锁，并发Recovery等。[3]认为，ARIES有两个主要的设计目标：

Feature：提供丰富灵活的实现事务的接口：包括提供灵活的存储方式、提供细粒度的锁、支持基于Savepoint的事务部分回滚、通过Logical Undo以获得更高的并发、通过Page-Oriented Redo实现简单的可并发的Recovery过程。
Performance：充分利用内存和磁盘介质特性，获得极致的性能：采用No-Force避免大量同步的磁盘随机写、采用Steal及时重用宝贵的内存资源、基于Page来简化恢复和缓存管理。


### Shadow Paging

System R的磁盘数据采用Page为最小的组织单位，一个File由多个Page组成，并通过称为Direcotry的元数据进行索引，每个Directory项纪录了当前文件的Page Table，指向其包含的所有Page。采用Shadow Paging的文件称为Shadow File，如下图中的File B所示，这种文件会包含两个Directory项，Current及Shadow。
事务对文件进行修改时，会获得新的Page，并加入Current的Page Table，所有的修改都只发生在Current Directory；事务Commit时，Current指向的Page刷盘，并通过原子的操作将Current的Page Table合并到Shadow Directory中，之后再返回应用Commit成功；事务Abort时只需要简单的丢弃Current指向的Page；如果过程中发生故障，只需要恢复Shadow Directory，相当于对所有未提交事务的回滚操作。Shadow Paging很方便的实现了：

Durability of Updates：事务完成Commit后，所有修改的Page已经落盘，合并到Shadow后，其所有的修改可以在故障后恢复出来。
Failure Atomic：回滚的事务由于没有Commit，从未影响Shadow Directory，因此其所有修改不可见。
首先，不支持Page内并发，一个Commit操作会导致其Page上所有事务的修改被提交，因此一个Page内只能包含一个事务的修改；其次，不断修改Page的物理位置，导致很难将相关的页维护在一起，破坏局部性；另外，对大事务而言，Commit过程在关键路径上修改Shadow Directory的开销可能很大，同时这个操作还必须保证原子；最后，增加了垃圾回收的负担，包括对失败事务的Current Pages和提交事务的Old Pages的回收。


### WAL

由于传统磁盘顺序访问性能远好于随机访问，采用Logging的故障恢复机制意图利用顺序写的Log来记录对数据库的操作，并在故障恢复后通过Log内容将数据库恢复到正确的状态。简单的说，每次修改数据内容前先顺序写对应的Log，同时为了保证恢复时可以从Log中看到最新的数据库状态，要求Log先于数据内容落盘，也就是常说的Write Ahead Log，WAL。除此之外，事务完成Commit前还需要在Log中记录对应的Commit标记，以供恢复时了解当前的事务状态，因此还需要关注Commit标记和事务中数据内容的落盘顺序。根据Log中记录的内容可以分为三类：Undo-Only，Redo-Only，Redo-Undo。

Undo-Only Logging

Undo-Only Logging的Log记录可以表示未<T, X, v>，事务T修改了X的值，X的旧值是v。事务提交时，需要通过强制Flush保证Commit标记落盘前，对应事务的所有数据落盘，即落盘顺序为Log记录->Data->Commit标记。恢复时可以根据Commit标记判断事务的状态，并通过Undo Log中记录的旧值将未提交事务的修改回滚。我们来审视一下Undo-Only对Durability及Atomic的保证：

Durability of Updates：Data强制刷盘保证，已经Commit的事务由于其所有Data都已经在Commit标记之前落盘，因此会一直存在；
Failure Atomic：Undo Log内容保证，失败事务的已刷盘的修改会在恢复阶段通过Undo日志回滚，不再可见。
然而Undo-Only依然有不能Page内并发的问题，如果两个事务的修改落到一个Page中，一个事务提交前需要的强制Flush操作，会导致同Page所有事务的Data落盘，可能会早于对应的Log项从而损害WAL。同时，也会导致关键路径上过于频繁的磁盘随机访问。

Redo-Only Logging

不同于Undo-Only，采用Redo-Only的Log中记录的是修改后的新值。对应地，Commit时需要保证，Log中的Commit标记在事务的任何数据之前落盘，即落盘顺序为Log记录->Commit标记->Data。恢复时同样根据Commit标记判断事务状态，并通过Redo Log中记录的新值将已经Commit，但数据没有落盘的事务修改重放。

Durability of Updates：Redo Log内容保证，已提交事务的未刷盘的修改，利用Redo Log中的内容重放，之后可见；
Failure Atomic：阻止Commit前Data落盘保证，失败事务的修改不会出现在磁盘上，自然不可见。
Redo-Only同样有不能Page内并发的问题，Page中的多个不同事务，只要有一个未提交就不能刷盘，这些数据全部都需要维护在内存中，造成较大的内存压力。

Redo-Undo Logging

可以看出的只有Undo或Redo的问题，主要来自于对Commit标记及Data落盘顺序的限制，而这种限制归根结底来源于Log信息中对新值或旧值的缺失。因此Redo-Undo采用同时记录新值和旧值的方式，来消除Commit和Data之间刷盘顺序的限制。

Durability of Updates：Redo 内容保证，已提交事务的未刷盘的修改，利用Redo Log中的内容重放，之后可见；
Failure Atomic：Undo内容保证，失败事务的已刷盘的修改会在恢复阶段通过Undo日志回滚，不再可见。
如此一来，同Page的不同事务提交就变得非常简单。同时可以将连续的数据攒着进行批量的刷盘已利用磁盘较高的顺序写性能。

***ARIES***

其中提出的ARIES逐步成为磁盘数据库实现故障恢复的标配，ARIES本质是一种Redo-Undo的WAL实现。 Normal过程：修改数据之前先追加Log记录，Log内容同时包括Redo和Undo信息，每个日志记录产生一个标记其在日志中位置的递增LSN（Log Sequence Number）；数据Page中记录最后修改的日志项LSN，以此来判断Page中的内容的新旧程度，实现幂等。故障恢复阶段需要通过Log中的内容恢复数据库状态，为了减少恢复时需要处理的日志量，ARIES会在正常运行期间周期性的生成Checkpoint，Checkpoint中除了当前的日志LSN之外，还需要记录当前活跃事务的最新LSN，以及所有脏页，供恢复时决定重放Redo的开始位置。需要注意的是，由于生成Checkpoint时数据库还在正常提供服务（Fuzzy Checkpoint），其中记录的活跃事务及Dirty Page信息并不一定准确，因此需要Recovery阶段通过Log内容进行修正。

Recover过程：故障恢复包含三个阶段：Analysis，Redo和Undo。Analysis阶段的任务主要是利用Checkpoint及Log中的信息确认后续Redo和Undo阶段的操作范围，通过Log修正Checkpoint中记录的Dirty Page集合信息，并用其中涉及最小的LSN位置作为下一步Redo的开始位置RedoLSN。同时修正Checkpoint中记录的活跃事务集合（未提交事务），作为Undo过程的回滚对象；Redo阶段从Analysis获得的RedoLSN出发，重放所有的Log中的Redo内容，注意这里也包含了未Commit事务；最后Undo阶段对所有未提交事务利用Undo信息进行回滚，通过Log的PrevLSN可以顺序找到事务所有需要回滚的修改。

除此之外，ARIES还包含了许多优化设计，例如通过特殊的日志记录类型CLRs避免嵌套Recovery带来的日志膨胀，支持细粒度锁，并发Recovery等。[3]认为，ARIES有两个主要的设计目标：

Feature：提供丰富灵活的实现事务的接口：包括提供灵活的存储方式、提供细粒度的锁、支持基于Savepoint的事务部分回滚、通过Logical Undo以获得更高的并发、通过Page-Oriented Redo实现简单的可并发的Recovery过程。
Performance：充分利用内存和磁盘介质特性，获得极致的性能：采用No-Force避免大量同步的磁盘随机写、采用Steal及时重用宝贵的内存资源、基于Page来简化恢复和缓存管理。


### REDO LOG （8.0）

1、首先，REDO的维护增加了一份写盘数据，同时为了保证数据正确，事务只有在他的REDO全部落盘才能返回用户成功，REDO的写盘时间会直接影响系统吞吐，显而易见，REDO的数据量要尽量少
2、其次，系统崩溃总是发生在始料未及的时候，当重启重放REDO时，系统并不知道哪些REDO对应的Page已经落盘，因此REDO的重放必须可重入，即REDO操作要保证幂等。
3、最后，为了便于通过并发重放的方式加快重启恢复速度，REDO应该是基于Page的，即一个REDO只涉及一个Page的修改。

#### 1，需要基于正确的Page状态上重放REDO

由于在一个Page内，REDO是以逻辑的方式记录了前后两次的修改，因此重放REDO必须基于正确的Page状态。然而InnoDB默认的Page大小是16KB，是大于文件系统能保证原子的4KB大小的，因此可能出现Page内容成功一半的情况。InnoDB中采用了 ***Double Write Buffer*** 的方式来通过写两次的方式保证恢复的时候找到一个正确的Page状态。这部分会在之后介绍 Buffer Pool的时候详细介绍。

#### 2，需要保证REDO重放的幂等

Double Write Buffer 能够保证找到一个正确的Page状态，我们还需要知道这个状态对应REDO上的哪个记录，来避免对Page的重复修改。为此，InnoDB给每个REDO记录一个全局唯一递增的标号 ***LSN(Log Sequence Number)***。Page在修改时，会将对应的REDO记录的LSN记录在Page上（FIL_PAGE_LSN字段），这样恢复重放REDO时，就可以来判断跳过已经应用的REDO，从而实现重放的幂等。

#### 3，根据REDO记录不同的作用对象，可以将这65中REDO划分为三个大类：作用于Page，作用于Space以及提供额外信息的Logic类型。

#### 4，REDO是如何组织的

***逻辑REDO层***
这一层是真正的REDO内容，REDO由多个不同Type的多个REDO记录收尾相连组成，有全局唯一的递增的偏移sn，InnoDB会在全局log_sys中维护当前sn的最大值，并在每次写入数据时将sn增加REDO内容长度。

***物理REDO层***
磁盘是块设备，InnoDB中也用Block的概念来读写数据，一个Block的长度OS_FILE_LOG_BLOCK_SIZE等于磁盘扇区的大小512B，
每次IO读写的最小单位都是一个Block。
除了REDO数据以外，Block中还需要一些额外的信息，包括12字节的Block Header：
前4字节中Flush Flag占用最高位bit，标识一次IO的第一个Block，剩下的31个个bit是Block编号；之后是2字节的数据长度，取值在[12，508]；紧接着2字节的First Record Offset用来指向Block中第一个REDO组的开始，这个值的存在使得我们对任何一个Block都可以找到一个合法的的REDO开始位置；最后的4字节Checkpoint Number记录写Block时的next_checkpoint_number，用来发现文件的循环使用，这个会在文件层详细讲解。Block末尾是4字节的Block Tailer，记录当前Block的Checksum，通过这个值，读取Log时可以明确Block数据有没有被完整写盘。

Block中剩余的中间498个字节就是REDO真正内容的存放位置，也就是我们上面说的逻辑REDO。我们现在将逻辑REDO放到物理REDO空间中，由于Block内的空间固定，而REDO长度不定，因此可能一个Block中有多个REDO，也可能一个REDO被拆分到多个Block中。

由于增加了Block Header和Tailer的字节开销，在物理REDO空间中用LSN来标识偏移，可以看出LSN和SN之间有简单的换算关系，也比较好理解

	constexpr inline lsn_t log_translate_sn_to_lsn(lsn_t sn) {
	  return (sn / LOG_BLOCK_DATA_SIZE * OS_FILE_LOG_BLOCK_SIZE +
	          sn % LOG_BLOCK_DATA_SIZE + LOG_BLOCK_HDR_SIZE);
	}

SN加上之前所有的Block的Header以及Tailer的长度就可以换算到对应的LSN，反之亦然。


***文件层***
最终REDO会被写入到REDO日志文件中，以ib_logfile0、ib_logfile1…命名，为了避免创建文件及初始化空间带来的开销，InooDB的REDO文件会循环使用，


虽然通过LSN可以唯一标识一个REDO位置，但最终对REDO的读写还需要转换到对文件的读写IO，这个时候就需要表示文件空间的offset，他们之间的换算方式如下：

	  const auto real_offset =
	      log.current_file_real_offset + (start_lsn - log.current_file_lsn);

#### 5, 如何高效地写REDO

作为维护数据库正确性的重要信息，REDO日志必须在事务提交前保证落盘，否则一旦断电将会有数据丢失的可能，因此从REDO生成到最终落盘的完整过程成为数据库写入的关键路径，其效率也直接决定了数据库的写入性能。这个过程包括REDO内容的产生，REDO写入InnoDB Log Buffer，从InnoDB Log Buffer写入操作系统 Page Cache，以及REDO刷盘，之后还需要唤醒等待的用户线程完成Commit。下面就通过这几个阶段来看看InnoDB如何在高并发的情况下还能高效地完成写REDO。

REDO -> InnoDB Log Buffer -> Page Cache -> REDO刷盘 -> 唤醒用户线程Commit

***REDO产生***

我们知道事务在写入数据的时候会产生REDO，一次原子的操作可能会包含多条REDO记录，这些REDO可能是访问同一Page的不同位置，也可能是访问不同的Page（如Btree节点分裂）。InnoDB有一套完整的机制来保证涉及一次原子操作的多条REDO记录原子，即恢复的时候要么全部重放，要不全部不重放，就是这些REDO必须连续。InnoDB中通过min-transaction实现，简称mtr，需要原子操作时，调用mtr_start生成一个mtr，mtr中会维护一个动态增长的m_log，这是一个动态分配的内存空间，将这个原子操作需要写的所有REDO先写到这个m_log中，当原子操作结束后，调用mtr_commit将m_log中的数据拷贝到InnoDB的Log Buffer。


***写入InnoDB Log Buffer***
	
高并发的环境中，会同时有非常多的min-transaction(mtr)需要拷贝数据到Log Buffer，如果通过锁互斥，那么毫无疑问这里将成为明显的性能瓶颈。为此，从MySQL 8.0开始，设计了一套无锁的写log机制，其核心思路是允许不同的mtr，同时并发地写Log Buffer的不同位置。不同的mtr会首先调用 log_buffer_reserve函数，这个函数里会用自己的REDO长度，原子地对全局偏移log.sn做fetch_add，得到自己在Log Buffer中独享的空间。之后不同mtr并行的将自己的 m_log中的数据拷贝到各自独享的空间内。


***写入Page Cache***

写入到Log Buffer中的REDO数据需要进一步写入操作系统的Page Cache，InnoDB中有单独的log_writer来做这件事情。这里有个问题，由于Log Buffer中的数据是不同mtr并发写入的，这个过程中Log Buffer中是有空洞的，因此log_writer需要感知当前Log Buffer中连续日志的末尾，将连续日志通过pwrite系统调用写入操作系统Page Cache。整个过程中应尽可能不影响后续mtr进行数据拷贝，InnoDB在这里引入一个叫做link_buf的数据结构

***刷盘&唤醒用户线程***

需要用到一下线程协同完成，多用户并发mtr的场景

	N个用户线程
	1个log_writer
	1个log_closer
	1个log_flusher
	1个log_write_notifier
	1个log_flush_notifier

涉及这些线程间同步的条件变量也很多:

	writer_event
	write_events[]
	write_notifier_event
	flusher_event
	flush_events[]
	flush_notifier_event

下面结合上图说下他们之间的关系

	1、用户线程，并发写入log buffer，如果写之前发现log buffer剩余空间不足，则唤醒等在writer_event上的log_writer线程来将log buffer数据写入page cache释放log buffer空间，在此期间，用户线程会等待在write_events[]上，等待log_writer线程写完page cache后唤醒，用户线程被唤醒后，代表当前log buffer有空间写入mtr对应的redo log，将其拷贝到log buffer对应位置，然后在recent_written上更新对应区间标记，接着将对应脏页挂到flush list上，并且在recent_closed上更新对应区间标记
	
	2、log_writer，在writer_event上等待用户线程唤醒或者timeout，唤醒后扫描recent_written，检测从write_lsn后，log buffer中是否有新的连续log，有的话就将他们一并写入page cache，然后唤醒此时可能等待在write_events[]上的用户线程或者等待在write_notifier_event上的log_write_notifier线程，接着唤醒等待在flusher_event上的log_flusher线程
	
	3、log_flusher，在flusher_event上等待log_writer线程或者其他用户线程（调用log_write_up_to true）唤醒，比较上次刷盘的flushed_to_disk_lsn和当前写入page cache的write_lsn，如果小于后者，就将增量刷盘，然后唤醒可能等待在flush_events[]上的用户线程（调用log_write_up_to true)或者等待在flush_notifier_event上的log_flush_notifier

	4、log_closer，不等待任何条件变量，每隔一段时间，会扫描recent_closed，先前推进，recent_closed.m_tail代表之前的lsn已经都挂在flush_list上了，用来取checkpoint时用。

还有log_write_notifier和log_flush_notifier线程，注意到上面有两个条件变量是数组：write_events[]，flush_events[]，他们默认有2048个slot，这么做是为什么呢？

根据上面描述可以看到，等待在这两组条件变量上的线程只有用户线程（调用log_write_up_to），每个用户线程其实只关心自己需要的lsn之前的log是否被写入，是否被刷盘，如果这里用的是一个全局条件变量，很多时候不相关lsn的用户线程会被无效唤醒，为了降低无效唤醒，InnoDB这里做了细分，lsn会被映射到对应的slot上，那么就只需要wait在该slot就可以了。这样log_writer和log_flusher在完成写page cache或者刷盘后会判断：如果本次写入的lsn区间落在同一个slot上，那么就唤醒该slot上等待的用户线程，如果跨越多个slot，则唤醒对应的log_write_notifier和log_flush_notifier线程，让他们去扫描并唤醒lsn区间覆盖的所有slot上的用户线程，这里之所以将多slot的唤醒交给专门的notifier来异步做，应该是想减小当lsn跨度过大时，log_writer和log_flusher在此处的耗时




数据文件的第一个Page类型为FIL_PAGE_TYPE_FSP_HDR，在创建一个新的表空间时进行初始化(fsp_header_init)，该page同时用于跟踪随后的256个Extent(约256MB文件大小)的空间管理，所以每隔256MB就要创建一个类似的数据页，类型为FIL_PAGE_TYPE_XDES ，XDES Page除了文件头部外，其他都和FSP_HDR页具有相同的数据结构，可以称之为Extent描述页，每个Extent占用40个字节，一个XDES Page最多描述256个Extent。


buf_page_get_gen



mtr commit函数代码流


从上图可以看出, mtr_t 存储了 Impl::mtr_buf_t, mtr_buf_t 是512长度的 dyn_buf_t 数组, dyn_buf_t 又存储了 block_t的链表 block_list_t,
block_t 存储了 m_data REDO真正数据。

也就是说，每个mtr可以包含多个 dyn_buf_t控制体，dyn_buf_t维护redo block链表。

redo 的block需要转化为 buffer pool的 buf_block_t,     block = reinterpret_cast<buf_block_t *>(slot->object);




flushed_to_disk_lsn
Buf_next_to_write_

mtr commit函数代码流




log_buffer_write_completed


  while (!log.recent_written.has_space(start_lsn)) {
    os_event_set(log.writer_event);
    ++wait_loops;
    std::this_thread::sleep_for(std::chrono::microseconds(20));
  }


log_advance_ready_for_write_lsn

log_advance_ready_for_write_lsn



link_buf 是一个循环使用的数组，对每个lsn取模可以得到其在link_buf上的一个槽位，在这个槽位中记录REDO长度。另外一个线程从开始遍历这个link_buf，通过槽位中的长度可以找到这条REDO的结尾位置，一直遍历到下一位置为0的位置，可以认为之后的REDO有空洞，而之前已经连续，这个位置叫做link_buf的tail。下面看看log_writer和众多mtr是如何利用这个link_buf数据结构的。这里的这个link_buf为log.recent_written，如下图所示：

图中上半部分是REDO日志示意图，write_lsn是当前log_writer已经写入到Page Cache中日志末尾，current_lsn是当前已经分配给mtr的的最大lsn位置，而buf_ready_for_write_lsn是当前log_writer找到的Log Buffer中已经连续的日志结尾，
从write_lsn到buf_ready_for_write_lsn是下一次log_writer可以连续调用pwrite写入Page Cache的范围，
而从buf_ready_for_write_lsn到current_lsn是当前mtr正在并发写Log Buffer的范围。

下面的连续方格便是log.recent_written的数据结构，可以看出由于中间的两个全零的空洞导致buf_ready_for_write_lsn无法继续推进，接下来，假如reserve到中间第一个空洞的mtr也完成了写Log Buffer，并更新了log.recent_written，如下图：



srv_start -> log_start_background_threads



mtr_t::Command::execute() -> prepare_write
							-> log_buffer_reserve -> log_buffer_s_lock_enter_reserve -> log.sn.fetch_add(len) 
												  -> if (end_sn > log.buf_limit_sn.load()) // 空间不足 -> log_wait_for_space_after_reserving -> log_wait_for_space_in_log_buf -> log_write_up_to -> log_wait_for_write							-> for_each_block -> mtr_write_log_t -> log_buffer_write // redo写入 redo log buffer
																 -> log_buffer_write_completed -> !while(log.recent_written.has_space()) -> os_event_set(log.writer_event)  // Redo没空间了，立即write page cache 来释放。
																 							   -> log.recent_written.add_link_advance_tail() 
																 							   -> os_event_set(log.closer_event) // 如果有线程在等待 flush_list 上数据 刷脏
							-> log_wait_for_space_in_log_recent_closed -> while (!log.recent_closed.has_space(lsn)){}
							-> add_dirty_blocks_to_flush_list  // 数据脏页加到 buffer pool的 flush list
							-> log_buffer_close -> log_buffer_s_lock_exit_close -> log.recent_closed.add_link_advance_tail() 





// 持久化的最大事务ID，这个值不是实时写入的，而是256次递增写一次



--

# 传统 RDS MySQL 扩容

完全 手动扩容，用户在使用中，通过 cpu 内存、甚至 磁盘预警，被动接收到 告警，

开始登录  平台，查看 实例的资源情况，

1、 cpu  cpu资源不足，通常影响有限
  a、cpu 按照预设值 已经预先赠送 客户一部分buffer。
  b、cpu扩容 可以热升级

2、 内存， mysqld oom，影响很大，会触发容灾【容灾判断条件很复杂，下文细说】
	a、采用了 一些用户态的小程序，来提前重启实例，比如 earlyoom，在触发 操作系统 oom killer之前，就先重启 mysqld。
		【可能在容灾探测的时间周期 和 earlyoom 拉起mysqld时间周期，时间gap可能两种情况都会发生】
	b、mysqld oom 还是发生了的话，
		1、earlyoom没有触发，mysqld 僵死，网络连通，但不对外提供服务。【这种情况，很坑，容灾模块较难易发现，集群对外始终 不能提供服务】
		2、oom killer 运行，mysqld 挂掉，由proxy或者sentinel 探测，强制拉起 mysqld。同时在 单点停服时间gap内，集群已经容灾到备库节点。影响时间为容灾时间
		3、TODO

Sentinel [容灾模块] TODO

HaProxy
Sentinel

3、磁盘 磁盘满，写失败。按96% 提前告警
		1、proxy 寻找空间匹配的磁盘实例，本地扩容 重启服务？、跨实例扩容，先切换主备【主备切换，下文细说】，数据迁移扩容
		2、sentinel 云主机 扩容。理论上，云主机 完成云盘的升级，降级 还需要拷贝数据到新盘，要停服，容灾。
			a、升级磁盘，热升级磁盘 还有点风险；
			b、静态升级磁盘：



--
***冷兵器时代的资源扩容***，已经不能满足，k8s serverless 在自动化 缩扩容方面的建设，才是发展方向。

# Aurora serverless v2 在按需扩容的能力实现上，是如何做到的？？？？


an on-demand, auto-scaling configuration for Amazon Aurora that now supports even the most demanding applications and database workloads

a、Scale instantly in a fraction of a second
	秒级扩容 如何实现？？
b、Scale in fine-grained increments (细粒度)
	细粒度增长，如何探测细分？？
c、full breadth of Aurora capabilities
	全部支持，产品化覆盖
d、up to 90% cost savings vs provisioning for peak
	自然实现了成本的节省


--
## Aurora Global Database 

使用 Server-Agent 进程对，在多路复用的前提下，实现 Redo同时同步到 RO(Read-Only) 和 Storage ！！RW


## Fast Clones

Aurora 使用存储计算分离架构。Fast Clone 本质是分布式存储中的Copy-On-Write快照技术的应用。

Clone Storage 并不需要完全复制一份数据，而是只保留对原数据块的引用。Primary Storage 和 Clone Storage 都可以独立的修改数据，
只有 产生了Diff的数据块才会真正的被 Copy和 Write。

基于Fast Clone，Reporting服务可以基于任意时间点的数据，进行计算和修改(Clone Storage)，即不会影响集群上的核心业务，也不会一次性产生大量的存储空间占用。


## Buffer Pool Resizing
MySQL 官方本身支持 动态BP，Aurora可能做了一层面向 ServerLess的封装，比如扩缩容步长、Min/Max限制等。

a、默认 75% for BP, 25% for heap
b、BP根据数据库capacity动态扩缩容
c、innodb_buffer_pool_size for MySQL; shared_buffers for PostgreSQL
d、BP 缩容采用LFU和LRU结合的淘汰机制

--
## Aurora Serverless v1

亚马逊按实际需求量自动动态分配资源。 在 Aurora Serverless 中，用ACU (Aurora Compute Unit) 代表分配资源的最小单元，通常为2G内存加配套的CPU和网络资源；
Aurora 根据实际需求，动态增加或者减少 ACU的数量，遇到剑锋时刻自动扩容，遇到流量低谷，自动缩容。

## Aurora Serverless v2

v2 缩短了扩容时间到毫秒、缩容到1分钟；扩缩容都按0.5个ACU为步长生效；且支持Aurora所以功能。

同时支持 ServerLess 和 Provision 两种模式，即可以很好的应对 写稳定读随机的新闻类服务，又可以支持IoT场景，写波动剧烈读稳定的。
稳定的操作配置 Provision，波动剧烈的操作配置 ServerLess模式。看起来很 NICE。

Primary Region 用预制配置，Secondary Region 用 ServerLess模式，一般 Secondary Region 集群通常用来支付 对延时、数据一致性要求不高的读请求，用了 ServerLess可以做到***极致的成本控制***。

## Multi-AZ high Availability
Aurora 支持一个 Writer 和多个 Reader共享一个存储，并且多个 Reader可是不同规格。
对于多个只读，Aurora 会给他们指定 Failover Tier，分别为0-15，优先级认为是HA高可用。

Tier 0 (Writer)
Tier 1 (Reader)
...
Tier 14 (Reader)
Tier 15 (Reader)

其中，Tier 0-1 和Writer同规格，会随Writer一起扩缩容，这样Failover会优先选择 Tier 0-1。Tier (2-15)可以是更小的规格。


## 基于 Multi-AZ 和 Serverless v2 & Provision

在 Primary Region 配置 Provision 的Writer，在配置 Provision 的Reader作为和主节点同配置的HA节点。其他 Secondary Region的 Reader 都配置成 ServerLess模式。



--

# 不同数据库内存管理差异
## Oracle

PGA (Program Global Area):

SGA (Server Global Area):
包含了很多不同的内存结构，如Buffer Cache、Redo Log Buffer、Shared Pool、Large Pool、Java Pool、Strems Pool等等。
	在Shared Pool中又包含了Library Cache、Data Dictionary Cache、Result Cache、Reserved Pool等数据结构。

单从Cache来说，在Oracle中有Buffer Cache（缓存数据页）、Library Cache（缓存sql、存储过程、计划等对象）、Result Cache（缓存查询结果）等多种不同类型的Cache，尽管它们在结构上来说都是Cache，但是在实现上却非常不一样。

Database Buffer Cache:
	例如Buffer Cache中缓存的数据都是定长的内存页，并且可能会对内存页进行修改，修改后的页就成为脏页，留待后台进程在合适的时候将其刷到磁盘上；

Shared Pool -> Server Result Cache
	而Result Cache中缓存的是变长的查询结果，通常是只读的，当数据发生变更时需要使查询结果失效。


因此Oracle, 将Buffer Cache和其他Cache分在不同的模块中进行管理，
在Oracle 10g中引入了Automatic Shared Memory Management（ASMM），DBA只需要设置SGA的总大小，Oracle可以对SGA内部各个模块的内存使用进行自动调整；更进一步地，在Oracle 11g中引入了Automatic Memory Management（AMM），DBA只要设置总内存占用，Oracle可以对SGA和PGA的大小进行自动调整。



Oracle 的 Buffer Cache 淘汰机制 很有意思！！

通常来说，在Oracle中Buffer Cache会占用大部分的内存，在结构上Buffer Cache是一个由定长内存块（由参数DB_BLOCK_SIZE指定，默认为8KB）组成的链表，通过LRU算法进行淘汰，同时由DBW进程将比较冷的脏页定期刷到磁盘。

在经典LRU算法中，每次数据读取都需要把记录移动到链表头，这意味着每次读取都需要对链表加写锁，这对于高并发的数据读取是不友好的，因此Oracle对经典的LRU算法做了改进。

在Oracle中，一条LRU链表在逻辑上被分为两半，一半为热链，一半为冷链。同时会在每个数据块上记录Buffer Touch Counts，(Touch Counts 可以认为是三秒连续访问的热度累加值，累加值越高，说明连续访问属性越高) 

当一个数据块被Pin住时（通常是读取或者是写入），会判断这个数据块的Touch Counts是否在3s之前增加过，如果没有增加过那么就对Touch Counts加1，注意在这里并没有将数据块移动到链表头，所以也就不需要对LRU链表加锁，提升了高并发下数据读取的性能。当对Buffer Cache有新增写入，但是Buffer Cache中没有free的内存块时，Oracle会先从冷链尾部寻找可以淘汰的内存块，如果对应内存块的Touch Counts较大，那么就将这个内存块移动到热链上，同时Touch Counts归0；如果对应内存块的Touch Counts较小，那么就会被淘汰用于新数据的写入，新写入的数据会被放入冷链头。

## MySQL

对比 MySQL的 LRU 可以写下 对比分析。

InnoDB Buffer Pool 管理数据Page 单位16KB，对应 Query Cache（对应Oracle的Result Cache）来缓存查询结果，但需要手动设置缓存大小，buffer pool和querycache；
类似 oracle, InnoDB 内存 LRU page链表，分为 Young(占比 5/8 热数据，这部分page访问不会对链表加锁，提高热数据的并发访问性能) 和Old Buffer(比较冷的数据)。
新页从磁盘独到 buffer pool，先添加到old buffer的头部，当再次被访问，提升到young buffer头部。


## OceanBase

见原文



--
# 数据库、分布式数据库 中的 数据一致性分析

 
## 不同的数据库模型

### 单副本单客户端 
一个最简单的存储系统，
只有一个客户端（单进程）和一个服务端（单进程服务）
客户端顺序发起读写操作，服务端也顺序处理每个请求，那么无论从服务器视角还是从客户端视角，后一个操作都可以看到前一个操作的结果，以前面一个操作的结果的基础上做读写。


### 单副本多客户端
系统变的复杂一些，系统还是单个服务进程（单副本）多客户端并发读写
这个模型下，多个客户端的操作会互相影响，比如一个客户端会读到不是自己写的数据（另一个客户端写入的）。
一般单机并发程序就是这样的模型，比如多个线程共享内存的程序中。

### 多副本单客户端
系统向另外一个方向变的复杂一些，为了让后端存储系统更健壮（目的不仅如此），我们可以让两个不同的服务进程（位于不同的机器上）同时存储同一份数据的拷贝，
然后，通过某种同步机制让这两个拷贝的数据保持一致。
这就是我们所说的“多副本”。
假设还是一个客户端进程顺序发起读写操作，每个操作理论上可以发给两个副本所在的任意一个服务。那么，如果副本之间是数据同步不及时，就可能发生前面写入的数据读不到，或者前面读到的数据后面读不到等情况。


### 更进一步，多副本多客户端
在一类真实的系统中，实际存在多个同时执行读写操作的客户端同时读写多个副本的后端存储服务。
如果这些不同客户端的读写操作涉及到 同一个数据项（比如，文件系统中，同一个文件的同一个位置范围；或者数据库系统中同一个表的同一行）
那么他们的操作会互相影响，同时写副本，两份存储都是数据的副本。

比如，A、B两个客户端，操作同一行数据，A修改这行，是不是要求B立即能够读到这个最新数据呢？副本之间通过Raft/Paxos同步，那么肯定还是要有Master节点和Slave节点的区分的。  在多副本的系统中，如果不要求上述保证，那么可能可以允许A和B分别操作不同的副本，再通过Raft或者Paxos同步数据一致性。


### OceanBase 的情况 (数据分区)

前面的系统模型中，假设每个服务进程拥有和管理着一份“全量”的数据。而更复杂的系统中，每个服务进程中，只服务整个数据集的一个子集

数据库表和表的分区是分散在多机上提供服务，每个服务进程只负责某些表分区的一个副本。
在这样的系统中，如果一个读写操作涉及到的多个分区位于多个服务进程中，可能出现更复杂的读写语义的异常情况。
如果一个读写操作，涉及到的多个分区位于多个服务进程中，比如一个写操作，修改了两个不同分区的副本的不同数据项，随后，一个读操作同时读取这两个数据项，是否同时都可以读到这次修改？



讨论的分布式知识点:

1、数据分为多个分片(N)，存储在多台服务节点上
2、每个分片有多个副本(M)，存储在不同的服务节点上；N * M个节点
3、许多客户端并发访问系统，执行读写操作，每个读写操作在系统中需要花费不等的时间；进一步增加复杂度，Raft一致性的延时问题？
4、除非下文中特别注明和讨论，读写操作是原子的；只能基于读写操作的原子性。

分布式说涉及的一致性，都和多副本有关。

--
### 数据库里面的数据一致性(Consistency)

数据库中的一致性(consistency)，是指事务所观察到的一致性。整个数据库 在满足一定的约束条件下，才认为是正确的。

事务的隔离性(isolation)，是对并发执行的事务来说，能多大程度的看到彼此。
那么隔离性就有多种隔离级别，RR(repeatable-read)、RC(read-committed)、脏读(read-uncommitted)、串行化(serializable)，类似多线程中，对共享数据的访问需要解决的竞争问题。

在实际系统中，事务是由一组读写操作组成，原子的事务的中间状态，是可能被并发的别的事务观察到的。如 RC，只要事务中的数据commit了，对于别的并发事务是可见的。

所以，分布式涉及的一致性，是不包含 数据库底层纬度的数据一致性的，认为读写操作在服务端都是一瞬间完成的，读写本身具有原子性！这一点很重要，不然 多副本很难展开建设。



### 客户端视角的一致性模型 （写得特别好）


在多副本的存储系统中，无论采用什么样的多副本同步协议，为了保证多个副本能够一致，本质上都要求做到：

1、同一份数据的所有副本，都能够接收到全部写操作（无论需要花费多久时间）。保证WAL日志 在各个节点前滚，或者事务在各个节点最终执行?
2、所有副本要以某种确定顺序执行这些写操作。为什么，因为总要分master和slave?


客户视角的一致性模型定义了下面4种不同的保证。

1、单调读。如果一个客户端读到了数据的某个版本n，那么之后它读到的版本必须大于等于n。读某个数据，数据版本必须单调递增。
2、读自己所写。如果一个客户端写了某个数据的版本n，那么它之后的读操作必须读到大于等于版本n的数据。
3、单调写。单调写保证同一个客户端的两个不同写操作，在所有副本上都以他们到达存储系统的相同的顺序执行。单调写可以避免写操作被丢失。 到达不同副本，当然要保持相同的写顺序。
4、读后写。读后写一致性，保证一个客户端读到版本n数据后（可能是其他客户端写入的），随后的写操作必须要在版本号大于等于n的副本上执行。 这一点，保证了客户端可以在别的客户端写的数据基础上，再更新数据。

系统对外提供的不同的一致性级别，实际上提供了这其中某几个保证。不同的一致性级别，限定了系统允许的操作执行顺序，以及允许读到多旧的数据。

为什么要定义不同的一致性级别呢？
对用户来说，当然越严格的一致性越好，在异常和复杂场景下，严格的一致性级别可以极大地简化应用的复杂度。

但是天下没有免费的午餐，一般来说，越严格的一致性模型，意味着性能（延迟）、可用性或者扩展性（能够提供服务的节点数）等要有所损失。



PACELC theorem

在分布式系统中，CAP 中 一定要容忍P 分区的存在，那么 除了consistent一致性和 Available可用性，只能保证一个之外，
还要考虑到更实际的 分区复制之间的的 latency延时问题 和 consistent一致性的 权衡问题。比如 一主多从，
主节点写完数据，为了保证数据一致性，考虑到分区延时，等待各个从节点数据同步完成，再返回写成功，那么就是为了同步数据一致性，牺牲了写的时效性。


比如 MySQL的半同步复制，认为一个从库和主库数据一致，即可返回写成功，这既是一种折中的方案。


NoSQL 系统的最终一致性，对同一行数据的写操作允许乱序写，只要写操作完整执行完毕，最终各个副本的数据会保持一致。

而关系型数据库的Insert和Update的修改语句，内部实现都需要读数据、计算最终写数据。

并且，NoSQL数据库 仅提供单行操作的原子性。而关系型数据库的基本操作是一条SQL语句，SQL语句天生是多行操作，并且有多语句事务，需要提供原子性。

所以，关系型数据库的多副本一致性，需要考虑同步算法，会比NoSQL 复制很多。



Azure Cosmos DB 的一致性级别
	-- Consistency levels in Azure Cosmos DB

Azure Cosmos DB2 是一个支持多地部署的分布式NoSQL数据库服务。它提供了丰富的可配置的一致性级别。以下五种一致性级别，从前向后可以提供更低的读写延迟，更高的可用性，更好的读扩展性。

1. Strong consistency (强一致性) 

保证读操作总是可以读到最新版本的数据（即可线性化）
写操作需要同步到多数派副本后才能成功提交。读操作需要多数派副本应答后才返回给客户端 (这里不止是多数派日志落盘完成，日志应用也需要完成)。读操作不会看到未提交的或者部分写操作的结果，并且总是可以读到最近的写操作的结果。
保证了全局的（会话间）单调读，读自己所写，单调写，读后写
读操作的代价比其他一致性级别都要高，读延迟最高

2. Bounded staleness consistency (有界旧一致性)

保证读到的数据最多和最新版本差 K个版本
通过维护一个滑动窗口，在窗口之外，有界旧一致性保证了操作的全局序。此外，在一个地域内，保证了单调读。

3. Session consistency (会话一致性)

在一个会话内保证 单调读，单调写，和读自己所写，会话之间不保证
会话一致性能够提供把读写操作的版本信息维护在客户端会话中，在多个副本之间传递
会话一致性的读写延迟都很低

4. Consistent prefix consistency (前缀一致性)

前缀一致保证，在没有更多写操作的情况下，所有的副本最终会一致
前缀一致保证，读操作不会看到乱序的写操作。例如，写操作执行的顺序是`A, B, C`，那么一个客户端只能看到`A`, `A, B`, 或者`A, B, C`，不会读到`A, C`，或者`B, A, C`等。
在每个会话内保证了单调读

5. Eventual consistency (最终一致性)

最终一致性保证，在没有更多写操作的情况下，所有的副本最终会一致
最终一致性是很弱的一致性保证，客户端可以读到比之前发生的读更旧的数据
最终一致性可以提供最低的读写延迟和最高的可用性，因为它可以选择读取任意一个副本

Cosmos DB的文档中提到了一个有趣的数字。大约有73%的用户使用会话一致性级别，有20%的用户使用有界旧一致性级别。



OceanBase 实现机制


OceanBase 使用 Multi-Paxos分布式共识算法，在多个副本之间同步事务提交日志(WAL)，每个修改事务，在多数派副本应答后(注意只保证多数派副本上日志落盘，而不要求完成了日志回放)，才认为提交成功；
通过投票机制，一个投票周期只能有一个副本被选为Leader，所有的修改语句都在Leader副本执行，达成多数派的WAL要包含Leader副本自己；
在通常情况下，数据库需要保证强一致性语义（和单机数据库类比），我们的做法是，读写语句都在主副本上执行。
Leader副本宕机的时候，通过多数派副本选出新Leader副本，此时，已完成的每个事务一定至少有一个副本记录了它的提交日志；
新的Leader副本通过和其他副本通信(grpc)获取到所有已提交事务的日志，进而完成数据恢复。-- 类似Etcd Raft的实现。


如果一个语句的执行涉及到多个表的分区，在OceanBase中这些分区的主副本可能位于不同的服务节点上。
严格的数据库隔离级别要求涉及多个分区的读请求看到的是一个“快照”，也就是说，不允许看到部分事务。这要求维护某种形式的全局读版本号，开销较大。
如果应用允许，可以调整 ***读一致性*** 级别，系统保证读到最新写入的数据，但是不同分区上的数据不是一个快照。
从一致性级别来看，这也是强一致性级别，但是打破了数据库事务的 ACID属性。


在使用数据库的互联网业务中，有很多情况下业务组件还允许读到稍旧的数据，OceanBase提供两种更弱的一致性级别。

1、在最弱的级别下，我们可以利用所有副本提供读服务。在OceanBase的实现中，多副本同步协议只保证日志落盘，并不要求日志在多数派副本上完成回放（写入存储引擎的memtable中）。

所以，利用任意副本提供读服务时，即使对于同一个分区的多个副本，每个副本完成回放的数据版本也是不同的，这样可能会导致读操作读到比之前发生的读更旧的数据。也就是说，这种情况下提供的是[最终一致性]。

当任意副本宕机的时候，客户端可以迅速重试其他副本，甚至当多数派副本宕机的时候还可以提供这种读服务。 -- 只提供最终数据一致性。那么就会出现，多次请求 返回的数据很可能会读到多版本的旧数据。

2、但是，实际上，使用关系数据库的应用，大多数还是不能容忍乱序读的。通过在数据库连接内记录读版本号，我们还提供了比最终一致性更严格的 [前缀一致性]。它可以在每个数据库连接内(connection)，保证单调读。

这种模式，一般用于OceanBase 集群内读库的访问，业务本身是读写分离的架构。

3、此外，对于这两种弱一致性级别，用户可以通过配置，控制允许读到多旧的数据。在多地部署OceanBase的时候，跨地域副本数据之间的延迟是固有的。

比如，用户配置允许读到30秒内的数据，那么只要本地副本的延迟小于30秒，则读操作可以读取本地副本。如果不能满足要求，则读取主副本所在地的其他副本。如果还不能满足，则会读取主副本。

这样的方式可以获得最小的读延迟，以及比强一致性读更好的可用性。这样，在同时保证会话级别的单调读的条件下，我们提供了 [有界旧一致性级别]。



注意，这些弱一致性级别都是放松了读操作的语义，而所有的写操作都需要写入主副本节点。所以，单调写和读后写总是保证的，但是读自己所写是不保证的。

理论上，对于后几种弱一致性级别中的每一种，我们也可以提供读到的数据是不是保证“快照”的两种不同语义，但是这违反了 ACID语义，所以并没有提供。

综上所述，OceanBase在保证关系数据库完备的 ACID事务语义前提下，提供了强一致性、有界旧一致性、前缀一致性和最终一致性这几种一致性级别。



--

## 分布式数据划分

数据划分的最终目的是负载均衡，尽可能充分利用集群资源。
分布式系统中往往也有一个全局管控节点，真正执行负载均衡操作，将划分后的数据分片按照一定策略分散到整个集群。

数据划分:

数据划分，不大不小。
不大：
1、分片数据量不大，迁移复制单个数据分片的成本不高，便于根据实时负载情况动态负载均衡；
2、没有访问热点，热点数据通常尽量减少迁移复制；
不小：
1、单个分片不能太小，太小会增大元数据的存储管理开销；100MB-1GB，宿主机磁盘容量的 0.1%差不多；
2、并且 上面数据量大小，通常大部分数据分片不活跃，不怎么需要负载均衡。



事务划分:

对于 分布式数据库，对于无共享 (share-nothing) 的分布式数据库，采用两阶段提交协议(2PC)，实现分布式事务；
那么要处理两阶段事务耗时问题，集群一致耗时等。

处理这个问题，一是尽可能优化两阶段提交事务的性能，二是数据划分尽量把经常一起操作的数据分布在同一台机器上，从而降低跨机器事务概率。


--

## 原生分布式 VS 分库分表

分库分表的劣势:

1、全局一致性：
分库分表，将表拆分为多个表格分散在多台机器，可能一台的schema已经更新，而另外一台还没有更新，无法保证原子性；
分库分表，无法获取整个系统的全局一致快照；
分库分表，无法提供全局索引功能，例如MySQL的自增ID

把复杂度留给了使用方。
2、可扩展性
3、跨机器事务支持
4、HTAP能力
5、SQL功能完备性


-- 

## OceanBase vs 分库分表


### 数据一致性

先看分库分表的主备高可用架构:

1、一主多从+半同步: 当主库写入数据，会等待至少一个从库返回 ACK信号之后，主库才会返回客户端写成功，切换时保证至少有一台从库和主库有一样的数据。
等待 ACK信号阻塞了用户线程的返回，

TDSQL对半同步复制做了优化，增加了线程池，定义了用户线程和工作线程，当binlog发送到从库后，将用户线程会话缓存起来，工作线程继续处理别的请求，待从库的ACK返回后，唤醒之前的用户线程，返回用户写成功。
-- 对用户线程做了挂起再唤醒，实现异步化半同步复制。

2、主库A 在包含新事务的binlog未同步到任意从库的情况下宕机，待A作为新从库加入到集群后，包含该新事务的Redo完成了前滚，主要A和集群中的别的节点的数据出现不一致。
-- TDSQL 做了优化，再回滚掉这个新事务，保证集群数据一致性。




### 分布式事务

分布式事务的一些问题：

-- 首先搞清楚第一个问题，这里应用在分布式事务的两阶段提交 不是 MySQL内核所指的binlog和redolog落盘的两阶段提交。

在标准的分布式两阶段提交，分为两个步骤:

1、协调者向 参与者们发送 prepare请求，参与者执行事务但并不提交
2、协调者根据参与者返回的状态，如果都返回 prepare ok，则发送 commit请求，保证成员数据一致


标准的两阶段提交方式中，如果协调者宕机，会造成事务最终提交的阻塞。分为下面几种情况

发送 prepare 请求前宕机，没有影响，相当于事务没有向集群同步，自身也不会提交事务；
发送 prepare 请求后 (或者收到所有参与者的prepare ok回包后)宕机，参与者概率已经收到请求(如果它没有宕机)，prepare事务后，参与者们会发送 prepare ok后，没有收到协调者的回包，也不会再重试发送请求；
发送 commit 请求后宕机，同时参与者也宕机，那么便无法知道事务最终状态。


OceanBase 的优势点

OceanBase 的协调者和参与者都是Paxos组，可以保证多数派存活，实现高可用就不存在协调者和参与者宕机的问题。
因为协调者是高可用的，可以不写日志记录自己的状态(协调者"无状态")，在发生切换主副或宕机恢复后，它虽然不知道自己之前的状态，但参与者发送prepare ok后，会定时重试发送 prepare ok 消息给协调者，事务在集群中接着完成提交。


容灾

其实两种方案都可以做到同城三机房、两地三中心、三地五中心，但OB中对于存储成本做了优化，OB副本集做了三种，分别是: 全能、只读、日志型副本。

全能型副本和只读型副本的区别，只读副本不参与投票，投票职责由日志型副本代替，
所以 只读副本资源消耗也大，并且还不能写，不参与投票候选，只作为被动从Leader同步数据的节点。

对于三地五中心，其中一个数据中心部署日志型副本，节省资源；任何一个城市的数据中心同时故障，集群基于Paxos都可以保证高可用。


对比优势:
1、普通分库发布 完成两阶段提交需要，Proxy协调者写日志记录状态，
2、因为参与者也是MySQL实例，要改源码才能支持消息重传机制



## 在没有分布式原生数据库之前，如何使用分库分表来解决数据分片的需求


众所周知，单库读写瓶颈以及修复代价使得分库分表方案十分必要。

垂直切分通常是分库或者拆表，水平切分通常是把数据做切分。

### 数据的水平切分

1、简单的分库分表(按ID分)
缺点1：通常新增的活跃数据都在一个库里，会导致热点过于集中，通常我们会采用打散数据的方式应对热点集中问题，
可是当需要扩容时候，就需要大量数据迁移，于是一致性hash方案会更好兼容这两个点，再结合对冷热数据点的分布式存储。

2、比较常用的取模分库分表
（1）根据表的数据增加库的数量

	DB_0: tb_0/tb_1/tb_2/tb_3

	那么数据这样分 id为数据自增ID
	DB='DB_0'
	TB='tb_' + id%4 

	当需要扩容时，增加一个数据库 DB_1

	DB='DB_' + ((id%4)/2)
	TB='tb_' + id%4

	DB_0: tb_0/tb_1
	DB_1: tb_2/tb_3

	数据继续飙升，2^n 增加DB数量，这次加2个DB DB_2/DB_3

	DB="DB_"+id%4
	TB=“tb_"+id%4

	DB_0: tb_0
	DB_1: tb_1
	DB_2: tb_2
	DB_3: tb_3

	为了扩容不迁移数据，但明显已经达到数据库数量上限了；并且单表数量很大。


(2) 成本增加表数量

	DB_0: tb_0/tb_1

数量到1千万后，停服增加一个DB_1，同时增加两张表 tb_0_1,tb_1_1，把tb_1的数据迁移到 DB_1;
tb_0 和 tb_0_1 放在DB_0中，tb_1 和 tb_1_1 放到DB1中， (第二次扩容的表命名 tb_x_1)
DB = 'DB_' + id%2
tb:
if(id < 1千万) { return 'tb_' + id % 2 }
else if(id >= 1千万) { return 'tb_'+ id % 2 + '1' }
	

数据量继续飙升到2千万，按照这种做法，我们需要增加两个库(DB_2,DB_3)和四张表(tb_0_2,tb_1_2,tb_2_2,tb_3_2)，


思路大概是这样的，设定旧DB数据量MAX值，把原DB里的右表，作为有数据的母表迁移到新的DB，再把新表加入到新旧DB的右表，新表的ID数据范围即为MAX值+


### 常用的分库分表中间件

sharding
OneProxy(支付宝首席架构师楼方鑫开发)
MyCAT（基于阿里开源的Cobar产品而研发）
vitess（谷歌开发的数据库中间件）




## 分库分表面临的问题
分布式事务，跨节点Join、GroupBy、OrderBy等都需要交给应用端，分表读取数据作为结果集，再计算数据；数据迁移虽然使用2的整数倍取余，避免了行级别的数据迁移，
但依然需要表级别的数据迁移，同时对扩容规模和分表数量都有限制，反应了Sharding扩容的难度。

### 分布式事务
前文提到的两阶段提交，也分析过优劣，事务补偿延时保证幂等性，对于对于A要求高，C延时可以容忍的系统来说，
系统只要在一定时间周期内达到最终一致性即可。常见的方式 需要对数据做对账检查，定期和数据源做同步。






