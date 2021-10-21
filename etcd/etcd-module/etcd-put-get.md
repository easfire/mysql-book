etcd-put/get.md


### flow


### code review


etcd/client/v3/client.go
	
	L409	client.KV = NewKV(client)

在服务端初始化通过rpc调用api/v3rpc的接口模块。

etcd/server/embed/etcd.go
	L269	e.serveClients()
		L703	v3client.New(e.Server)

// New creates a clientv3 client that wraps an in-process EtcdServer. Instead of 
making gRPC calls through sockets, the client makes direct function calls 
to the etcd server through its api/v3rpc function interfaces.

		etcd/server/etcdserver/api/v3client/v3client.go
		初始化(KV,Lease,Watcher,Maintenance,Cluster,Auth)模块
		先只看KV模块，
		KV NewKVFromKVClient() 初始化KV interface的struct对象，实现(Put/Get/Delete等原子接口)
			etcd/client/v3/kv.go  
			L113	Put() { kv.Do() }
				L144	func (kv *kv) Do(ctx context.Context, op Op) (OpResponse, error) {
					switch op.t {
						case tRange:
						case tPut:
							kv.remote.Put()
						case tDeleteRange:
						case tTxn:
					}
				}

				etcd/api/etcdserverpb/rpc.pb.go
					L6499	err := c.cc.Invoke(ctx, "/etcdserverpb.KV/Put", in, out, opts...)

				client 发送rpc请求到 server，执行put操作


etcd/server/etcdserver/api/v3rpc/key.go
etcd/server/etcdserver/v3_server.go

	L135	func (s *EtcdServer) Put() {
		resp, err := s.raftRequest(ctx, pb.InternalRaftRequest{Put: r})
	}
	-processInternalRaftRequestOnce() 
	--s.r.Propose(cctx, data) 
		[etcd 是基于 Raft 算法实现节点间数据复制的，因此它需要将 put 写请求内容打包成一个提案消息，提交给 Raft 模块]

etcd/raft/node.go

	---Propose() L423 (etcd/raft/node.go) {
		n.stepWait(ctx, pb.Message{Type: pb.MsgProp, Entries: []pb.Entry{{Data: data}}}) 
	}
	----stepWithWaitOption(m pb.Message) { //发送到propc这个chan进行处理;
		ch := n.propc
		pm := msgWithResult{m: m}
		select {
			case ch <- pm:
		}
	}


etcd/raft/node.go

	L303	(n *node) run() {   // 主node
		for {
				 rd = n.rn.readyWithoutAccept() 构建 Ready对象，包含 Entries,Messages,SoftState,HardState,Snapshot 等;
				 readyc = n.readyc
				 select {
				 	case pm := <-propc:
				 		err := r.Step(m) 进入raft状态机处理 (etcd/raft/raft.go)
				 }
		}
	}

	processInternalRaftRequestOnce()函数中需要注意s.reqIDGen.Next()当该请求完成时根据该id唤醒后续处理。然后进入(n *node) Propose()函数，该函数将用户请求包装成pb.Message 结构，type类型是pb.MsgProp。进入(n *node) step（）函数，发送到propc 这个chan进行处理。

	(n *node) run（）函数接收propc面发来的MsgProp消息，然后送入raft状态机处理(r *raft) Step（）。raft StateMachine 处理完这个 MsgProp Msg 会产生 1 个 Op log entry 和 2 个发送给另外两个副本的 Append entries 的 MsgApp messages，node 模块(n *node) run会将这两个输出打包成 Ready，通过readyc <- rd 提交到etcdserver处理。

etcd/raft/raft.go

	L847	(r *raft) Step(m pb.Message) {	// raft 模块 Step 入口。
			L1421 stepFollower(r *raft, m pb.Message) {
				 case pb.MsgApp:		// 处理 MsgApp 请求
				 	r.handleAppendEntries(m) // 逻辑大致上，分析node内存和接收到Leader的msg数据，比较Term/Index/Commit等等信息，得出Reject等结果。
				 case ...
			}
	}

etcd/server/etcdserver/raft.go

	L157 	func (r *raftNode) start(rh *raftReadyHandler) { // etcdserver处理 Ready对象
			for {
			  select {
				case rd := <-r.Ready():
				rh.updateLead(rd.SoftState.Lead)
				rh.updateLeadership(newLeader)
				ap := apply{
		          entries:  rd.CommittedEntries,
		          snapshot: rd.Snapshot,
		          notifyc:  notifyc,
		        }

		   	L209  updateCommittedIndex(&ap, rh)                                                                                                                                                                                         
		        select {
		        case r.applyc <- ap: 
		        case <-r.stopped:
		          return
		        }

		        // the leader can write to its disk in parallel with replicating to the followers and them writing to their disks.
		        // Leader 可以落盘同时并发 触发 Followers 复制操作落盘
		        if islead {
          			r.transport.Send(r.processMessages(rd.Messages))
        		}

        		// Must save the snapshot file and WAL snapshot entry before saving any other entries or hardstate to ensure that recovery after a snapshot restore is possible. 
        		// 通常都是先保存快照，再落盘raftlog和state，如果宕机，保证了可以从快照恢复数据。

        		if !raft.IsEmptySnap(rd.Snapshot) {
        			L229 	r.storage.SaveSnap(rd.Snapshot)
        		}
	        	
	        	--> server/etcdserver/storage.go

	        		(st *storage) SaveSnap(snap raftpb.Snapshot) { //SaveSnap saves the snapshot file to disk and writes the WAL snapshot entry.
	        			// 先写.snap文件，再写.wal, .wal 写完毕后，.snap 会清理。

	        			L57  st.Snapshotter.SaveSnap(snap)
	        				
	        				--> etcd/server/etcdserver/api/snap/snapshotter.go

	        					(s *Snapshotter) SaveSnap(snapshot raftpb.Snapshot) {
	        						L72  s.save(&snapshot)
		        						(s *Snapshotter) save() { 
		        							err = pioutil.WriteAndSyncFile(spath, d, 0666)
		        						}	
		        					--> etcd/pkg/ioutil/util.go
		        					 WriteAndSyncFile {
			        					os.OpenFile(filename, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, perm)
			        					f.Write(data)
			        					fileutil.Fsync(f)
			        					f.Close()
		        					}
	        					}
	        					

	        			L63  st.WAL.SaveSnapshot(walsnap)
	        				--> etcd/server/storage/wal/wal.go
	        					 (w *WAL) SaveSnapshot(e walpb.Snapshot) { 
		        					L958  w.encoder.encode(rec)

		        					--> etcd/server/storage/wal/encoder.go

		        						(e *encoder) encode(rec *walpb.Record) {
		        							e.bw.Write(data)
		        						}

		        					--> etcd/pkg/ioutil/pagewriter.go

		        						(pw *PageWriter) Write(p []byte) {	// PageWriter implements the io.Writer interface so that writes will either be in page chunks or from flushing
		        							// 底层 page 写逻辑，可以再看看。
		        						}

		        					L965 	w.sync()

		        						(w *WAL) sync() error {
		        							err := fileutil.Fdatasync(w.tail().File)  // Fdatasync is similar to fsync(), but does not flush modified metadata unless that metadata is needed in order to allow a subsequent data retrieval to be correctly handled
		        							// 还做了 fsync 超时处理. 
		        						}
		        				}
	        		}

        		L236 	r.storage.Save(rd.HardState, rd.Entries)

        		--> etcd/server/storage/wal/wal.go

        		L912 (w *WAL) Save(st raftpb.HardState, ents []raftpb.Entry) {
	        			for i := range ents {
	        				w.saveEntry(&ents[i]) // 存储 entries
	        			}
	        			w.saveState(&st)	// 存储 HardState
	        			if mustSync {  // entsnum != 0 || st.Vote != prevst.Vote || st.Term != prevst.Term
	        				w.sync()
	        			}
	        			w.cut()  // cut closes current file written and creates a new one ready to append
        			}

        		L244 if !raft.IsEmptySnap(rd.Snapshot) {		//如果还存在snapshot
	        		L249 	r.storage.Sync()

	        		// etcdserver now claim the snapshot has been persisted onto the disk
	        		notifyc <- struct{}{}

	        		// ApplySnapshot overwrites the contents of this Storage object with those of the given snapshot.
					L257 	r.raftStorage.ApplySnapshot(rd.Snapshot)

					// Release releases resources older than the given snap and are no longer needed:
					// - releases the locks to the wal files that are older than the provided wal for the given snap.
					// - deletes any .snap.db files that are older than the given snap
					L261	r.storage.Release(rd.Snapshot)
        		}

				L267 	r.raftStorage.Append(rd.Entries) //Append the new entries to storage.

				if !isLead { 
					msgs := r.processMessages(rd.Messages)
					notifyc <- struct{}{}
					// 候选和跟随者要等待所有配置变更提交后，再send message.
					r.transport.Send(msgs)
				} else {
					notifyc <- struct{}{}
				}
				r.Advance()
			}
		}
	}
	
etcd/raft/node.go 

	L303	(n *node) run() {   // 其他副本node
		for {
				 select {
				 	case m := <-n.recvc:
				 		err := r.Step(m) 进入raft状态机处理 (etcd/raft/raft.go)
				 }
		}
	}

·  其他副本的返回 Append entries 的 response： MsgAppResp message，会通过 node 模块的接口经过 recvc Channel（case m := <-n.recvc:） 提交给 node 模块的 coroutine；

便于理解 APPLY 模块的执行内容 以及时序
![wal_apply_mvcc](https://img-blog.csdnimg.cn/2021092917533198.png)

server/etcdserver/server.go  [raft_node的上级server] 异步获取 raftNode的 applyc; applyAll 应用到 mvcc.KV

	L728	(s *EtcdServer) run() {
		L731	sn, err := s.r.raftStorage.Snapshot() 
		L737	sched := schedule.NewFIFOScheduler()	
		L794	s.r.start(rh)
		L829	for {
					select {
						case ap := <-s.r.apply():		// func (r *raftNode) apply() chan apply {return r.applyc} 
							f := func(context.Context) { s.applyAll(&ep, &ap) }		// 任务入队列，顺序执行，依次应用到 Mvcc.KV
							sched.Schedule(f)	
					}
				}
			}

		L898	(s *EtcdServer) applyAll(ep *etcdProgress, apply *apply) {
			L899	s.applySnapshot(ep, apply)

			--> L920	(s *EtcdServer) applySnapshot(ep *etcdProgress, apply *apply) {
					// wait for raftNode to persist snapshot onto the disk
					<-apply.notifyc
					newbe, err := serverstorage.OpenSnapshotBackend(s.Cfg, s.snapshotter, apply.snapshot, s.beHooks)    // 创建Etcd Db (boltdb)

					if s.lessor != nil { 	// 考虑lessor情况 always recover lessor before kv
						s.lessor.Recover(newbe, func() lease.TxnDelete { return s.kv.Write(traceutil.TODO()) })
					}

				L975	s.kv.Restore(newbe)		// 处理 构建 Tree Index (内容比较多)

				--> etcd/server/storage/mvcc/kvstore.go
					L299	(s *store) Restore(b backend.Backend) {
								s.kvindex = newTreeIndex(s.lg)		// 新建 TreeIndex (BTree) 保存key的多版本revision
					L320		s.restore()
							}
					L323	(s *store) restore() {
								// 开始生成 revision 信息了。
								min, max Revision
								finishedCompact		// 最近一次压缩的revision
								scheduledCompact

						L351	rkvc, revc := restoreIntoIndex(s.lg, s.kvindex)		
								L427	restoreIntoIndex(lg *zap.Logger, idx index) (chan<- revKeyValue, <-chan int64) {
									// for循环 restore tree index from streaming the unordered index

									rkvc, revc := make(chan revKeyValue, restoreChunkKeys), make(chan int64, 1)
									kiCache := make(map[string]*keyIndex, restoreChunkKeys)		//TreeIndex 缓存

									for rkv := range rkvc {
										ki, ok := kiCache[rkv.kstr]
										// purge some kiCache
										// cache miss, fetch from tree index if there
										if ok {
											ki.put()	// put a revision to the keyIndex.
										} else {
											ki.restore()
											idx.Insert(ki)
											kiCache[rkv.kstr] = ki
										}
									}
								}

							.....

							}

				L979	s.consistIndex.SetBackend(newbe)	//初始化consistIndex
						// close old backend, close txs

						// v2store 不需要了?

						s.cluster.SetBackend(schema.NewMembershipBackend(lg, newbe))	// UnsafeCreateBucket(Members/MembersRemoved/Cluster 等 Buckets)

						s.cluster.Recover(api.UpdateCapability)		// restored cluster config

						s.r.transport.RemoveAllPeers()				// recover raft transport

						for range s.cluster.Members() {
							s.r.transport.AddPeer(m.ID, m.PeerURLs)
						}

						ep.appliedt = apply.snapshot.Metadata.Term
						ep.appliedi = apply.snapshot.Metadata.Index
						ep.snapi = ep.appliedi
						ep.confState = apply.snapshot.Metadata.ConfState

						type etcdProgress struct {                                                                                                                                                                                                    
						  confState raftpb.ConfState
						  snapi     uint64
						  appliedt  uint64
						  appliedi  uint64
						} 
				}

			L900 	s.applyEntries(ep, apply)

			--> L1057	(s *EtcdServer) applyEntries(ep *etcdProgress, apply *apply) {
				  if ep.appliedt, ep.appliedi, shouldstop = s.apply(ents, &ep.confState); shouldstop {
				    go s.stopWithDelay(10*100*time.Millisecond, fmt.Errorf("the member has been permanently removed from the cluster"))
				  }

				--> L1820	(s *EtcdServer) apply {
								for i := range es {
									switch e.Type {
									    case raftpb.EntryNormal:
									      s.applyEntryNormal(&e)
									      s.setAppliedIndex(e.Index)
									      s.setTerm(e.Term)
									}
								}

					--> L1869	(s *EtcdServer) applyEntryNormal(e *raftpb.Entry) {
									index := s.consistIndex.ConsistentIndex()	// 获取 consistent Index (如果内存没有，从BoltDB获取 consistent Index 和 Term)
									if e.Index > index {
										// 更新当前的 Index
										s.consistIndex.SetConsistentIndex(e.Index, e.Term)
									}
									L1923	ar = s.applyV3.Apply(&raftReq, shouldApplyV3)  

						etcd/server/etcdserver/apply.go 	// Apply 模块

								--> L134	(a *applierV3backend) Apply(r *pb.InternalRaftRequest, shouldApplyV3 membership.ShouldApplyV3) *applyResult {
												switch {
													case r.Put != nil:
														a.s.applyV3.Put(context.TODO(), nil, r.Put)
												}
											}
											L250	(a *applierV3backend) Put (ctx context.Context, txn mvcc.TxnWrite, p *pb.PutRequest) {
												// 先写trace
												L269	txn = a.s.KV().Write(trace)	//构造Write tx
												// 后面处理了 Ignore的参数情况
												L301	resp.Header.Revision = txn.Put(p.Key, val, leaseID)
											}

						etcd/server/storage/mvcc/kvstore_txn.go		// 终于进入Mvcc 存储模块

								--> L108 	(tw *storeTxnWrite) Put(key, value []byte, lease lease.LeaseID) {
												tw.put(key, value, lease)
												return tw.beginRev + 1
											}
											L182 	(tw *storeTxnWrite) put(key, value []byte, leaseID lease.LeaseID) {
													// 看看 (key: revision, value: KeyValue) 写 boltdb 过程。
													// if the key exists before, use its previous created and get its previous leaseID
													_, created, ver, err := tw.s.kvindex.Get(key, rev)	// 如果key在 treeIndex 存在，使用它之前的 main, leaseID
													tw.tx.UnsafeSeqPut(schema.Key, ibytes, d) 
															// schema.Key(keyBucketName), ibytes: revision 字节流作为 Key, d: mvccpb.KeyValue().Marshal()作为 Value 
													tw.s.kvindex.Put(key, idxRev) 	// 更新 TreeIndex.

													// 最后 attach lease to kv pair
											}

						}
					}
			}
			
			// wait for the raft routine to finish the disk writes before triggering a
			// snapshot. or applied index might be greater than the last index in raft
			// storage, since the raft routine might be slower than apply routine.

			// APPLY执行快照，要等WAL模块执行完毕后，再执行。

	  		L908 	<-apply.notify 
	  		L910	s.triggerSnapshot(ep)

	  		--> (s *EtcdServer) triggerSnapshot(ep *etcdProgress) {

	  				L1097 	s.snapshot(ep.appliedi, ep.confState)
	  				--> (s *EtcdServer) snapshot(snapi uint64, confState raftpb.ConfState) {
	  					L2033 	clone := s.v2store.Clone()	// the store for v2

	  					L2043 	s.KV().Commit() 	// KV().commit() updates the consistent index in backend

	  					s.GoAttach(func() { 	// 这里开启goroutine
		  					L2048	d, err := clone.SaveNoCopy() 	// json.Marshal store 对象

		  					L2054 	snap, err := s.r.raftStorage.CreateSnapshot(snapi, &confState, d)	// 创建 snapshot的 Data/MetaData

		  					L2064	s.r.storage.SaveSnap(snap) 
		  					L2067	s.r.storage.Release(snap)

		  					// When sending a snapshot, etcd will pause compaction.  Pausing compaction avoids triggering a snapshot sending cycle.
		  					L2081	if atomic.LoadInt64(&s.inflightSnapshots) != 0 {	// skip compaction since there is an inflight snapshot
		  								return
		  							}

		  					// keep some in memory log entries for slow followers 
		  					L2087	compacti := uint64(1)
		  							if snapi > s.Cfg.SnapshotCatchUpEntries {	// 常量为5000, 最大吞吐量为1W，但5K足够 slow follower 去追赶 leader.
		  								compacti = snapi - s.Cfg.SnapshotCatchUpEntries
		  							}

		  					L2092 	s.r.raftStorage.Compact(compacti)	// compact MemoryStorage{} 内存缓存.


	  					}()

	  				}

	  				L1098 	ep.snapi = ep.appliedi
	  			}

	  		L911 	select {
	  					case m := <-s.r.msgSnapC:	// 等待 leader或者follower 在 raftNode 开始 send msg， 再发送 合并snap请求。
	  												// a chan to send/receive snapshot
	  					/* etcd/server/etcdserver/raft.go	
				      		  ** There are two separate data store: the store for v2, and the KV for v3. **
						      // The msgSnap only contains the most recent snapshot of store without KV.
						      // So we need to redirect the msgSnap to etcd server main loop for merging in the
						      // current store snapshot and KV snapshot.
								processMessages( raftpb.MsgSnap ( select {
	  								case r.msgSnapC <- ms[i]:
	  								default:
	  							})) */
	  					L914	merged := s.createMergedSnapshotMessage(m, ep.appliedt, ep.appliedi, ep.confState)	// 开始发送 merge snap 给follower

	  						--> etcd/server/etcdserver/snapshot_merge.go

	  						(s *EtcdServer) createMergedSnapshotMessage(m raftpb.Message, snapt, snapi uint64, confState raftpb.ConfState) snap.Message {	// v2 store and v3 KV
	  							L34 	clone := s.v2store.Clone()	// get a snapshot of v2 store as []byte
	  									clone.SaveNoCopy()

	  									s.KV().Commit()		// commit kv to write metadata(for example: consistent index)
	  									dbsnap := s.be.Snapshot()	// 构建snap send 对象，其中包含传输超时控制

	  									--> etcd/server/storage/backend/backend.go

	  									(b *backend) Snapshot() Snapshot {
	  										L322	tx, err := b.db.Begin(false)	// 构建bolt tx，并带 goroutine 计时器，发送到 follower pipeline 执行，nb
	  										L327 	stopc, donec := make(chan struct{}), make(chan struct{})
	  										L328 	dbBytes := tx.Size()

	  										go func() {
	  											ticker := time.NewTicker(timeout) // timeout 按照 100M/s 速率计算的 dbBytes包传输超时时间
	  											defer ticker.Stop()
	  											for {
	  												select {
	  													case <-ticker.C:
	  														warning("snapshotting taking too long to transfer")
	  													case <-stopc:
	  														return
	  												}
	  											}
	  										}()
	  										return &snapshot{tx, stopc, donec}
	  									}

	  									rc := newSnapshotReaderCloser(lg, dbsnap)	// get a snapshot of v3 KV as readCloser

	  									--> etcd/server/etcdserver/snapshot_merge.go

	  									newSnapshotReaderCloser(lg *zap.Logger, snapshot backend.Snapshot) io.ReadCloser { // 有点意思.
	  										L62 	pr, pw := io.Pipe()		// Pipe() (*PipeReader, *PipeWriter) {} // pr,pw (io.pipe)
	  												go func() {
	  													L64 	n, err := snapshot.WriteTo(pw)	// (tx *Tx) WriteTo() // boltDB
	  													L78 	pw.CloseWithError(err)			// (w *PipeWriter) CloseWithError()
	  													L79 	err = snapshot.Close()	

	  															--> etcd/server/storage/backend/backend.go
	  															(b *backend) Close() {
	  																close(b.stopc)
	  																<-b.donec
	  																return b.db.Close()
	  															}
	  													}()
	  												return pr
	  									}

		  							  	snapshot := raftpb.Snapshot{
										    Metadata: raftpb.SnapshotMetadata{
										      Index:     snapi,
										      Term:      snapt,
										      ConfState: confState,
									    	},  
									    	Data: d,
								  	 	}
							  			m.Snapshot = snapshot

	  									return *snap.NewMessage(m, rc, dbsnap.Size())	// m (v2 store raft.Snapshot); rc (v3 kv as readCloser)
	  						}

	  					L915	s.sendMergedSnap(merged)	// 开始send merged snap 操作

	  						--> etcd/server/etcdserver/server.go
	  						(s *EtcdServer) sendMergedSnap(merged snap.Message) {
	  							L1778 	atomic.AddInt64(&s.inflightSnapshots, 1)	// snapshot 压缩 应该在 发送之后发生，这里添加 一个Snap暗斗.
	  							L1789	s.r.transport.SendSnapshot(merged) 

	  							--> etcd/server/etcdserver/api/rafthttp/snapshot_sender.go //直接跳到
	  							 (s *snapshotSender) send(merged snap.Message) {
	  							 		L75 	body := createSnapBody(s.tr.Logger, merged) 	// encode 包含 v2 store snap Byte[]
	  							 		L79 	req := createPostRequest(s.tr.Logger, u, RaftSnapshotPrefix, body, "application/octet-stream", s.tr.URLs, s.from, s.cid)
	  							 		L98 	s.post(req)		// 不展开 post http 模块
	  							 		L129 	s.status.activate()
	  							 		L130 	s.r.ReportSnapshot(m.To, raft.SnapshotFinish)
	  								}
	  							L1792 	s.GoAttach(func() {
	  									select {
	  										case ok := <-merged.CloseNotify():
	  										case <-s.stopping:
	  									}
	  								})
	  						}
	  				}
	  		}
		}



raftNode对Ready实例中各个字段的处理：
softstate：1、更新raftNode.lead字段。2、根据leader节点的变化情况调用updateLeadership（）回调函数
readStates：readStateC通道
CommittedEntries：封装成apply实例，送入applyc通道
Snapshot：1、封装成apply实例，送入applyc通道。2、将快照数据保存到本地盘。3、保存到MemoryStorage中
Messages：1、目标节点不存在的，踢除。2、如果有多条msgAppresp消息，只保留最后一条。3、如果有msgSnap消息，送入raftNode.msgSnapC中。
Entries：保存到MemoryStorage中

	type node struct {
	  propc      chan msgWithResult
	  recvc      chan pb.Message                                                                                                                                                                                                  
	  confc      chan pb.ConfChangeV2
	  confstatec chan pb.ConfState
	  readyc     chan Ready
	  advancec   chan struct{}
	  tickc      chan struct{}
	  done       chan struct{}
	  stop       chan struct{}
	  status     chan chan Status
	   
	  rn *RawNode
	}


·  raftNode 模块的 coroutine 通过 readyc 读取到 Ready，首先通过网络层将 2 个 append entries 的 messages 发送给两个副本(PS:这里是异步发送的)；

·  raftNode 模块的 coroutine 自己将 Op log entry 通过持久化层的 WAL 接口同步的写入 WAL 文件中 :err := r.storage.Save(rd.HardState, rd.Entries);

·  让raft模块把entry从unstable移动到storage中保存:
		r.raftStorage.Append(rd.Entries)

·  raftNode 模块的 coroutine 通过 advancec Channel 通知当前 Ready 已经处理完，请给我准备下一个 带出的 raft StateMachine 输出Ready；

·  其他副本的返回 Append entries 的 response： MsgAppResp message，会通过 node 模块的接口经过 recvc Channel（case m := <-n.recvc:） 提交给 node 模块的 coroutine；

·  node 模块 coroutine 从 recvc Channel 读取到 MsgAppResp，然后提交给 raft StateMachine 处理。node 模块 coroutine 会驱动 raft StateMachine 得到关于这个 committedEntires，也就是一旦大多数副本返回了就可以 commit 了，node 模块 new 一个新的 Ready其包含了 committedEntries，通过 readyc Channel 传递给 raftNode 模块 coroutine 处理；  ????   

·  raftNode 模块 coroutine 从 readyc Channel 中读取 Ready结构

·  取出已经 commit 的 committedEntries 通过 applyc 传递给另外一个 etcd server coroutine 处理


case ap := <-s.r.apply():

     f := func(context.Context) { s.applyAll(&ep, &ap) }

     sched.Schedule(f)

其会将每个apply 任务提交给 FIFOScheduler 调度异步处理，这个调度器可以保证 apply 任务按照顺序被执行，因为 apply 的执行是不能乱的；

·  raftNode 模块的 coroutine 通过 advancec Channel 通知当前 Ready 已经处理完，请给我准备下一个待处理的 raft StateMachine 输出Ready；

·  FIFOScheduler 调度执行 apply 已经提交的 committedEntries

·  AppliedIndex 推进，通知 ReadLoop coroutine，满足 applied index>= commit index 的 read request 可以返回；

·  server调用网络层接口返回 client 成功。

总之最终进入：applyEntryNormal（）函数：


