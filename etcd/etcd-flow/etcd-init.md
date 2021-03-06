etcd-code.md

## backend.go
![backend struct](https://img-blog.csdnimg.cn/2021091710544481.png)


### 启动etcd raft

	Main (server/etcdmain/main.go)
	-startEtcdOrProxyV2 (server/etcdmain/etcd.go)
	--startEtcd (server/etcdmain/etcd.go)

	// StartEtcd launches the etcd server and HTTP handlers for client/server communication.
	// The returned Etcd.Server is not guaranteed to have joined the cluster.
	// Wait on the Etcd.Server.ReadyNotify() channel to know when it completes and is ready for use.

	etcd/server/embed/etcd.go
	L97 	StartEtcd(inCfg *Config) (e *Etcd, err error) {

				L244 	etcdserver.NewServer(srvcfg)
				--> etcd/server/etcdserver/server.go

					NewServer(cfg config.ServerConfig) (srv *EtcdServer, err error) {
						L300 bootstrap(cfg)
						--> etcd/server/etcdserver/bootstrap.go
						bootstrap(cfg config.ServerConfig) (b *bootstrappedServer, err error) {
							L70 	bootstrapSnapshot() //新建 snap快照|wal目录
							L78 	bootstrapBackend()  //新建 backend/一致性 index/backendHook
							....
						}

						L315 	srv = &EtcdServer{
									v2store:            b.st,
									snapshotter:        b.ss,
									r: 					*b.raft.newRaftNode(b.ss), # 每个Node新建自己的 raft 状态机
									....
								}

						--> etcd/server/etcdserver/bootstrap.go
							(b *bootstrappedRaft) newRaftNode(ss *snap.Snapshotter, wal *wal.WAL, cl *membership.RaftCluster) *raftNode {
								L520 	raft.StartNode(b.config, b.peers) 
								--> etcd/raft/node.go
									StartNode(c *Config, peers []Peer) Node {
										L222 	NewRawNode(c)		// 初始化raft newRaft() (raft/raft.go)
										L226 	err = rn.Bootstrap(peers)
										L231 	n := newNode(rn)
										L233 	go n.run()			// select 监听多种channel状态，做相应操作（Tick等）
										-->  (n *node) run() {
												for {
													select {
														case pm := <-propc:
														case m := <-n.recvc:
														...
													}
												}
											}
									}
							}
					}


				L264 	e.Server.Start()
				--> etcd/server/etcdserver/server.go // 直接跳到 EctdServer start()
					(s *EtcdServer) start () {
						L578 	go s.run()
						--> (s *EtcdServer) run() {
								L794 	s.r.start(rh)
								--> etcd/server/etcdserver/raft.go
									(r *raftNode) start(rh *raftReadyHandler) {
										for {
											select {
												case rd := <-r.Ready():		// 处理put/get 请求
											}
										}
									}

								L829 	for {
											select {
												case ap := <-s.r.apply():
												f := func(context.Context) { s.applyAll(&ep, &ap) }
											}
										}
							}
					}


				e.servePeers()
				e.serveClients()
					初始化 Clients, 可以跳转到 etcd-put-get.md
				e.serveMetrics()
			}

	// NewServer creates a new EtcdServer from the supplied configuration. 
	// The configuration is considered static for the lifetime of the EtcdServer.

	


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


