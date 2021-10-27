
### Paxos算法
https://www.cnblogs.com/linbingdong/p/6253479.html

https://xie.infoq.cn/article/e53cbcd0e723e3a6ce4be3b8c

### TiDB 数据一致 or 数据隔离
https://tech.ipalfish.com/blog/2020/09/08/tidb_htap/

	数据库内部有OLTP和OLAP两套存储引擎，数据在两套存储引擎中复制。

	Learner <- Leader  , 行格式 转 列格式

	TiFlash: Learner 是异步接受Raft Group 的 Raft Log，那么TiFlash 如何保证数据强一致性呢?

	通过TiFlash 的线性读来实现。(类似ETCD的线性读) 

	__ 在 TiFlash 读数据的时候，TiFlash 的 Learner 只需要和 TiKV Leader 节点做一次 readindex 操作，这是一个非常轻量的操作，所以 TiKV 和 TiFlash 之间的影响会非常小。

	所以，ETCD 异步log回放，但可以保持一致性，也是通过线性读保证的吗？
	如果没有发生线性读呢？
	也就是说，再首次线性读发生后，才确保了数据的强一致性。


