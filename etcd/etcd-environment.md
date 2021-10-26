etcd-environment.md

### etcd 单机集群搭建
https://cloud.tencent.com/developer/article/1394643
https://developer.aliyun.com/article/765312

### 手动脚本启动
https://www.v2ex.com/t/623362

	etcd1.conf 配置文件:

	name: etcd-1
	data-dir: /home/ericzhang/Project/github/etcd_cluster/etcd1/data
	listen-client-urls: http://0.0.0.0:2379
	advertise-client-urls: http://127.0.0.1:2379
	listen-peer-urls: http://0.0.0.0:2380
	initial-advertise-peer-urls: http://127.0.0.1:2380
	initial-cluster: etcd-1=http://127.0.0.1:2380,etcd-2=http://127.0.0.1:2480,etcd-3=http://127.0.0.1:2580
	initial-cluster-token: etcd-cluster-my
	initial-cluster-state: new

	etcd2/etcd.conf 配置文件:

	name: etcd-2
	data-dir: /home/ericzhang/Project/github/etcd_cluster/etcd2/data
	listen-client-urls: http://0.0.0.0:2479
	advertise-client-urls: http://127.0.0.1:2479
	listen-peer-urls: http://0.0.0.0:2480
	initial-advertise-peer-urls: http://127.0.0.1:2480
	initial-cluster: etcd-1=http://127.0.0.1:2380,etcd-2=http://127.0.0.1:2480,etcd-3=http://127.0.0.1:2580
	initial-cluster-token: etcd-cluster-my
	initial-cluster-state: new

	etcd3/etcd.conf 配置文件：

	name: etcd-3
	data-dir: /home/ericzhang/Project/github/etcd_cluster/etcd3/data
	listen-client-urls: http://0.0.0.0:2579
	advertise-client-urls: http://127.0.0.1:2579
	listen-peer-urls: http://0.0.0.0:2580
	initial-advertise-peer-urls: http://127.0.0.1:2580
	initial-cluster: etcd-1=http://127.0.0.1:2380,etcd-2=http://127.0.0.1:2480,etcd-3=http://127.0.0.1:2580
	initial-cluster-token: etcd-cluster-my
	initial-cluster-state: new


### goreman 多进程管理工具启动
https://developer.aliyun.com/article/765312

	etcd1: etcd --name infra1 --listen-client-urls http://127.0.0.1:12379 --advertise-client-urls http://127.0.0.1:12379 --listen-peer-urls http://127.0.0.1:12380 --initial-advertise-peer-urls http://127.0.0.1:12380 --initial-cluster-token etcd-cluster-1 --initial-cluster 'infra1=http://127.0.0.1:12380,infra2=http://127.0.0.1:22380,infra3=http://127.0.0.1:32380' --initial-cluster-state new --enable-pprof --logger=zap --log-outputs=stderr
	etcd2: etcd --name infra2 --listen-client-urls http://127.0.0.1:22379 --advertise-client-urls http://127.0.0.1:22379 --listen-peer-urls http://127.0.0.1:22380 --initial-advertise-peer-urls http://127.0.0.1:22380 --initial-cluster-token etcd-cluster-1 --initial-cluster 'infra1=http://127.0.0.1:12380,infra2=http://127.0.0.1:22380,infra3=http://127.0.0.1:32380' --initial-cluster-state new --enable-pprof --logger=zap --log-outputs=stderr
	etcd3: etcd --name infra3 --listen-client-urls http://127.0.0.1:32379 --advertise-client-urls http://127.0.0.1:32379 --listen-peer-urls http://127.0.0.1:32380 --initial-advertise-peer-urls http://127.0.0.1:32380 --initial-cluster-token etcd-cluster-1 --initial-cluster 'infra1=http://127.0.0.1:12380,infra2=http://127.0.0.1:22380,infra3=http://127.0.0.1:32380' --initial-cluster-state new --enable-pprof --logger=zap --log-outputs=stderr



#### 配置项说明：

	--name：etcd集群中的节点名，这里可以随意，可区分且不重复就行 
	--listen-peer-urls：监听的用于节点之间通信的url，可监听多个，集群内部将通过这些url进行数据交互(如选举，数据同步等)
	--initial-advertise-peer-urls：建议用于节点之间通信的url，节点间将以该值进行通信。
	--listen-client-urls：监听的用于客户端通信的url，同样可以监听多个。
	--advertise-client-urls：建议使用的客户端通信 url，该值用于 etcd 代理或 etcd 成员与 etcd 节点通信。
	--initial-cluster-token： etcd-cluster-1，节点的 token 值，设置该值后集群将生成唯一 id，并为每个节点也生成唯一 id，当使用相同配置文件再启动一个集群时，只要该 token 值不一样，etcd 集群就不会相互影响。
	--initial-cluster：也就是集群中所有的 initial-advertise-peer-urls 的合集。
	--initial-cluster-state：new，新建集群的标志

	注意上面的脚本，etcd 命令执行时需要根据本地实际的安装地址进行配置。下面我们启动 etcd 集群。


### 压测

	+-----------------------+------------------+-----------+---------+-----------+------------+-----------+------------+--------------------+--------+
	|       ENDPOINT        |        ID        |  VERSION  | DB SIZE | IS LEADER | IS LEARNER | RAFT TERM | RAFT INDEX | RAFT APPLIED INDEX | ERRORS |
	+-----------------------+------------------+-----------+---------+-----------+------------+-----------+------------+--------------------+--------+
	| http://127.0.0.1:2579 | 470f778210a711ed | 3.6.0-pre |   20 kB |     false |      false |         3 |         13 |                 13 |        |
	| http://127.0.0.1:2379 | 47a42fb96a975854 | 3.6.0-pre |   20 kB |      true |      false |         3 |         13 |                 13 |        |
	| http://127.0.0.1:2479 | 72ab37cc61e2023b | 3.6.0-pre |   20 kB |     false |      false |         3 |         13 |                 13 |        |
	| http://127.0.0.1:2679 | ee287c4d42f62ecf | 3.6.0-pre |   20 kB |     false |      false |         3 |         13 |                 13 |        |
	+-----------------------+------------------+-----------+---------+-----------+------------+-----------+------------+--------------------+--------+

>> 假定 HOST_1 是 leader, 写入请求发到 leader，连接数1，客户端数1
benchmark --endpoints=${HOST_1} --conns=1 --clients=1 put --key-size=8 --sequential-keys --total=10000 --val-size=256

>> 假定 HOST_1 是 leader, 写入请求发到 leader，连接数100，客户端数1000

benchmark --endpoints=${HOST_1} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=100000 --val-size=256

>> 写入发到所有成员，连接数100，客户端数1000
benchmark --endpoints=${HOST_1},${HOST_2},${HOST_3} --conns=100 --clients=1000 put --key-size=8 --sequential-keys --total=100000 --val-size=256




### 看下启动项的LOG

	{
	    "level":"info",
	    "ts":"2021-08-31T17:52:28.093+0800",
	    "caller":"embed/etcd.go:307",
	    "msg":"starting an etcd server",
	    "etcd-version":"3.6.0-pre",
	    "git-sha":"a4a82cc",
	    "go-version":"go1.16.3",
	    "go-os":"linux",
	    "go-arch":"amd64",
	    "max-cpu-set":16,
	    "max-cpu-available":16,
	    "member-initialized":false,
	    "name":"etcd-1",
	    "data-dir":"/home/ericzhang/Project/github/etcd_cluster/etcd1/data",
	    "wal-dir":"",
	    "wal-dir-dedicated":"",
	    "member-dir":"/home/ericzhang/Project/github/etcd_cluster/etcd1/data/member",
	    "force-new-cluster":false,
	    "heartbeat-interval":"100ms",
	    "election-timeout":"1s",
	    "initial-election-tick-advance":true,
	    "snapshot-count":100000,
	    "snapshot-catchup-entries":5000,
	    "initial-advertise-peer-urls":[
	        "http://127.0.0.1:2380"
	    ],
	    "listen-peer-urls":[
	        "http://0.0.0.0:2380"
	    ],
	    "advertise-client-urls":[
	        "http://127.0.0.1:2379"
	    ],
	    "listen-client-urls":[
	        "http://0.0.0.0:2379"
	    ],
	    "listen-metrics-urls":[
	    ],
	    "cors":[
	        "*"
	    ],
	    "host-whitelist":[
	        "*"
	    ],
	    "initial-cluster":"etcd-1=http://127.0.0.1:2380,etcd-2=http://127.0.0.1:2480,etcd-3=http://127.0.0.1:2580",
	    "initial-cluster-state":"new",
	    "initial-cluster-token":"etcd-cluster-my",
	    "quota-size-bytes":2147483648,
	    "pre-vote":true,
	    "initial-corrupt-check":false,
	    "corrupt-check-time-interval":"0s",
	    "auto-compaction-mode":"",
	    "auto-compaction-retention":"0s",
	    "auto-compaction-interval":"0s",
	    "discovery-url":"",
	    "discovery-proxy":"",
	    "downgrade-check-interval":"5s"
	}

### dlv debug 操作

	# 二进制
	dlv exec bin/etcd 
	// 不能开启debug模式 

	# code debug
	dlv debug /home/ericzhang/Project/github/etcd/server/main.go 
		// 编译生成debug二级制 /home/ericzhang/Project/github/etcd/__debug_bin

		root     19437 24466  0 15:52 pts/1    00:00:00 dlv debug /home/ericzhang/Project/github/etcd/server/main.go
		root     19581 19437  0 15:52 pts/1    00:00:00 /home/ericzhang/Project/github/etcd/__debug_bin

		// 没有绑定 listen-url 和 advertise-url; 
		// 在dlv restart __debug_bin 进程

	r --listen-client-urls http://0.0.0.0:2379 --advertise-client-urls http://0.0.0.0:2379 




