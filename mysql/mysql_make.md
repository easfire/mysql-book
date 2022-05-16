### build mysql-8.0 debug
cmake3 --version
cmake version 3.18.2

gcc --version
gcc (GCC) 8.3.1 20191121 (Red Hat 8.3.1-5)


### 使用boost版本mysql编译
#wget https://downloads.mysql.com/archives/get/p/23/file/mysql-boost-8.0.15.tar.gz

#wget http://dl.bintray.com/boostorg/release/1.68.0/source/boost_1_68_0.tar.gz

wget https://dev.mysql.com/get/Downloads/MySQL-8.0/mysql-boost-8.0.26.tar.gz 

wget https://dev.mysql.com/get/Downloads/MySQL-5.7/mysql-boost-5.7.35.tar.gz


wget https://github.com/thkukuk/rpcsvc-proto/releases/download/v1.4/rpcsvc-proto-1.4.tar.gz 

#cmake3 . -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++  -DWITH_BOOST=/root/mysql-8.0.15/boost -DCMAKE_INSTALL_PREFIX=/usr/local/mysql-8.0.15 -DMYSQL_DATADIR=/data/mysql -DWITHOUT_CSV_STORAGE_ENGINE=1 -DWITHOUT_BLACKHOLD_STORAGE_ENGINE=1 -DWITHOUT_FEDERATED_STORAGE_ENGINE=1 -DWITHOUT_ARCHIVE_STORAGE_ENGINE=1 -DWITHOUT_MRG_MYISAM_STORAGE_ENGINE=1 -DWITHOUT_NDBCLUSTER_STORAGE_ENGINE=1 -DWITHOUT_TOKUDB_STORAGE_ENGINE=1 -DWITHOUT_TOKUDB=1 -DWITHOUT_ROCKSDB_STORAGE_ENGINE=1 -DWITHOUT_ROCKSDB=1 -DFORCE_INSOURCE_BUILD=1 -DWITH_SSL=system -DCMAKE_BUILD_TYPE=Debug -DWITH_DEBUG=1 

cmake3 . -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++  -DWITH_BOOST=/data/mysql-8.0.26/boost -DCMAKE_INSTALL_PREFIX=/usr/local/mysql-8.0.26 -DCMAKE_BUILD_TYPE=Debug -DWITH_DEBUG=1 -DFORCE_INSOURCE_BUILD=1

cmake3 . -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++  -DWITH_BOOST=/data/mysql-5.7.35/boost -DCMAKE_INSTALL_PREFIX=/usr/local/mysql-5.7.35 -DCMAKE_BUILD_TYPE=Debug -DWITH_DEBUG=1 -DFORCE_INSOURCE_BUILD=1

#### 辅助工具
wget https://github.com/thkukuk/rpcsvc-proto/releases/download/v1.4/rpcsvc-proto-1.4.tar.gz

#### DEBUG版本 -DCMAKE_BUILD_TYPE=Debug
make -j 4

make install 


###初始化mysql目录
mkdir -p /data/mysql8 /data/mysql8/share /data/mysql8/data /data/mysql8/error /data/mysql8/log 
cp /usr/local/mysql-8.0.26/share/english/errmsg.sys /data/mysql8/share

####初始化mysql数据目录
mysqld --initialize-insecure --user=root --basedir=/data/mysql8 --datadir=/data/mysql8/data --innodb-undo-tablespaces=4


### run
命令行运行，参数没有全写到 my.cnf
[5.7] my.cnf 添加 server-id=xxx

/usr/local/mysql-8.0.26/bin/mysqld --defaults-file=/data/mysql8/my.cnf --user=root --basedir=/data/mysql8 --datadir=/data/mysql8/data --socket=/data/mysql8/mysql.sock --log-error=/data/mysql8/error/errlog.sys --binlog_group_commit_sync_delay=2000 --binlog_group_commit_sync_no_delay_count=100 --skip-stack-trace &

####   gdb DEBUG方式

![断点启动 mysqld](https://pic1.zhimg.com/80/v2-083a1d3b044ff3e11b63196e83debe46_720w.png)

① 第一步， gdb打开DEBUG版本的mysqld

gdb /usr/local/mysql/bin/mysqld (需要是DEBUG 版本)

ex: gdb /usr/local/mysql-8.0.26-old/bin/mysqld

② 添加好需要调试的断点（函数名或者代码行数）

b trx_rseg_adjust_rollback_segments
或者
b /data/mysql-8.0.26/sql/conn_handler/connection_handler_per_thread.cc:301

我常用5.7和8.0.26两个版本。

run --defaults-file=/data/mysql8/my.cnf

run --defaults-file=/data/mysql57/my.cnf 

ex: run --defaults-file=/data/mysql8/my.cnf 
// 可以不加  --skip-stack-trace


![mysqld 启动](https://pica.zhimg.com/80/v2-f8e621f308bdb95a8b603669d4552b76_720w.png)

至此，可以看到 mysqld启动时线程的运行情况，并且 断点在了 指定的 breakpoint，可以开始断点调试了。（注意这里mysqld服务还没有启动起来）

![ 常用命令](https://pic3.zhimg.com/80/v2-84e759f995714a01742514acb4b21b98_720w.png)

敲finish，一直回车，直到 mysqld 启动起来。

③ 客户端连接上

ex: /usr/local/mysql-8.0.26-old/bin/mysql -uroot -h127.0.0.1


# insert 
## 直接到InnoDB insert
b row_insert_for_mysql

#### 解决 debuginfos 问题
https://developer.aliyun.com/article/727403

0x00007f6733848ca1 in poll () from /lib64/libc.so.6                                              
Missing separate debuginfos, use: yum debuginfo-install glibc-2.28-127.el8.x86_64 libgcc-8.3.1-5.1.el8.x86_64 libstdc++-8.3.1-5.1.el8.x86_64 libxcrypt-4.1.1-4.el8.x86_64 openssl-libs-1.1.1g-15.el8_3.x86_64 sssd-client-2.3.0-9.el8.x86_64 zlib-1.2.11-16.el8_2.x86_64 

安装 Missing separate debuginfos, use: debuginfo-install 解决方法
1\ 修改/etc/yum.repos.d/CentOS-Debuginfo.repo里面的debuginfo目录中enable=1
2\ yum install nss-softokn-debuginfo --nogpgcheck
3\ yum debuginfo-install glibc-2.28-127.el8.x86_64 libgcc-8.3.1-5.1.el8.x86_64 libstdc++-8.3.1-5.1.el8.x86_64 libxcrypt-4.1.1-4.el8.x86_64 openssl-libs-1.1.1g-15.el8_3.x86_64 sssd-client-2.3.0-9.el8.x86_64 zlib-1.2.11-16.el8_2.x86_64 

### client 连接报错解决 
Oracle is a registered trademark of Oracle Corporation and/or its
affiliates. Other names may be trademarks of their respective
owners.

Segmentation fault (core dumped)

vim extra/libedit/libedit-20191231-3.1/src/terminal.c 
area=buf; -> area=NULL;



