
[MySQL · 性能优化 · 5.7 Innodb事务系统](http://mysql.taobao.org/monthly/2014/12/01/)

调试事务中undolog

create table t (id int(11) primary key not null, c varchar(10) default '');

| id    | int(11) | NO   | PRI | NULL    |       |
| c     | int(11) | YES  |     | NULL    |       |


start transaction;
start transaction with consistent snapshot;

select * from db.t where id = 1;

update db.t set c = b where id = 1;

commit;


为了生成 undo log 多个独立 space空间，
在初始化mysql 数据目录的时候，添加 innodb-undo-tablespaces 参数

--
mkdir -p /data/mysql57 /data/mysql57/share /data/mysql57/data /data/mysql57/error /data/mysql57/log
mysqld --initialize-insecure --user=root --basedir=/data/mysql57 --datadir=/data/mysql57/data --innodb-undo-tablespaces=4


### innodb-undo-tablespaces = 4

[mysqld]
character-set-server = utf8mb4
bind_address = 0.0.0.0
default_storage_engine = InnoDB
socket = /data/mysql57/mysqld.sock
sort_buffer_size = 262144
table_open_cache = 1024
thread_cache_size = 50
innodb-undo-tablespaces = 4
default_authentication_plugin = mysql_native_password
innodb_buffer_pool_size = 2055208960
innodb_buffer_pool_instances = 4 
general-log-file = /data/mysql57/log/mysqld.log
 
user=root
basedir=/data/mysql57
datadir=/data/mysql57/data
socket=/data/mysql57/mysql.sock
log-error=/data/mysql57/error/errlog.sys
binlog_group_commit_sync_delay=2000
binlog_group_commit_sync_no_delay_count=100

#BINLOG
log_bin = /data/mysql57/log/mysql-bin
relay_log = /data/mysql57/log/mysqld-relay-bin

max_binlog_size = 500M
binlog_format = row 
gtid_mode = on
read_only = off 
relay_log_purge = 1 
sync_binlog = 1 
skip_slave_start=1
slow_query_log   = 1 
enforce-gtid-consistency = 1 
log-slave-updates = on


### 初始化 生成数据目录 undo001..undo004

-rw-r----- 1 root root       56 Dec 10 11:54 auto.cnf
-rw------- 1 root root     1676 Dec 10 11:54 ca-key.pem
-rw-r--r-- 1 root root     1112 Dec 10 11:54 ca.pem
-rw-r--r-- 1 root root     1112 Dec 10 11:54 client-cert.pem
-rw------- 1 root root     1680 Dec 10 11:54 client-key.pem
-rw-r----- 1 root root      466 Dec 10 11:54 ib_buffer_pool
-rw-r----- 1 root root 12582912 Dec 10 11:54 ibdata1
-rw-r----- 1 root root 50331648 Dec 10 11:54 ib_logfile0
-rw-r----- 1 root root 50331648 Dec 10 11:54 ib_logfile1
drwxr-x--- 2 root root     4096 Dec 10 11:54 mysql
drwxr-x--- 2 root root     8192 Dec 10 11:54 performance_schema
-rw------- 1 root root     1676 Dec 10 11:54 private_key.pem
-rw-r--r-- 1 root root      452 Dec 10 11:54 public_key.pem
-rw-r--r-- 1 root root     1112 Dec 10 11:54 server-cert.pem
-rw------- 1 root root     1676 Dec 10 11:54 server-key.pem
drwxr-x--- 2 root root     8192 Dec 10 11:54 sys
-rw-r----- 1 root root 10485760 Dec 10 11:54 undo001
-rw-r----- 1 root root 10485760 Dec 10 11:54 undo002
-rw-r----- 1 root root 10485760 Dec 10 11:54 undo003
-rw-r----- 1 root root 10485760 Dec 10 11:54 undo004



