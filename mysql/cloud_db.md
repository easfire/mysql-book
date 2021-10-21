
## 云原生
AWS Aurora

https://zhuanlan.zhihu.com/p/33603518

https://blog.csdn.net/huangjw_806/article/details/101756681

说到云原生数据库，就不得不提到AWS的Aurora数据库。其在2014年下半年发布后，轰动了整个数据库领域。Aurora对MySQL存储层进行了大刀阔斧的改造，将其拆为独立的存储节点(主要做数据块存储，数据库快照的服务器)。上层的MySQL计算节点(主要做SQL解析以及存储引擎计算的服务器)共享同一个存储节点，可在同一个共享存储上快速部署新的计算节点，高效解决服务能力扩展和服务高可用问题。基于日志即数据的思想，大大减少了计算节点和存储节点间的网络IO，进一步提升了数据库的性能。再利用存储领域成熟的快照技术，解决数据库数据备份问题。被公认为关系型数据库的未来发展方向之一。
 

### PolarDB之谜
计算与存储分离
PolarDB采用计算与存储分离的设计理念，满足公共云计算环境下根据业务发展弹性扩展集群的刚性需求。数据库的计算节点（Database Engine Server）仅存储元数据，

而将数据文件、Redo Log等存储于远端的存储节点（Database Storage Server）。各计算节点之间仅需同步Redo Log相关的元数据信息，极大降低了主节点和只读节点间的复制延迟，而且在主节点故障时，只读节点可以快速切换为主节点。

共享分布式存储
多个计算节点共享一份数据，而不是每个计算节点都存储一份数据，极大降低了用户的存储成本。基于全新打造的分布式块存储（Distributed Storage）和文件系统（Distributed Filesystem），存储容量可以在线平滑扩展，不会受到单个数据库服务器的存储容量限制，可应对上百TB级别的数据规模。

------

从上面的介绍来看，PolarFS采用了计算和存储分离的架构，上层的计算节点共享下层的存储池里的日志和数据。这也就是说，计算层的读写节点和只读节点共享了存储层的数据和日志。那么是不是只要将只读节点配置文件中的数据目录换成读写节点的数据目录，数据库系统就可以直接工作了呢？

问题来了。》》》》》》》为什么还是需要物理复制。

https://blog.csdn.net/huangjw_806/article/details/101756681

因此，多个计算节点共享同一份数据并不是一件容易的事情。为了解决上述问题，PolarDB显然还需要有一套完善的主从同步复制机制。

在MySQL中，原先是通过Binlog同步的方式来实现主从的逻辑复制。然而在PolarDB中，为了提升数据库本身和主备复制的性能，放弃了binlog的逻辑复制，取而代之的是基于Redo log的物理复制机制。

----

虽然物理复制能带来性能上的巨大提升，但是逻辑日志由于其良好的兼容性也并不是一无是处，所以PolarDB依然保留了Binlog的逻辑，方便用户开启。

#### PolarDB-x 仅仅是 DRDS的升级版本吗
https://zhuanlan.zhihu.com/p/333458136

DRDS，其本质是搭建在标准MySQL（阿里云上的RDS For MySQL）上的分库分表中间件，具有很高的灵活性。 PolarDB-X是使用云原生技术的分布式数据库，具有一体化的数据库体验，其存储节点是经过了高度定制的MySQL，从而提供了大量中间件无法提供的能力（使用全局MVCC的强一致的分布式事务、私有RPC协议带来的性能提升、Follower上的一致性读能力等等）。

PolarDB（这里指PolarDB For MySQL）是一个基于共享存储技术的云原生数据库。PolarDB在存储空间上可以做到很强的弹性能力，但一般使用情况下，其计算能力、写入能力依然存在单机的上限。





### kingbus
https://zhuanlan.zhihu.com/p/55807044

https://github.com/flike/kingbus

Thanks ***etcd*** for providing raft library.

Thanks ***go-mysql*** for providing mysql replication protocol parser.


