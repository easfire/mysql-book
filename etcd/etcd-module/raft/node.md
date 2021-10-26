classDiagram
    Node --|> ConsistentIndexer
    class Node {
        Tick()
        Campaign(ctx) error
        Propose(ctx, data []byte) error
        ProposeConfChange(ctx, cc pb.ConfChangeI) error
        Step(ctx, msg pb.Message) error
        Ready() chan Ready
        Advance()
        ApplyConfChange(pbConfChangeI) pbConfState
        TransferLeadership(ctx, lead, transferee uint64)
        ReadIndex(ctx, rctx []byte) error
        Status() Status
        ReportUnreachable(id uint64)
        ReportSnapshot(id uint64, status SnapshotStatus)
        Stop()
        propc      chan_msgWithResult
        recvc      chan_pbMessage
        confc      chan_pbConfChangeV2
        confstatec chan_pbConfState
        readyc     chan_Ready
        advancec   chan_struct
        tickc      chan_struct
        done       chan_struct
        stop       chan_struct
        status     chan_chan_Status
        rn *RawNode
        +ConsistentIndexer interface()
    }
    class ConsistentIndexer {
        +ConsistentIndex() uint64
        +SetConsistentIndex(v uint64, term uint64)
        +UnsafeSave(tx backend.BatchTx)
        +SetBackend(be Backend)
    }

    class store {
        ReadView
        WriteView
        cfg StoreConfig
 
        mu sync.RWMutex
    
        b       backend.Backend
        kvindex index
        
        le lease.Lessor
        
        revMu sync.RWMutex
        currentRev int64
        compactMainRev int64
        
        fifoSched schedule.Scheduler
        
        stopc chan struct
        lg *zap.Logger
    }
