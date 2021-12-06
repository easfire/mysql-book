hash.md


mysql 底层通过 dynamic_array 和 hash表实现的 时间复杂度O(1)的散列结构。

实现了 
	my_hash_init
	my_hash_free
	my_hash_insert
	my_hash_search
	my_hash_delete
	my_hash_update 等接口。

文档里通过 my_hash_first 结合数据结构分析了hash算法的实现。
同时也对比了Hash散列表结合双向链表实现的LRU cache。

![hash](https://img-blog.csdnimg.cn/f66bfbe5ba6b43dd9d42b8f1158858bd.png)
