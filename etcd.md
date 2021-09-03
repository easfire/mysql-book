## etcd 单机集群搭建
https://cloud.tencent.com/developer/article/1394643
https://developer.aliyun.com/article/765312

#### learner
https://www.jianshu.com/p/bf0af696964d
https://www.jianshu.com/p/1037fb5a63ac
https://zhuanlan.zhihu.com/p/149885990
https://github.com/etcd-io/etcd/blob/main/etcdctl/README.md
https://pandaychen.github.io/2019/10/20/ETCD-BEST-PRACTISE/


### k8s sth
https://www.huweihuang.com/kubernetes-notes/etcd/etcdctl-v3.html

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

### etcd 一致性 (kv/raft snapshot/wal)
https://my.oschina.net/fileoptions/blog/1633746



### 添加member

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
		  其余三节点，也报本地clusterid和远端clusterid不一致，只好重置整个集群节点的数据。

#### Add member learner
	features in v3.4
	要使一个新的learner node相对比较简单：member add --learner 来添加一个learner node，此时该member只是作为一个non-voting member，并能够接收leader的logs，直至追上leader。  需要同步很久？？？
	一旦learner追赶上leader进度后，使用“member promote”api来将该learner变成具有quorum的member
	对于一个learner是否能够变为voting-member则需要etcd server来验证promoted request来确保安全，并保证learner已经赶上leader的进度了。
	在etcd server没有promoted request检验之前，learner会一直作为standby node存在：Leadership不能变为leaner，并且learner不对外提供read和write（client balancer不会路由请求到learner）。也就是说learner不需要向leader发送read index请求。


### transfer leader
	>> etcdctl --endpoints "http://127.0.0.1:2579" move-leader 47a42fb96a975854
	Leadership transferred from 470f778210a711ed to 47a42fb96a975854

### snapshot数据
	>> etcdutl snapshot status -w table  etcd1/data/member/snap/db 
	需要离线查看。

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


### 启动etcd raft

	Main (server/etcdmain/main.go)
		startEtcdOrProxyV2 (server/etcdmain/etcd.go)
			startEtcd (server/etcdmain/etcd.go)
				StartEtcd (server/embed/etcd.go)


					NewServer (server/etcdserver/server.go)
						bootstrap (server/etcdserver/server.go) 建立 member/snap快照|/wal日志目录 <暂时不展开>
						srv = &EtcdServer{
							v2store:            b.st,
							snapshotter:        b.ss,
							r: 					*b.raft.newRaftNode(b.ss), # 每个Node新建自己的 raft 状态机
							}
						newRaftNode (server/etcdserver/bootstrap.go)
							StartNode (raft/node.go)
								NewRawNode (raft/rawnode.go)
									newRaft (raft/raft.go)
								Bootstrap
								newNode
								run() select 监听多种channel状态，做相应操作（Tick等）



### 从集群宕机恢复

	NewServer (server/etcdserver/server.go)
			> boostFromWAL
			> restart local member
			> enabled capabilities for version
			从配置 load 其他member
			> 发起投票
				<< "caller":"raft/raft.go:692","msg":"470f778210a711ed became follower at term 6"
			> mvcc获取当前rev
			    srv.kv = mvcc.New(srv.Logger(), srv.be, srv.lessor, mvccStoreConfig) 
				<< "caller":"mvcc/kvstore.go:405","msg":"kvstore restored","current-rev":10001

			> new Transport
			(Transporter interface, provides the functionality to send raft messages to peers, and receive raft messages
					from peers)
				tr.Start() server/etcdserver/api/rafthttp/transport.go 
					newStreamRoundTripper() streamRt   http.RoundTripper // roundTripper used by streams
					NewRoundTripper()  pipelineRt http.RoundTripper // roundTripper used by pipelines
			> AddRemote [初次加载，Remotes 为空]

			> AddPeer [依次Add其余节点]
				->startPerr()
					pipeline.start()
					p:=&peer{
						msgAppV2Writer: startStreamWriter()
						writer: startStreamWriter()

						//startStreamWriter creates a streamWrite and starts a long running go-routine that accepts
						// messages and writes to the attached outgoing connection.
							func (cw *streamWriter) run() {
								for {
									select {
										case <-heartbeatc:
										case m := <-msgc:
										case conn := <-cw.connc:
										case <-cw.stopc:
										...
									}
								}
							}
					}

					// streamReader is a long-running go-routine that dials to the remote stream
					// endpoint and reads messages from the response body returned.
					p.msgAppV2Reader.start() 
					p.msgAppReader.start()
						start(){ go ct.run()}
						func (cr *streamReader) run() {
							for {
								cr.status.activate()
									>> "caller":"rafthttp/peer_status.go:53","msg":"peer became active","peer-id":"47a42fb96a975854"}
								cr.decodeLoop(rc, t)
							}
						}


			> rafthttp连通各节点，进行选举

##### 推选候选人逻辑
		server/embed/etcd.go
			func (e *Etcd) Close()-> e.Server.Stop()-> TransferLeadership()-> longestConnected()

##### 先粗略整理下选举过程: 

		etcd3 [470f778210a711ed] 新晋 leader...
			19:03:55.719 470f778210a711ed [term 5] received MsgTimeoutNow from 72ab37cc61e2023b and starts an election to get leadership
						 470f778210a711ed is starting a new election at term 5
						 became candidate at term 6" >>> etcd3的第6轮拉票已经开始。

						 同时旧主失去leader角色;
						 "msg":"raft.node: 470f778210a711ed lost leader 72ab37cc61e2023b at term 6"

						 向其余三个member发送MsgVote请求。但其余member也被pkill。无法参与。

						 集群再次启动后，etcd3的term 轮次是6。最高，所以他会成为候选，先发优势。

			19:04:07.912 became follower at term 6

			19:04:08.512 is starting a new election at term 6
			became pre-candidate at term 6
			received MsgPreVoteResp from 470f778210a711ed at term 6

			470f778210a711ed received MsgPreVoteResp from 72ab37cc61e2023b at term 6
			470f778210a711ed has received 2 MsgPreVoteResp votes and 0 vote rejections
			470f778210a711ed received MsgPreVoteResp from 47a42fb96a975854 at term 6
			470f778210a711ed has received 3 MsgPreVoteResp votes and 0 vote rejections

			[三票通过]


			19:04:08.514 became candidate at term 7

			[三票通过]

			19:04:08.519 became leader at term 7

			[当选]


		etcd2 [72ab37cc61e2023b] 前leader败选...
			19:03:55.719 "msg":"leadership transfer starting","local-member-id":"72ab37cc61e2023b","current-leader-member-id":"72ab37cc61e2023b","transferee-member-id":"470f778210a711ed"
						 [term 5] starts to transfer leadership to 470f778210a711ed"

			为什么选了 470f778210a711ed ???
				前leader推选候选人的标准，建立peer链接并保持active时间最长的member. 
				--> 18:58:03.208 "msg":"peer became active","peer-id":"470f778210a711ed" 第一个检查peer成功。

			19:03:55.738 [term: 5] received a MsgVote message with higher term from 470f778210a711ed [term: 6] 
				etcd3 接受到transfer leadership，已经开始拉票，etcd2接收到MsgVote，发现轮次低于6，变成follower。
				--> became follower at term 6

			还是会继续拉票 (成为 pre-candicate) 直到 19:04:02.742 [shutdown]
				  // PreVote enables the Pre-Vote algorithm described in raft thesis section
				  // 9.6. This prevents disruption when a node that has been partitioned away
				  // rejoins the cluster.
				  PreVote bool
			19:04:02.720 "msg":"leadership transfer failed","local-member-id":"72ab37cc61e2023b","error":"etcdserver: request timed out, leader transfer took too long"
			停止掉与每个member的peer
			19:04:02.742 closed etcd server


			19:04:07.910 became follower at term 6"
			19:04:07.926 "msg":"starting initial election tick advance","election-ticks":10

			19:04:08.518 "msg":"72ab37cc61e2023b [term: 6] received a MsgVote message with higher term from 470f778210a711ed [term: 7]"
						 became follower at term 7


		etcd1 [47a42fb96a975854] follower 败选理由....
			19:03:55.719 "msg":"received signal; shutting down","signal":"terminated" (pkill etcd)
			19:03:55.719 skipped leadership transfer; local server is not leader
			19:04:07.868 start 
			19:04:07.910 became follower at term 5
			19:04:07.924 "msg":"starting initial election tick advance","election-ticks":10
			19:04:08.410 became pre-candidate at term 5

			19:04:08.411 [term: 5] received a MsgPreVoteResp message with higher term from 470f778210a711ed [term: 6]
			19:04:08.411 became follower at term 6  >>> 470f778210a711ed 收到.

			19:04:08.518 [term: 6] received a MsgVote message with higher term from 470f778210a711ed [term: 7]
			19:04:08.518 became follower at term 7


		etcd4 [ee287c4d42f62ecf] follower 败选理由....
			19:04:07.909 became follower at term 5

			19:04:08.110 became pre-candidate at term 5

			19:04:08.112 "msg":"ee287c4d42f62ecf [term: 5] received a MsgPreVoteResp message with higher term from 72ab37cc61e2023b [term: 6]"
						 became follower at term 6  >>> 72ab37cc61e2023b 收到.

			19:04:08.518 "msg":"ee287c4d42f62ecf [term: 6] received a MsgVote message with higher term from 470f778210a711ed [term: 7]"
						 became follower at term 7



###### 总结下来

			term 5 					term 6      										term 7
	etcd2   leader(推选etcd3)   		follower(重启) 													follower(被etcd3 第7轮的拉票降为follower)
	etcd3   follower 			candidate(被推选,发起过拉票)->follower(重启)->pre-candidate							candidiate->leader
	etcd1 	follower(重启)->pre-candidate(etcd4给投过票)  follower(被etcd3 第6轮的拉票降为follower)      				follower(被etcd3 第7轮的拉票降为follower)
	etcd4   follower(重启)->pre-candidate(etcd1给投过票)  follower(被etcd2 第6轮的拉票降为follower)      				follower(被etcd3 第7轮的拉票降为follower)


### ETCD MVCC模型
https://pandaychen.github.io/2019/10/20/ETCD-BEST-PRACTISE/



