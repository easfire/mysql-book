classDiagram
    Transport --|> Raft
    Transport --|> ServerStats
    Transport --|> LeaderStats
    Transport --|> remote
    Transport --|> Peer
    Transport --|> pipelineProber
    Transport --|> streamProber
    ServerStats --|> statsQueue
    statsQueue --|> StatsQueue
    statsQueue --|> RequestStats
    LeaderStats --|> FollowerStats
    remote --|> pipeline
    pipeline --|> urlPicker
    pipeline --|> peerStatus
    class Transport {
        +Raft (interface)Raft
        ServerStats *stats.ServerStats
        LeaderStats *stats.LeaderStats
        remotes *map[*types.ID]remote
        +peers *map[*types.ID]Peer(interface)
    }
    class Raft {
        +Process() error
        +IsIDRemoved() bool
        +ReportUnreachable()
        +ReportSnapshot()
    }
    class ServerStats {
        sendRateQueue *statsQueue
        recvRateQueue *statsQueue
    }
    class statsQueue {
        items [queueCapacity]*RequestStats
        size int
        front int
        back int
        totalReqSize int
        rwl *sync.RWMutex
        Itf (interface)StatsQueue
    }
    class StatsQueue {
        Len()
        ReqSize()
        frontAndBack()
        Insert()
        Rate()
        Clear()
    }
    class RequestStats {
        SendingTime *time.Time
        Size int
    }
    class LeaderStats {
        Leader string
        Followers [string]*FollowerStats
    }
    class FollowerStats {
        Latency LatencyStats
        Counts CountsStats
    }
    class remote {
        localID ID
        id ID
        status *peerStatus
        pipeline *pipeline
    }
    class pipeline {
        peerID ID
        tr *Transport
        picker *urlPicker
        status *peerStatus
        raft Raft
        followerStats *stats.FollowerStats
        msgc *raftpb.Message 
    }
    class urlPicker {
        mu *sync.Mutex
        urls *types.URLs
        picked int
    }
    class peerStatus {
        local *types.ID
        id *types.ID
        active bool
        since *time.Time
    }
    class Peer {
        send(m raftpb.Message)
        sendSnap(m snap.Message)
        update(urls types.URLs)
        attachOutgoingConn(conn *outgoingConn)
        activeSince() time.Time
        stop()
    }

            
