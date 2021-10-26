classDiagram
class BatchTx {
  ReadTx
  UnsafeCreateBucket(bucket Bucket)
  UnsafeDeleteBucket(bucket Bucket)
  UnsafePut(bucket Bucket, key []byte, value []byte)
  UnsafeSeqPut(bucket Bucket, key []byte, value []byte)
  UnsafeDelete(bucket Bucket, key []byte)
  #(Commit_commits_a_previous_tx_and_begins_a_new_writable_one)
  Commit()
  #(CommitAndStop_commits_the_previous_tx_and_does_not_create_a_new_one)
  CommitAndStop()
}
BatchTx --|> ReadTx
class ReadTx {                                                                                                                                                                                                       
  Lock()
  Unlock()
  RLock()
  RUnlock()
  UnsafeRange(bucket Bucket, key, endKey []byte, limit int64) (keys [][]byte, vals [][]byte)
  UnsafeForEach(bucket Bucket, visitor func(k, v []byte) error) error
}

readTx --|> ReadTx
concurrentReadTx --|> ReadTx
class readTx{
    baseReadTx
    ReadTx()
}
class concurrentReadTx{
    baseReadTx  
    ReadTx()
}

