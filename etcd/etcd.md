
#### 理论文案 
https://www.jianshu.com/p/bf0af696964d
https://www.jianshu.com/p/1037fb5a63ac
https://zhuanlan.zhihu.com/p/149885990
https://github.com/etcd-io/etcd/blob/main/etcdctl/README.md
https://pandaychen.github.io/2019/10/20/ETCD-BEST-PRACTISE/
https://blog.didiyun.com/index.php/2019/02/27/etcd-raft/
https://www.codedump.info/post/20210628-etcd-wal/

### k8s sth
https://www.huweihuang.com/kubernetes-notes/etcd/etcdctl-v3.html

****************************************************************

### etcd直观架构
![ETCD1](https://img-blog.csdnimg.cn/20210907103144477.png)


#### 串行读和线性读  (Serializable Read & Linearizable Read)

	<Serializable Read>
Leader收到写请求，将请求持久化到WAL日志，并广播到各节点，若一半以上节点持久化成功，
该请求对应的日志条目标记为已提交，etcdserver 模块异步从 Raft 模块获取已提交的日志条目，应用到状态机 (boltdb 等)。
如果读请求到某节点，该节点IO延时，可能会读到旧数据。

	<Linearizable Read>
ReadIndex, 每个Node都持久化了 RAFT_INDEX, RAFT_APPLIED_INDEX.

线性请求假如发到了节点C，首先要从 Leader 获取集群最新的已提交的日志索引 (confirmedIndex)，Leader 收到 ReadIndex 请求后，
为了防止脑裂，会向 Follower 节点发送心跳确认，一半以上节点确认 Leader 身份后才能将已提交的日志索引返回给节点C；
节点C 则会等待，直到状态机已应用索引 (appliedIndex) 大于等于 confirmedIndex时；
然后，通知读请求，节点C 数据已赶上 Leader，可以取状态机中访问数据了。

总体而言，KVServer 模块收到线性读请求后，通过向 Raft 模块发起 ReadIndex 请求，Raft 模块将 Leader 最新提交日志索引封装在 ReadState 结构体，
通过 channel 层层返回给线性读模块，线性读模块等待本节点状态机追赶上 Leader 进度，追赶完后，通知 KVServer 模块，与状态机中 MVCC 模块交互。

### ETCD MVCC模型

它核心由内存树形索引模块 (treeIndex) 和嵌入式的 KV 持久化存储库 boltdb 组成。

boltdb，它是个基于 B+ tree 实现的 key-value 键值库，支持事务，提供 Get/Put 等简易 API 给 etcd 操作
https://github.com/boltdb/bolt#getting-started

boltdb 的 key 是全局递增的版本号 (revision)，value 是用户 key、value 等字段组合成的结构体，
然后通过 treeIndex 模块来保存用户 key 和版本号的映射关系


### etcd 一致性 (kv/raft snapshot/wal)
https://my.oschina.net/fileoptions/blog/1633746

****************************************************************

## 实操记录

### add member

	>> etcdctl member add etcd-4 --peer-urls=http://127.0.0.1:2680 --learner=true       
	Member 462d837236b726b4 added to cluster 1355fcfe840232f5

	ETCD_NAME="etcd-4"
	ETCD_INITIAL_CLUSTER="etcd-4=http://0.0.0.0:2680,etcd-3=http://127.0.0.1:2580,etcd-1=http://127.0.0.1:2380,etcd-2=http://127.0.0.1:2480"
	ETCD_INITIAL_ADVERTISE_PEER_URLS="http://0.0.0.0:2680"
	ETCD_INITIAL_CLUSTER_STATE="existing"

	>> nohup etcd --config-file=etcd.conf & 
	>> etcdctl member list
	462d837236b726b4, unstarted, , http://0.0.0.0:2680, , true
	470f778210a711ed, started, etcd-3, http://127.0.0.1:2580, http://127.0.0.1:2579, false
	47a42fb96a975854, started, etcd-1, http://127.0.0.1:2380, http://127.0.0.1:2379, false
	72ab37cc61e2023b, started, etcd-2, http://127.0.0.1:2480, http://127.0.0.1:2479, false

	>> etcdctl member promote 462d837236b726b4

{"level":"warn","ts":"2021-08-31T19:20:17.509+0800","logger":"etcd-client","caller":"v3/retry_interceptor.go:63","msg":"retrying of unary invoker failed","target":"etcd-endpoints://0xc00000a1e0/#initially=[127.0.0.1:2379]","attempt":0,"error":"rpc error: code = FailedPrecondition desc = etcdserver: can only promote a learner member which is in sync with leader"}

	TODO: 通过add promote 添加learner，如何预先同步learner和leader的数据 ???
	TRY1: 新增节点的属性（learner属性）会保留在节点的/data/member数据中，粗暴地删除member数据，重启；
		  其余三节点，也报本地clusterid和远端clusterid不一致，只好重置整个集群节点的 WAL 数据。

### Add member learner
	features in v3.4
	要使一个新的learner node相对比较简单：member add --learner 来添加一个learner node，此时该member只是作为一个non-voting member，并能够接收leader的logs，直至追上leader。  需要同步很久？？？
	一旦learner追赶上leader进度后，使用“member promote”api来将该learner变成具有quorum的member

	对于一个learner是否能够变为voting-member则需要etcd server来验证promoted request来确保安全，并保证learner已经赶上leader的进度了。
	在etcd server没有promoted request检验之前，learner会一直作为standby node存在：Leadership不能变为leaner，并且learner不对外提供read和write
	（client balancer不会路由请求到learner）。也就是说learner不需要向leader发送read index请求。


### transfer leader
	>> etcdctl --endpoints "http://127.0.0.1:2579" move-leader 47a42fb96a975854
	Leadership transferred from 470f778210a711ed to 47a42fb96a975854

### snapshot数据
	>> etcdutl snapshot status -w table  etcd1/data/member/snap/db 
	需要离线查看。



#### 日志同步 (Log Replication)
	TODO

