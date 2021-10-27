#runtime/pprof
#net/http/pprof
#runtime/trace
https://zhuanlan.zhihu.com/p/141640004

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
