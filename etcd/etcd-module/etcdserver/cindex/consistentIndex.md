classDiagram
    consistentIndex --|> ConsistentIndexer
    class consistentIndex {
        consistentIndex uint64
        term uint64
        be Backend
        mutex sync.Mutex
        +ConsistentIndexer interface()
    }
    class ConsistentIndexer {
        +ConsistentIndex() uint64
        +SetConsistentIndex(v uint64, term uint64)
        +UnsafeSave(tx backend.BatchTx)
        +SetBackend(be Backend)
    }
    newBackend --|> newBackend
    class newBackend{
        +newBackend(bcfg BackendConfig) *backend
    }
    class newBackend{
        <1> bolt.Open(bcfg.Path, 0600, bopts)
            #(初始化boltdb)
        <2> newBackend()
            #(初始化backend)
        <3> go *b.run()
            #(启动goroutine)
    }
    class BackendHooks {
        indexer *cindex.ConsistentIndexer
        confState *raftpb.ConfState
        confStateDirty bool 
        confStateLock  *sync.Mutex
    }
    BackendHooks --|> ConsistentIndexer


