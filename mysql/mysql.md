

## MySQL源码目录简析
https://blog.csdn.net/weixin_33514277/article/details/113692268


## 双主结构
https://www.modb.pro/db/11525

## Mysql复制过滤
以下配置参数，可以设置需要复制或过滤的库表

	binlog_ignore_db
	replicate-ignore-db
	replicate-ignore-table
	replicate-do-table
	replicate-do-db

在复制中，如果启用参数 replicate-ignore-db / replicate-do-db 后想要让复制正常运行，只需在连接数据库后不执行 "use db" 语句即可

参考：

https://keithlan.github.io/2015/11/02/mysql_replicate_rule/

### MySQL Binlog和Relaylog生成和清理
参考：

https://cloud.tencent.com/developer/article/1602196

### MySQL Reset/Stop/Start/Show Slave/Master Status
第一、reset slave 命令和 reset slave all命令会删除所有的relay log（包括还没有应用完的日志），创建一个新的relay log文件；

第二、使用reset slave命令，那么所有的连接信息仍然保留在内存中，包括主库地址、端口、用户、密码等。
这样可以直接运行start slave命令而不必重新输入change master to命令，而运行show slave status也仍和没有运行reset slave一样，有正常的输出。
但如果使用reset slave all命令，那么这些内存中的数据也会被清除掉，运行show slave status就输出为空了。

第三、reset slave 和 reset slave all命令会将系统mysql数据库的slave_master_info表和slave_relay_log_info表中对应的复制记录清除。

1、reset master操作有两个功能，一个是把binary log进行清空，并生成新的编号为000001的binary log

2、reset master会清空主库的GTID编号，在搭建主从的时候可以使用，但是要慎用（如果你的二进制日志还有需要的话）。


### Mysql Explain Type分析
https://segmentfault.com/a/1190000038793131

https://mengkang.net/1124.html

### Innodb层线程并发参数
http://blog.itpub.net/7728585/viewspace-2658990/

### Mysql账户管理
https://cloud.tencent.com/developer/article/1056271

https://zhuanlan.zhihu.com/p/55798418


### Mysql权限
https://blog.csdn.net/qq_38658567/article/details/108626816

https://www.cnblogs.com/zhoujinyi/p/10939715.html

https://cloud.tencent.com/developer/article/1558721


### Mysql分区
https://www.cnblogs.com/wushaopei/p/11750541.html


### Mysql的ACID
https://www.cnblogs.com/xuanzhi201111/p/4103696.html

### MVCC (undo log+read view)
https://cloud.tencent.com/developer/article/1527179


### UDB回档
数据库回档基于Binlog和备份文件，最早回档时间取决于Binlog时间范围内的最早备份时间。
获取最早binlog位置点
    # 找到第一个binlog的开始时间
    # 1. 首先, 查看第一个binlog文件的第一个event的时间, 成功则返回
    # 2. 否则, 获取第一个binlog文件的第一个SET TIMESTAMP, 成功则返回
    # 3. 否则, 返回第一个binlog文件的修改时间

#### sql_thread回档
https://mp.weixin.qq.com/s/UemzA47xO7DYa2ohzkdHVg

https://www.modb.pro/db/61751


#### Seconds_Behind_Master是否准确?

https://blog.csdn.net/weixin_30209121/article/details/113650749

SBM计算方法：

 clock_of_slave - last_timestamp_executed_by_SQL_thread - clock_diff_with_master

### xtrabackup
https://opensource.actionsky.com/20190505-xtrabackup/

https://www.cnblogs.com/DataArt/p/10175758.html


### Mysql 开启心跳包
 监控复制关系异常的情况  
https://www.jianshu.com/p/2e051b3720ac
 
 需要开启 show_compatibility_56, 
https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_show_compatibility_56


### MySQL 延时从库
https://blog.csdn.net/L835311324/article/details/87455269


### InnoDB 多版本控制

InnoDB事务，MVCC (Multi-Version Concurrency Control) 多版本并发控制
InnoDB每行数据，都有多个版本（多事务的多数据版本实现并发读写）
多数据版本，其实就是通过 undo-log 回滚到历史数据版本。

格式可以写成  [row trx_id]
![数据记录多事务](https://img-blog.csdnimg.cn/20210829213037630.png)

看上去已经可以定义全量数据的各个版本快照了。是这样吗？

从一个新创建的事务角度，看某个数据版本。按照Mysql默认RR视图规则，
事务启动之前的，可见。之后的不可见，需要寻找上一个数据版本。
---- 但怎么可能出现之前的版本，还是这个事务的数据版本呢？？

InnoDB为每个**事务**都构建了一个数组，保存事务启动瞬间，当前启动未提交的“活跃”事务。

					当前事务	
					  |				  
			  A		  |B 	  C
			低水位	  |		高水位
	-----------|---------------|----------------
	已提交事务		未提交事务			未开始事务
			   |<-	->|	
			 当前事务的一致性视图 （consistent read-view）
				


![MVCC](https://img-blog.csdnimg.cn/20210829212723182.png)

	显然，row-trx_id落在B之前，对于当前事务都可见。	

**把三个相关概念串一下**

- 一致性读	
	- 可重复读的核心就是一致性读
- 当前读
	- 而当事务更新数据的时候，只能用当前读
- 行锁
	- 如果当前记录的行锁被其他事务占用，就需要进入锁等待

接下来再理解一下RC和RR
+ 在RR隔离级别下，只需要在事务开始时刻创建一致性视图，之后事务里的其他查询都共用这个一致性视图
+ 在RC隔离级别下，每个语句执行前都会重新计算一个新的视图，别的事务的Commit会被读到

**S锁，X锁 （共享锁，排他锁）**

		select * from t where id = 1 lock in share mode;
		select * from t where id = 1 for update;

### S锁 X锁
https://segmentfault.com/a/1190000015210634


### binlog/redolog 落盘时机

初步理解落盘顺序:

	1> redolog prepare
	2> binlog write/fsync
	3> commit

binlog

	sync_binlog
		枚举值
		0: 每次提交事务都只 write，不 fsync;
		1: 每次 commit fsync 到磁盘;
		N(N>1): 每次提交事务都 write，但累积 N 个事务后才 fsync.

redolog

	innodb_flush_log_at_trx_commit
		枚举值
		0: 每次事务提交时都只是把 redolog 留在 redo log buffer 中;
		1: 表示每次事务提交时都将 redolog 直接持久化到磁盘;
		2: 每次 commit 只需写到 page_cache. 

redolog存储过程:

	1、写 redolog buffer 内存
	2、write 文件系统 page_cache
	3、fsync 到磁盘
		步骤1、2 都很快。

	什么实际redolog会落盘?
	1> Innodb 后台进程 每秒一次刷把 redo log buffer， write 到 page_cache，再 fsync 到磁盘.
	2> 另外两种写 page_cache 的情况:
		1、redo log buffer 占到 innodb_log_buffer_size 一半，后台 write 到 page_cache.
		2、并行事务提交，
			事务A 	  	redo log 写 buffer
			事务B		commit (把事务A的redo log 也落盘) <innodb_flush_log_at_trx_commit = 1>
			---------------
	>> 所以 InnoDB任务 redolog commit 的时候，不需要再刷盘。


udb innodb_flush_log_at_trx_commit = 2; 降低磁盘IO


如果 双1， 那岂不是，每次事务提交都会产生两次磁盘写IO，

group commit 组提交的概念。 事务合并[概念] --- 类似etcd的批量事务一次性提交。

LSN (log sequence number) 每写入 length 长度的 redo log， LSN增加length; 
LSN 也会写到InnoDB的数据页上。保证数据页不会重复写 redo log。

并发事务，可以通过合并事务提交redolog，binlog，减少IO写，提高IOPS
	
	rtx1, rtx2, rtx3
	--50  --120 --160
	rtx1 作为这一组的leader，在rtx1 提交的时候，LSN < 160 的redolog都落盘了。


单线程，就不行了。

为了增加一次fsync的事务组员，真实 redolog、binlog落盘的过程如下
		
		1> redolog prepare: write
		2> binlog: write
		3> redolog prepare: fsync
		4> binlog: fsync
		5> redolog commit: write

因为 步骤3的同步过程很快，所以binlog可以合并的事务组员比较少

提升binlog组提交的效果

	--binlog_group_commit_sync_delay=2000 延时多少微秒再fsync，也可能会加大延时
	--binlog_group_commit_sync_no_delay_count=100 积累多少次事务binlog才fsync, 可能会延迟加大

	？？参数如何配置。

WAL机制似乎没有减少IO读写

	1、redolog 和 binlog 都是顺序写，顺序写比随机写速度要快；
	2、组提交还是会减少IO消耗

分析到这里，我们再来回答这个问题：
如果你的 MySQL 现在出现了性能瓶颈，而且瓶颈在 IO 上，可以通过哪些方法来提升性能呢？

针对这个问题，可以考虑以下三种方法：

	1> 设置 binlog_group_commit_sync_delay 和 binlog_group_commit_sync_no_delay_count 参数，减少 binlog 
	   的写盘次数。这个方法是基于“额外的故意等待”来实现的，因此可能会增加语句的响应时间，但没有丢失数据的风险。
	2> 将 sync_binlog 设置为大于 1 的值（比较常见是 100~1000）。这样做的风险是，主机掉电时会丢 binlog 日志。
	3> 将 innodb_flush_log_at_trx_commit 设置为 2。这样做的风险是，主机掉电的时候会丢数据。

	udb高可用 	innodb_flush_log_at_trx_commit=1 高可用binlog丢失风险更大.
	udb单点  	innodb_flush_log_at_trx_commit=2


