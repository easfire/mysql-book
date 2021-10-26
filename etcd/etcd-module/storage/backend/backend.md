classDiagram
class backend {
  db *bolt.DB
  +batchTx       *batchTxBuffered
  +readTx *readTx
  +txReadBufferCache txReadBufferCache
  stopc chan struct
  donec chan struct
  hooks Hooks
}
backend --|>batchTxBuffered
class  batchTxBuffered{
    batchTx
    buf txWriteBuffer
}
batchTxBuffered --|> batchTx
class batchTx {
    tx      *bolt.Tx
    backend *backend
}
batchTx --o backend
batchTxBuffered --|> txWriteBuffer
class txWriteBuffer{
    txBuffer
    bucket2seq *map[BucketID]bool
}
txWriteBuffer --|> txBuffer
class txBuffer {
    buckets *map[BucketID]*bucketBuffer
}
txBuffer --|>bucketBuffer
class bucketBuffer {
    buf []kv
    used int
}
bucketBuffer --|> kv
class kv {
    key []byte
    value []byte
}
backend --|> readTx

readTx --|> baseReadTx
class baseReadTx{
    mu  sync.RWMutex
    buf txReadBuffer
  txMu    *sync.RWMutex
  tx      *bolt.Tx
  buckets *map[BucketID]*bolt.Bucket
  txWg *sync.WaitGroup
}
baseReadTx --|> txReadBuffer
class txReadBuffer{
    txBuffer
    bufVersion uint64
}
txReadBuffer --|> txBuffer

readTx --> ReadTx
concurrentReadTx --> ReadTx
class readTx{
    baseReadTx
    ReadTx()
}
class concurrentReadTx{
    baseReadTx  
    ReadTx()
}
backend --|> txReadBufferCache
class txReadBufferCache {
    mu         sync.Mutex
    buf        *txReadBuffer
    bufVersion uint64
}
txReadBufferCache --|> txReadBuffer
class ReadTx {                                                                                                                                                                                                       
  Lock()
  Unlock()
  RLock()
  RUnlock()
  UnsafeRange(bucket Bucket, key, endKey []byte, limit int64) (keys [][]byte, vals [][]byte)
  UnsafeForEach(bucket Bucket, visitor func(k, v []byte) error) error
}
