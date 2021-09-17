
#include <关于副本集的概念>

副本集是一种在多台机器同步数据的进程，副本集体提供了数据冗余，扩展了数据可用性。在多台服务器保存数据可以避免因为一台服务器导致的数据丢失。
也可以从硬件故障或服务中断解脱出来，利用额外的数据副本，可以从一台机器致力于灾难恢复或者备份。

在一些场景，可以使用副本集来扩展读性能，客户端有能力发送读写操作给不同的服务器。也可以在不同的数据中心获取不同的副本来扩展分布式应用的能力。
mongodb副本集是一组拥有相同数据的mongodb实例，主mongodb接受所有的写操作，所有的其他实例可以接受主实例的操作以保持数据同步。
主实例接受客户的写操作，副本集只能有一个主实例，因为为了维持数据一致性，只有一个实例可写，主实例的日志保存在oplog。

Client Application Driver
  Writes  Reads
    |   |
    Primary
  |Replication|Replication
Secondary    Secondary

二级节点复制主节点的oplog然后在自己的数据副本上执行操作，二级节点是主节点数据的反射，如果主节点不可用，会选举一个新的主节点。
默认读操作是在主节点进行的，但是可以指定读取首选项参数来指定读操作到副本节点。
可以添加一个额外的仲裁节点（不拥有被选举权），使副本集节点保持奇数，确保可以选举出票数不同的主节点。仲裁者并不需要专用的硬件设备，
仲裁者节点一直会保存仲裁者身份。

........异步复制........
副本节点同步主节点操作是异步的，然而会导致副本集无法返回最新的数据给客户端程序。

........自动故障转移........
如果主节点10s以上与其他节点失去通信，其他节点将会选举新的节点作为主节点。
拥有大多数选票的副节点会被选举为主节点。
副本集提供了一些选项给应用程序，可以做一个成员位于不同数据中心的副本集。
也可以指定成员不同的优先级来控制选举。

#include <副本集的结构及原理>

心跳检测:
整个集群需要保持一定的通信才能知道哪些节点活着哪些节点挂掉。
mongodb节点会向副本集中的其他节点每两秒就会发送一次pings包，如果其他节点在10秒钟之内没有返回就标示为不能访问。
每个节点内部都会维护一个状态映射表，表明当前每个节点是什么角色、日志时间戳等关键信息。
如果是主节点，除了维护映射表外还需要检查自己能否和集群中内大部分节点通讯，如果不能则把自己降级为secondary只读节点。

数据同步:
副本集同步分为初始化同步和keep复制。
初始化同步指全量从主节点同步数据，如果主节点数据量比较大同步时间会比较长。
而keep复制指初始化同步过后，节点之间的实时同步一般是增量同步。

初始化同步不只是在第一次才会被触发，有以下两种情况会触发：
1）secondary第一次加入，这个是肯定的。
2）secondary落后的数据量超过了oplog的大小，这样也会被全量复制。


1）主节点
    负责处理客户端请求, 读、写数据, 记录在其上所有操作的 oplog;

2）从节点
    定期轮询主节点获取这些操作,然后对自己的数据副本执行这些操作,从而保证从节点的数据与主节点一致。
    默认情况下,从节点不支持外部读取,但可以设置;
    副本集的机制在于主节点出现故障的时候,余下的节点会选举出一个新的主节点,从而保证系统可以正常运行。

3）仲裁节点
    不复制数据,仅参与投票。由于它没有访问的压力,比较空闲,因此不容易出故障。
    由于副本集出现故障的时候,存活的节点必须大于副本集节点总数的一半,否则无法选举主节点,或者主节点会自动降级为从节点,整个副本集变为只读。
    因此,增加一个不容易出故障的仲裁节点,可以增加有效选票,降低整个副本集不可用的风险。
    仲裁节点可多于一个。也就是说只参与投票，不接收复制的数据，也不能成为活跃节点。


#include <副本集工作流程>

在 MongoDB 副本集中,主节点负责处理客户端的读写请求,备份节点则负责映射主节点的数据。备份节点的工作原理过程可以大致描述为,备份节点定期轮询主节点上的数据操作,
然后对 自己的数据副本进行这些操作,从而保证跟主节点的数据同步。至于主节点上的所有 数据库状态改变 的操作,都会存放在一张特定的系统表中。备份节点则是根据这些数据进
行自己的数据更新。

oplog
上面提到的数据库状态改变的操作,称为 oplog(operation log,主节点操作记录)。oplog 存储在 local 数据库的"oplog.rs"表中。
副本集中备份节点异步的从主节点同步 oplog,然后重新 执行它记录的操作,以此达到了数据同步的作用。
关于 oplog 有几个注意的地方:
1）oplog 只记录改变数据库状态的操作
2）存储在 oplog 中的操作并不是和主节点执行的操作完全一样,例如"$inc"操作就会转化为"$set"操作
3）oplog 存储在固定集合中(capped collection),当 oplog 的数量超过 oplogSize,新的操作就会覆盖旧的操作


数据同步
在副本集中,有两种数据同步方式:
1）initial sync(初始化):这个过程发生在当副本集中创建一个新的数据库或其中某个节点刚从宕机中恢复,或者向副本集中添加新的成员的时候,默认的,副本集中的节点会从离它最近
   的节点复制 oplog 来同步数据,这个最近的节点可以是 primary 也可以是拥有最新 oplog 副本的 secondary 节点。该操作一般会重新初始化备份节点,开销较大。
2）replication(复制):在初始化后这个操作会一直持续的进行着,以保持各个 secondary 节点之间的数据同步。

initial sync
当遇到无法同步的问题时,只能使用以下两种方式进行 initial sync 了
1）第一种方式就是停止该节点,然后删除目录中的文件,重新启动该节点。这样,这个节点就会执行 initial sync
   注意:通过这种方式,sync 的时间是根据数据量大小的, 如果数据量过大,sync 时间就会很长
   同时会有很多网络传输,可能会影响其他节点的工作
2）第二种方式,停止该节点,然后删除目录中的文件,找一个比较新的节点,然后把该节点目录中的文件拷贝到要 sync 的节点目录中
   通过上面两种方式中的一种,都可以重新恢复"port=33333"的节点。不在进行截图了。

副本集管理
1）查看oplog的信息 通过"db.printReplicationInfo()"命令可以查看 oplog 的信息
   字段说明:
   configured oplog size: oplog 文件大小
   log length start to end:     oplog 日志的启用时间段
   oplog first event time:      第一个事务日志的产生时间
   oplog last event time:       最后一个事务日志的产生时间
   now:                         现在的时间

2）查看 slave 状态 通过"db.printSlaveReplicationInfo()"可以查看 slave 的同步状态
  当插入一条新的数据,然后重新检查 slave 状态时,就会发现 sync 时间更新了



#include <副本集选举的过程和注意点>

Mongodb副本集选举采用的是Bully算法,这是一种协调者(主节点)竞选算法,主要思想是集群的每个成员都可以声明它是主节点并通知其他节点。
别的节点可以选择接受这个声称或是拒绝并进入主节点竞争,被其他所有节点接受的节点才能成为主节点。 
节点按照一些属性来判断谁应该胜出,这个属性可以是一个静态 ID,也可以是更新的度量像最近一次事务ID(最新的节点会胜出) 

副本集的选举过程大致如下:
1）得到每个服务器节点的最后操作时间戳。每个 mongodb 都有 oplog 机制会记录本机的操作,方便和主服务器进行对比数据是否同步还可以用于错误恢复。
2）如果集群中大部分服务器 down 机了,保留活着的节点都为 secondary 状态并停止,不选举了。
3）如果集群中选举出来的主节点或者所有从节点最后一次同步时间看起来很旧了,停止选举等待人来操作。
4）如果上面都没有问题就选择最后操作时间戳最新 (保证数据是最新的)的服务器节点作为主节点。

副本集选举的特点:
选举还有个前提条件,参与选举的节点数量必须大于副本集总节点数量的一半（建议副本集成员为奇数。最多12个副本节点,最多7个节点参与选举）
如果已经小于一半了所有节点保持只读状态。
集合中的成员一定要有大部分成员(即超过一半数量)是保持正常在线状态,3个成员的副本集,需要至少2个从属节点是正常状态。
如果一个从属节点挂掉,那么当主节点down掉 产生故障切换时,由于副本集中只有一个节点是正常的,少于一半,则选举失败。
4个成员的副本集,则需要3个成员是正常状态(先关闭一个从属节点,然后再关闭主节点,产生故障切换,此时副本集中只有2个节点正常,则无法成功选举出新主节点)。


#include <副本集数据过程>

Primary节点写入数据，Secondary通过读取Primary的oplog得到复制信息，开始复制数据并且将复制信息写入到自己的oplog。如果某个操作失败，则备份节点
停止从当前数据源复制数据。如果某个备份节点由于某些原因挂掉了，当重新启动后，就会自动从oplog的最后一个操作开始同步，同步完成后，将信息写入自己的
oplog，由于复制操作是先复制数据，复制完成后再写入oplog，有可能相同的操作会同步两份，不过MongoDB在设计之初就考虑到这个问题，将oplog的同一个操作
执行多次，与执行一次的效果是一样的。简单的说就是:

当Primary节点完成数据操作后，Secondary会做出一系列的动作保证数据的同步:
1）检查自己local库的oplog.rs集合找出最近的时间戳。
2）检查Primary节点local库oplog.rs集合，找出大于此时间戳的记录。
3）将找到的记录插入到自己的oplog.rs集合中，并执行这些操作。

副本集的同步和主从同步一样，都是异步同步的过程，不同的是副本集有个自动故障转移的功能。
其原理是：slave端从primary端获取日志，然后在自己身上完全顺序的执行日志所记录的各种操作（该日志是不记录查询操作的）,这个日志就是local数据库中的oplog.rs表，
默认在64位机器上这个表是比较大的，占磁盘大小的5%，oplog.rs的大小可以在启动参数中设定：--oplogSize 1000,单位是M。

注意：在副本集的环境中，要是所有的Secondary都宕机了，只剩下Primary。最后Primary会变成Secondary，不能提供服务。


#include <MongoDB 同步延迟问题>

当你的用户抱怨修改过的信息不改变,删除掉的数据还在显示,你掐指一算,估计是数据库主从不同步。与其他提供数据同步的数据库一样,MongoDB 也会遇到同步延迟的问题,
在MongoDB的Replica Sets模式中,同步延迟也经常是困扰使用者的一个大问题。

什么是同步延迟?
首先,要出现同步延迟,必然是在有数据同步的场合,在MongoDB 中,有两种数据冗余方式,一种是Master-Slave 模式,一种是Replica Sets模式。
这两个模式本质上都是在一个节点上执行写操作, 另外的节点将主节点上的写操作同步到自己这边再进行执行。
在MongoDB中,所有写操作都会产生 oplog,oplog 是每修改一条数据都会生成一条,如果你采用一个批量 update 命令更新了 N 多条数据, 那么抱歉,oplog 会有很多条,而不是一条。
所以同步延迟就是写操作在主节点上执行完后,从节点还没有把 oplog 拿过来再执行一次。而这个写操作的量越大,主节点与从节点的差别也就越大,同步延迟也就越大了。

同步延迟带来的问题
首先,同步操作通常有两个效果,
一是读写分离,将读操作放到从节点上来执行,从而减少主节点的压力。对于大多数场景来说,读多写少是基本特性,所以这一点是很有用的;
另一个作用是数据备份, 同一个写操作除了在主节点执行之外,在从节点上也同样执行,这样我们就有多份同样的数据,一旦主节点的数据因为各种天灾人祸无法恢复的时候,我们至少还有从节点可以依赖。
但是主从延迟问题 可能会对上面两个效果都产生不好的影响。

如果主从延迟过大,主节点上会有很多数据更改没有同步到从节点上。这时候如果主节点故障,就有两种情况:
1）主节点故障并且无法恢复,如果应用上又无法忍受这部分数据的丢失,我们就得想各种办法将这部数据更改找回来,再写入到从节点中去。可以想象,即使是有可能,那这也绝对是一件非常恶心的活。
2）主节点能够恢复,但是需要花的时间比较长,这种情况如果应用能忍受,我们可以直接让从节点提供服务,只是对用户来说,有一段时间的数据丢失了,而如果应用不能接受数据的不一致,那么就只能下线整个业务,等主节点恢复后再提供服务了。

如果你只有一个从节点,当主从延迟过大时,由于主节点只保存最近的一部分 oplog,可能会导致从节点青黄不接,不得不进行 resync 操作,全量从主节点同步数据。
带来的问题是：当从节点全量同步的时候,实际只有主节点保存了完整的数据,这时候如果主节点故障,很可能全部数据都丢掉了。




1）机器环境
182.48.115.236    master-node(主节点)
182.48.115.237    slave-node1(从节点)
182.48.115.238    slave-node2(从节点)

MongoDB 安装目录:/usr/local/mongodb
MongoDB 数据库目录:/usr/local/mongodb/data
MongoDB 日志目录:/usr/local/mongodb/log/mongo.log 
MongoDB 配置文件:/usr/local/mongodb/mongodb.conf

mongodb安装可以参考：http://www.cnblogs.com/kevingrace/p/5752382.html

对以上三台服务器部署Mongodb的副本集功能，定义副本集名称为:hqmongodb
关闭三台服务器的iptables防火墙和selinux

2）确保三台副本集服务器上的配置文件完全相同（即三台机器的mongodb.conf配置一样，除了配置文件中绑定的ip不一样）。下面操作在三台节点机上都要执行：

编写配置文件
[root@master-node ~]# cat /usr/local/mongodb/mongodb.conf
port=27017
bind_ip = 182.48.115.236                 //这个最好配置成本机的ip地址。否则后面进行副本集初始化的时候可能会失败！            
dbpath=/usr/local/mongodb/data
logpath=/usr/local/mongodb/log/mongo.log
pidfilepath=/usr/local/mongodb/mongo.pid
fork=true
logappend=true 
shardsvr=true 
directoryperdb=true
#auth=true
#keyFile =/usr/local/mongodb/keyfile
replSet =hqmongodb

编写启动脚本（各个节点需要将脚本中的ip改为本机自己的ip地址）

[root@master-node ~]# cat /usr/local/mongodb/mongodb.conf
port=27017
dbpath=/usr/local/mongodb/data
logpath=/usr/local/mongodb/log/mongo.log
pidfilepath=/usr/local/mongodb/mongo.pid
fork=true
logappend=true 
shardsvr=true 
directoryperdb=true
#auth=true
#keyFile =/usr/local/mongodb/keyfile
replSet =hqmongodb

[root@master-node ~]# cat /etc/init.d/mongodb
#!/bin/sh
# chkconfig: - 64 36
# description:mongod
case $1 in
start)
/usr/local/mongodb/bin/mongod --maxConns 20000 --config /usr/local/mongodb/mongodb.conf
;;
stop)
/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.shutdownServer()"
#/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.auth('system', '123456');db.shutdownServer()"
;;
status)
/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.stats()"
#/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.auth('system', '123456');db.stats()"
;; esac

启动mongodb
[root@master-node ~]# ulimit -SHn 655350

[root@master-node ~]# /etc/init.d/mongodb start
about to fork child process, waiting until server is ready for connections.
forked process: 28211
child process started successfully, parent exiting

[root@master-node ~]# ps -ef|grep mongodb
root     28211     1  2 00:30 ?        00:00:00 /usr/local/mongodb/bin/mongod --maxConns 20000 --config /usr/local/mongodb/mongodb.conf
root     28237 27994  0 00:30 pts/2    00:00:00 grep mongodb

[root@master-node ~]# lsof -i:27017
COMMAND   PID USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
mongod  28211 root    7u  IPv4 684206      0t0  TCP *:27017 (LISTEN)

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－
如果启动mongodb的时候报错如下：
about to fork child process, waiting until server is ready for connections.
forked process: 14229
ERROR: child process failed, exited with error number 100

这算是一个Mongod 启动的一个常见错误，非法关闭的时候，lock 文件没有干掉，第二次启动的时候检查到有lock 文件的时候，就报这个错误了。
解决方法：进入mongod启动时指定的data目录下删除lock文件，然后执行"./mongod  --repair"就行了。即
# rm -rf /usr/local/mongodb/data/mongod.lock                   //由于我的测试环境下没有数据，我将data数据目录下的文件全部清空，然后--repair
# /usr/local/mongodb/bin/mongod --repair

然后再启动mongodb就ok了！

－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－

3）对master-node主节点进行配置（182.48.115.236）         //其实，刚开始这三个节点中的任何一个都可以用来初始化为开始的主节点。这里选择以master-node为主节点
[root@master-node ~]# mongo 182.48.115.236:27017        //登陆到mongodb数据库中执行下面命令操作。由于配置文件中绑定了ip，所以要用这个绑定的ip登陆
....

3.1）初始化副本集,设置本机为主节点 PRIMARY

> rs.initiate()
{
  "info2" : "no configuration specified. Using a default configuration for the set",
  "me" : "182.48.115.236:27017",
  "ok" : 1
}
hqmongodb:OTHER> rs.conf()
{
  "_id" : "hqmongodb",
  "version" : 1,
  "protocolVersion" : NumberLong(1),
  "members" : [
    {
      "_id" : 0,
      "host" : "182.48.115.236:27017",
      "arbiterOnly" : false,
      "buildIndexes" : true,
      "hidden" : false,
      "priority" : 1,
      "tags" : {
      },
      "slaveDelay" : NumberLong(0),
      "votes" : 1
    }
  ],
  "settings" : {
    "chainingAllowed" : true,
    "heartbeatIntervalMillis" : 2000,
    "heartbeatTimeoutSecs" : 10,
    "electionTimeoutMillis" : 10000,
    "catchUpTimeoutMillis" : 2000,
    "getLastErrorModes" : {
      
    },
    "getLastErrorDefaults" : {
      "w" : 1,
      "wtimeout" : 0
    },
    "replicaSetId" : ObjectId("5932f142a55dc83eca86ea86")
  }
}

3.2）添加副本集从节点。（发现在执行上面的两个命令后，前缀已经改成"hqmongodb:PRIMARY"了，即已经将其自己设置为主节点 PRIMARY了）
hqmongodb:PRIMARY> rs.add("182.48.115.237:27017")
{ "ok" : 1 }
hqmongodb:PRIMARY> rs.add("182.48.115.238:27017")
{ "ok" : 1 }

3.3）设置节点优先级
hqmongodb:PRIMARY> cfg = rs.conf()          //查看节点顺序
{
  "_id" : "hqmongodb",
  "version" : 3,
  "protocolVersion" : NumberLong(1),
  "members" : [
    {
      "_id" : 0,
      "host" : "182.48.115.236:27017",
      "arbiterOnly" : false,
      "buildIndexes" : true,
      "hidden" : false,
      "priority" : 1,
      "tags" : {
        
      },
      "slaveDelay" : NumberLong(0),
      "votes" : 1
    },
    {
      "_id" : 1,
      "host" : "182.48.115.237:27017",
      "arbiterOnly" : false,
      "buildIndexes" : true,
      "hidden" : false,
      "priority" : 1,
      "tags" : {
        
      },
      "slaveDelay" : NumberLong(0),
      "votes" : 1
    },
    {
      "_id" : 2,
      "host" : "182.48.115.238:27017",
      "arbiterOnly" : false,
      "buildIndexes" : true,
      "hidden" : false,
      "priority" : 1,
      "tags" : {
        
      },
      "slaveDelay" : NumberLong(0),
      "votes" : 1
    }
  ],
  "settings" : {
    "chainingAllowed" : true,
    "heartbeatIntervalMillis" : 2000,
    "heartbeatTimeoutSecs" : 10,
    "electionTimeoutMillis" : 10000,
    "catchUpTimeoutMillis" : 2000,
    "getLastErrorModes" : {
      
    },
    "getLastErrorDefaults" : {
      "w" : 1,
      "wtimeout" : 0
    },
    "replicaSetId" : ObjectId("5932f142a55dc83eca86ea86")
  }
}

hqmongodb:PRIMARY> cfg.members[0].priority = 1
1
hqmongodb:PRIMARY> cfg.members[1].priority = 1
1
hqmongodb:PRIMARY> cfg.members[2].priority = 2      //设置_ID 为 2 的节点为主节点。即当当前主节点发生故障时，该节点就会转变为主节点接管服务
2
hqmongodb:PRIMARY> rs.reconfig(cfg)                 //使配置生效
{ "ok" : 1 }


说明:
MongoDB副本集通过设置priority 决定优先级,默认优先级为1,priority值是0到100之间的数字,数字越大优先级越高,priority=0,则此节点永远不能成为主节点 primay。
cfg.members[0].priority =1 参数,中括号里的数字是执行rs.conf()查看到的节点顺序, 第一个节点是0,第二个节点是 1,第三个节点是 2,以此类推。优先级最高的那个
被设置为主节点。

4）分别对两台从节点进行配置

slave-node1节点操作（182.48.115.237）
[root@slave-node1 ~]# mongo 182.48.115.237:27017
.....
hqmongodb:SECONDARY> db.getMongo().setSlaveOk()        //设置从节点为只读.注意从节点的前缀现在是SECONDARY。看清楚才设置

slave-node2节点操作（182.48.115.238）
[root@slave-node2 ~]# mongo 182.48.115.238:27017
......
hqmongodb:SECONDARY> db.getMongo().setSlaveOk()       //从节点的前缀是SECONDARY，看清楚才设置。执行这个，否则后续从节点同步数据时会报错："errmsg" : "not master and slaveOk=false",

5）设置数据库账号,开启登录验证(这一步可以直接跳过，即不开启登陆验证，只是为了安全着想)
5.1）设置数据库账号
在master-node主节点服务器182.48.115.236上面操作
[root@master-node ]# mongo 182.48.115.236:27017
......
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－
如果执行命令后出现报错： "errmsg" : "not master and slaveOk=false",
这是正常的，因为SECONDARY是不允许读写的，如果非要解决，方法如下：
> rs.slaveOk();              //执行这个命令然后，再执行其它命令就不会出现这个报错了
－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－－
hqmongodb:PRIMARY> show dbs
local  0.000GB        
hqmongodb:PRIMARY> use admin     //mongodb3.0没有admin数据库了，需要手动创建。admin库下添加的账号才是管理员账号
switched to db admin    
hqmongodb:PRIMARY> show collections

＃添加两个管理员账号,一个系统管理员:system 一个数据库管理员:administrator
＃先添加系统管理员账号,用来管理用户
hqmongodb:PRIMARY> db.createUser({user:"system",pwd:"123456",roles:[{role:"root",db:"admin"}]})
Successfully added user: {
  "user" : "system",
  "roles" : [
    {
      "role" : "root",
      "db" : "admin"
    }
  ]
}
hqmongodb:PRIMARY> db.auth('system','123456')        //添加管理员用户认证,认证之后才能管理所有数据库
1

#添加数据库管理员,用来管理所有数据库
hqmongodb:PRIMARY> db.createUser({user:'administrator', pwd:'123456', roles:[{ role: "userAdminAnyDatabase", db: "admin"}]});
Successfully added user: {
  "user" : "administrator",
  "roles" : [
    {
      "role" : "userAdminAnyDatabase",
      "db" : "admin"
    }
  ]
}
hqmongodb:PRIMARY> db.auth('administrator','123456')      //添加管理员用户认证,认证之后才能管理所有数据库
1

hqmongodb:PRIMARY> db
admin
hqmongodb:PRIMARY> show collections
system.users
system.version
hqmongodb:PRIMARY> db.system.users.find()
{ "_id" : "admin.system", "user" : "system", "db" : "admin", "credentials" : { "SCRAM-SHA-1" : { "iterationCount" : 10000, "salt" : "uTGH9NI6fVUFXd2u7vu3Pw==", "storedKey" : "qJBR7dlqj3IgnWpVbbqBsqo6ECs=", "serverKey" : "pTQhfZohNh760BED7Zn1Vbety4k=" } }, "roles" : [ { "role" : "root", "db" : "admin" } ] }
{ "_id" : "admin.administrator", "user" : "administrator", "db" : "admin", "credentials" : { "SCRAM-SHA-1" : { "iterationCount" : 10000, "salt" : "zJ3IIgYCe4IjZm0twWnK2Q==", "storedKey" : "2UCFc7KK1k5e4BgWbkTKGeuOVB4=", "serverKey" : "eYHK/pBpf8ntrER1A8fiI+GikBY=" } }, "roles" : [ { "role" : "userAdminAnyDatabase", "db" : "admin" } ] }

退出，用刚才创建的账号进行登录
[root@master-node ～]# mongo 182.48.115.236:27017 -u system -p 123456 --authenticationDatabase admin
[root@master-node ～]# mongo 182.48.115.236:27017 -u administrator -p 123456  --authenticationDatabase admin

5.2）开启登录验证
在master-node主节点服务器182.48.115.236上面操作
[root@master-node ~]# cd /usr/local/mongodb/                        //切换到mongodb主目录
[root@master-node mongodb]# openssl rand -base64 21 > keyfile      //创建一个 keyfile(使用 openssl 生成 21 位 base64 加密的字符串)
[root@master-node mongodb]# chmod 600 /usr/local/mongodb/keyfile
[root@master-node mongodb]# cat /usr/local/mongodb/keyfile          //查看刚才生成的字符串,做记录,后面要用到
RavtXslz/WTDwwW2JiNvK4OBVKxU

注意:上面的数字 21,最好是 3 的倍数,否则生成的字符串可能含有非法字符,认证失败。

5.3）设置配置文件
分别在所有节点上面操作（即三个节点的配置文件上都要修改）
[root@master-node ~]# vim /usr/local/mongodb/mongodb.conf     //添加下面两行内容
......
auth=true
keyFile =/usr/local/mongodb/keyfile

启动脚本使用下面的代码(注释原来的,启用之前注释掉的)
[root@master-node ~]# cat /etc/init.d/mongodb
#!/bin/sh
# chkconfig: - 64 36
# description:mongod
case $1 in
start)
/usr/local/mongodb/bin/mongod --maxConns 20000 --config /usr/local/mongodb/mongodb.conf
;;
stop)
#/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.shutdownServer()"
/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.auth('system', '123456');db.shutdownServer()"
;;
status)
#/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.stats()"
/usr/local/mongodb/bin/mongo 182.48.115.236:27017/admin --eval "db.auth('system', '123456');db.stats()"
;; esac

5.4）设置权限验证文件
先进入master-node主节点服务器182.48.115.236,查看 
[root@master-node ~]# cat /usr/local/mongodb/keyfile
RavtXslz/WTDwwW2JiNvK4OBVKxU                              //查看刚才生成的字符串,做记录

再分别进入两台从节点服务器182.48.115.237/238
[root@slave-node1 ~]# vim /usr/local/mongodb/keyfile       //将主节点生成的权限验证字符码复制到从节点的权限验证文件里
RavtXslz/WTDwwW2JiNvK4OBVKxU
[root@slave-node1 ~]# chmod 600 /usr/local/mongodb/keyfile

[root@slave-node2 ~]# vim /usr/local/mongodb/keyfile 
[root@slave-node2 ~]# cat /usr/local/mongodb/keyfile
RavtXslz/WTDwwW2JiNvK4OBVKxU
[root@slave-node2 ~]# chmod 600 /usr/local/mongodb/keyfile

6）验证副本集
分别重启三台副本集服务器（三台节点都要重启）
[root@master-node ~]# ps -ef|grep mongodb
root     28964     1  1 02:22 ?        00:00:31 /usr/local/mongodb/bin/mongod --maxConns 20000 --config /usr/local/mongodb/mongodb.conf
root     29453 28911  0 03:04 pts/0    00:00:00 grep mongodb
[root@master-node ~]# kill -9 28964
[root@master-node ~]# /etc/init.d/mongodb start
about to fork child process, waiting until server is ready for connections.
forked process: 29457
child process started successfully, parent exiting
[root@master-node ~]# lsof -i:27017
COMMAND   PID USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
mongod  29457 root    7u  IPv4 701471      0t0  TCP slave-node1:27017 (LISTEN)
mongod  29457 root   29u  IPv4 701526      0t0  TCP slave-node1:27017->master-node:39819 (ESTABLISHED)
mongod  29457 root   30u  IPv4 701573      0t0  TCP slave-node1:27017->master-node:39837 (ESTABLISHED)
mongod  29457 root   31u  IPv4 701530      0t0  TCP slave-node1:36768->master-node:27017 (ESTABLISHED)
mongod  29457 root   32u  IPv4 701549      0t0  TCP slave-node1:36786->master-node:27017 (ESTABLISHED)
mongod  29457 root   33u  IPv4 701574      0t0  TCP slave-node1:27017->master-node:39838 (ESTABLISHED)

然后登陆mongodb
[root@master-node ~]# mongo 182.48.115.236:27017 -u system -p 123456 --authenticationDatabase admin
.......
hqmongodb:PRIMARY> rs.status()             //副本集状态查看.也可以省略上面添加登陆验证的步骤，不做验证，直接查看集群状态。集群状态中可以看出哪个节点目前是主节点
{
  "set" : "hqmongodb",
  "date" : ISODate("2017-06-03T19:06:59.708Z"),
  "myState" : 1,
  "term" : NumberLong(2),
  "heartbeatIntervalMillis" : NumberLong(2000),
  "optimes" : {
    "lastCommittedOpTime" : {
      "ts" : Timestamp(1496516810, 1),
      "t" : NumberLong(2)
    },
    "appliedOpTime" : {
      "ts" : Timestamp(1496516810, 1),
      "t" : NumberLong(2)
    },
    "durableOpTime" : {
      "ts" : Timestamp(1496516810, 1),
      "t" : NumberLong(2)
    }
  },
  "members" : [
    {
      "_id" : 0,
      "name" : "182.48.115.236:27017",
      "health" : 1,
      "state" : 1,                         //state的值为1的节点就是主节点
      "stateStr" : "PRIMARY",              //主节点PRIMARY标记
      "uptime" : 138,
      "optime" : {
        "ts" : Timestamp(1496516810, 1),
        "t" : NumberLong(2)
      },
      "optimeDate" : ISODate("2017-06-03T19:06:50Z"),
      "infoMessage" : "could not find member to sync from",
      "electionTime" : Timestamp(1496516709, 1),
      "electionDate" : ISODate("2017-06-03T19:05:09Z"),
      "configVersion" : 4,
      "self" : true
    },
    {
      "_id" : 1,
      "name" : "182.48.115.237:27017",
      "health" : 1,
      "state" : 2,
      "stateStr" : "SECONDARY",
      "uptime" : 116,
      "optime" : {
        "ts" : Timestamp(1496516810, 1),
        "t" : NumberLong(2)
      },
      "optimeDurable" : {
        "ts" : Timestamp(1496516810, 1),
        "t" : NumberLong(2)
      },
      "optimeDate" : ISODate("2017-06-03T19:06:50Z"),
      "optimeDurableDate" : ISODate("2017-06-03T19:06:50Z"),
      "lastHeartbeat" : ISODate("2017-06-03T19:06:59.533Z"),
      "lastHeartbeatRecv" : ISODate("2017-06-03T19:06:59.013Z"),
      "pingMs" : NumberLong(0),
      "syncingTo" : "182.48.115.236:27017",
      "configVersion" : 4
    },
    {
      "_id" : 2,
      "name" : "182.48.115.238:27017",
      "health" : 1,
      "state" : 2,
      "stateStr" : "SECONDARY",
      "uptime" : 189,
      "optime" : {
        "ts" : Timestamp(1496516810, 1),
        "t" : NumberLong(2)
      },
      "optimeDurable" : {
        "ts" : Timestamp(1496516810, 1),
        "t" : NumberLong(2)
      },
      "optimeDate" : ISODate("2017-06-03T19:06:50Z"),
      "optimeDurableDate" : ISODate("2017-06-03T19:06:50Z"),
      "lastHeartbeat" : ISODate("2017-06-03T19:06:59.533Z"),
      "lastHeartbeatRecv" : ISODate("2017-06-03T19:06:59.013Z"),
      "pingMs" : NumberLong(0),
      "syncingTo" : "182.48.115.236:27017",
      "configVersion" : 4
    },
  ],
  "ok" : 1
}

注意上面命令结果中的state,如果这个值为 1,说明是主控节点(master);如果是2，说明是从属节点slave。在上面显示的当前主节点写入数据，到从节点上查看发现数据会同步。
 
当主节点出现故障的时候,在两个从节点上会选举出一个新的主节点,故障恢复之后,之前的主节点会变为从节点。从上面集群状态中开看出，当前主节点是master-node，那么关闭它的mongodb，再次查看集群状态，就会发现主节点变为之前设置的slave-node2，即182.48.115.238了！
至此,Linux 下 MongoDB 副本集部署完成。

-------------------------------------------------------------------------------------------
添加数据,来需要验证的--------------------
1）主从服务器数据是否同步,从服务器没有读写权限
a:向主服务器写入数据 ok 后台自动同步到从服务器,从服务器有数据
b:向从服务器写入数据 false 从服务器不能写
c:主服务器读取数据 ok
d:从服务器读取数据 false 从服务器不能读

2）关闭主服务器,从服务器是否能顶替
 mongo 的命令行执行 rs.status() 发现 PRIMARY 替换了主机了

3）关闭的服务器,再恢复,以及主从切换
 a:直接启动关闭的服务,rs.status()中会发现,原来挂掉的主服务器重启后变成从服务器了
 b:额外删除新的服务器 rs.remove("localhost:9933"); rs.status()
 c:额外增加新的服务器 rs.add({_id:0,host:"localhost:9933",priority:1});
 d:让新增的成为主服务器 rs.stepDown(),注意之前的 priority 投票

4）从服务器读写
 db.getMongo().setSlaveOk();
 db.getMongo().slaveOk();//从库只读,没有写权限,这个方法 java 里面不推荐了
 db.setReadPreference(ReadPreference.secondaryPreferred());// 在 复 制 集 中 优 先 读
 secondary,如果 secondary 访问不了的时候就从 master 中读
 db.setReadPreference(ReadPreference.secondary());// 只 从 secondary 中 读 , 如 果
 secondary 访问不了的时候就不能进行查询

日志查看--------------------------- 
 MongoDB 的 Replica Set 架构是通过一个日志来存储写操作的,这个日志就叫做”oplog”,
 它存在于”local”数据库中,oplog 的大小是可以通过 mongod 的参数”—oplogSize”来改变
 oplog 的日志大小。
 > use local
 switched to db local
 > db.oplog.rs.find()
 { "ts" : { "t" : 1342511269000, "i" : 1 }, "h" : NumberLong(0), "op" : "n", "ns" : "", "o" :
 { "msg" : "initiating set" } }
 
 字段说明:
 ts: 某个操作的时间戳
 op: 操作类型,如下:
 i: insert
 d: delete
 u: update
 ns: 命名空间,也就是操作的 collection name
-------------------------------------------------------------------------------------------
其它注意细节：
rs.remove("ip:port");       //删除副本
单服务器,启动时添加--auth         参数开启验证。

副本集服务器,开启--auth 参数的同时,必须指定 keyfile 参数,节点之间的通讯基于该 keyfile,key 长度必须在 6 到 1024 个字符之间,
最好为 3 的倍数,不能含有非法字符。

重新设置副本集 
rs.stepDown() 
cfg = rs.conf()
cfg.members[n].host= 'new_host_name:prot'
rs.reconfig(cfg)

副本集所有节点服务器总数必须为奇数,服务器数量为偶数的时候,需要添加一个仲裁节 点,仲裁节点不参与数副本集,只有选举权。

rs.addArb("182.48.115.239:27017") #添加仲裁节点


