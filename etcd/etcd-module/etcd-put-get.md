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

	func (r *raftNode) start(rh *raftReadyHandler) { // etcdserver处理 Ready对象 L157
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

		   	    updateCommittedIndex(&ap, rh)                                                                                                                                                                                         
		        select {
		        case r.applyc <- ap: 
		        case <-r.stopped:
		          return
		        }
		        // the leader can write to its disk in parallel with replicating to the followers and them writing to their disks.
		        if islead {
          			r.transport.Send(r.processMessages(rd.Messages))
        		}

        		// Must save the snapshot file and WAL snapshot entry before saving any other entries or hardstate to ensure that recovery after a snapshot restore is possible. 
        		r.storage.SaveSnap(rd.Snapshot)
        		-(st *storage) SaveSnap(snap raftpb.Snapshot) { //SaveSnap saves the snapshot file to disk and writes the WAL snapshot entry.
        			// 先写.snap文件，再写.wal
        			st.Snapshotter.SaveSnap(snap)
        				-- (s *Snapshotter) save() { (etcd/server/etcdserver/api/snap/snapshotter.go)
        					err = pioutil.WriteAndSyncFile(spath, d, 0666)
        				}
        				--- WriteAndSyncFile{
        					os.OpenFile(filename, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, perm)
        					f.Write(data)
        					fileutil.Fsync(f)
        					f.Close()
        				}
        			st.WAL.SaveSnapshot(walsnap)
        				-- (w *WAL) SaveSnapshot(e walpb.Snapshot) { (etcd/server/storage/wal/wal.go)
        					w.encoder.encode(rec)
        					--- (e *encoder) encode(rec *walpb.Record) { (etcd/server/storage/wal/encoder.go)
        						e.bw.Write(data)
        					}
        					---- (pw *PageWriter) Write(p []byte) { (etcd/pkg/ioutil/pagewriter.go)
        					}
        					w.sync()
        				}
        		}

        		r.storage.Save(rd.HardState, rd.Entries)
        		- (w *WAL) Save(st raftpb.HardState, ents []raftpb.Entry) {
        			for i := range ents {
        				w.saveEntry(&ents[i]) // 存储 entries
        			}
        			w.saveState(&st)	// 存储 HardState
        			if mustSync {  // entsnum != 0 || st.Vote != prevst.Vote || st.Term != prevst.Term
        				w.sync()
        			}
        			w.cut()  // cut closes current file written and creates a new one ready to append
        		}

        		r.storage.Sync()
        		// etcdserver now claim the snapshot has been persisted onto the disk
        		notifyc <- struct{}{}

        		// ApplySnapshot overwrites the contents of this Storage object with those of the given snapshot.
				r.raftStorage.ApplySnapshot(rd.Snapshot)

				// Release releases resources older than the given snap and are no longer needed:
				// - releases the locks to the wal files that are older than the provided wal for the given snap.
				// - deletes any .snap.db files that are older than the given snap
				r.storage.Release(rd.Snapshot)


				r.raftStorage.Append(rd.Entries) //Append the new entries to storage.

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
					s.applySnapshot(ep, apply)
					s.applyEntries(ep, apply)
				}

		L920	(s *EtcdServer) applySnapshot(ep *etcdProgress, apply *apply) {
				// wait for raftNode to persist snapshot onto the disk
				<-apply.notifyc
				newbe, err := serverstorage.OpenSnapshotBackend(s.Cfg, s.snapshotter, apply.snapshot, s.beHooks)    // 创建Etcd Db (boltdb)

				}

				if s.lessor != nil { 	// 考虑lessor情况 always recover lessor before kv
					s.lessor.Recover(newbe, func() lease.TxnDelete { return s.kv.Write(traceutil.TODO()) })
				}

		L975	s.kv.Restore(newbe)		// 处理 构建 Tree Index (内容比较多)

		etcd/server/storage/mvcc/kvstore.go
			L299	(s *store) Restore(b backend.Backend) {
						s.kvindex = newTreeIndex(s.lg)
			L320		s.restore()
					}
			L323	(s *store) restore() {
						// 开始生成 revision 信息了。
						min, max Revision
						finishedCompact
						scheduledCompact

				L351	rkvc, revc := restoreIntoIndex(s.lg, s.kvindex)		??
						L427	restoreIntoIndex(lg *zap.Logger, idx index) (chan<- revKeyValue, <-chan int64) {
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

				s.cluster.SetBackend(schema.NewMembershipBackend(lg, newbe))	// UnsafeCreateBucket(Members/MembersRemoved/Cluster)







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


