
### 先粗略整理下选举过程: 

		etcd3 [470f778210a711ed] 新晋 leader...
			19:03:55.719 470f778210a711ed [term 5] received MsgTimeoutNow from 72ab37cc61e2023b and starts an election to get leadership
						 470f778210a711ed is starting a new election at term 5
						 became candidate at term 6" >>> etcd3的第6轮拉票已经开始。

						 同时旧主失去leader角色;
						 "msg":"raft.node: 470f778210a711ed lost leader 72ab37cc61e2023b at term 6"

						 向其余三个member发送MsgVote请求。但其余member也被pkill。无法参与。

						 集群再次启动后，etcd3的term 轮次是6。最高，所以他会成为候选，先发优势。

			19:04:07.912 became follower at term 6

			19:04:08.512 is starting a new election at term 6
			became pre-candidate at term 6
			received MsgPreVoteResp from 470f778210a711ed at term 6

			470f778210a711ed received MsgPreVoteResp from 72ab37cc61e2023b at term 6
			470f778210a711ed has received 2 MsgPreVoteResp votes and 0 vote rejections
			470f778210a711ed received MsgPreVoteResp from 47a42fb96a975854 at term 6
			470f778210a711ed has received 3 MsgPreVoteResp votes and 0 vote rejections

			[三票通过]


			19:04:08.514 became candidate at term 7

			[三票通过]

			19:04:08.519 became leader at term 7

			[当选]


		etcd2 [72ab37cc61e2023b] 前leader败选...
			19:03:55.719 "msg":"leadership transfer starting","local-member-id":"72ab37cc61e2023b","current-leader-member-id":"72ab37cc61e2023b","transferee-member-id":"470f778210a711ed"
						 [term 5] starts to transfer leadership to 470f778210a711ed"

			为什么选了 470f778210a711ed ???
				前leader推选候选人的标准，建立peer链接并保持active时间最长的member. 
				--> 18:58:03.208 "msg":"peer became active","peer-id":"470f778210a711ed" 第一个检查peer成功。

			19:03:55.738 [term: 5] received a MsgVote message with higher term from 470f778210a711ed [term: 6] 
				etcd3 接受到transfer leadership，已经开始拉票，etcd2接收到MsgVote，发现轮次低于6，变成follower。
				--> became follower at term 6

			还是会继续拉票 (成为 pre-candicate) 直到 19:04:02.742 [shutdown]
				  // PreVote enables the Pre-Vote algorithm described in raft thesis section
				  // 9.6. This prevents disruption when a node that has been partitioned away
				  // rejoins the cluster.
				  PreVote bool
			19:04:02.720 "msg":"leadership transfer failed","local-member-id":"72ab37cc61e2023b","error":"etcdserver: request timed out, leader transfer took too long"
			停止掉与每个member的peer
			19:04:02.742 closed etcd server


			19:04:07.910 became follower at term 6"
			19:04:07.926 "msg":"starting initial election tick advance","election-ticks":10

			19:04:08.518 "msg":"72ab37cc61e2023b [term: 6] received a MsgVote message with higher term from 470f778210a711ed [term: 7]"
						 became follower at term 7


		etcd1 [47a42fb96a975854] follower 败选理由....
			19:03:55.719 "msg":"received signal; shutting down","signal":"terminated" (pkill etcd)
			19:03:55.719 skipped leadership transfer; local server is not leader
			19:04:07.868 start 
			19:04:07.910 became follower at term 5
			19:04:07.924 "msg":"starting initial election tick advance","election-ticks":10
			19:04:08.410 became pre-candidate at term 5

			19:04:08.411 [term: 5] received a MsgPreVoteResp message with higher term from 470f778210a711ed [term: 6]
			19:04:08.411 became follower at term 6  >>> 470f778210a711ed 收到.

			19:04:08.518 [term: 6] received a MsgVote message with higher term from 470f778210a711ed [term: 7]
			19:04:08.518 became follower at term 7


		etcd4 [ee287c4d42f62ecf] follower 败选理由....
			19:04:07.909 became follower at term 5

			19:04:08.110 became pre-candidate at term 5

			19:04:08.112 "msg":"ee287c4d42f62ecf [term: 5] received a MsgPreVoteResp message with higher term from 72ab37cc61e2023b [term: 6]"
						 became follower at term 6  >>> 72ab37cc61e2023b 收到.

			19:04:08.518 "msg":"ee287c4d42f62ecf [term: 6] received a MsgVote message with higher term from 470f778210a711ed [term: 7]"
						 became follower at term 7


##### 竞选规则
	TODO

###### 总结下来

			term 5 					term 6      										term 7
	etcd2   leader(推选etcd3)   		follower(重启) 													follower(被etcd3 第7轮的拉票降为follower)
	etcd3   follower 			candidate(被推选,发起过拉票)->follower(重启)->pre-candidate							candidiate->leader
	etcd1 	follower(重启)->pre-candidate(etcd4给投过票)  follower(被etcd3 第6轮的拉票降为follower)      				follower(被etcd3 第7轮的拉票降为follower)
	etcd4   follower(重启)->pre-candidate(etcd1给投过票)  follower(被etcd2 第6轮的拉票降为follower)      				follower(被etcd3 第7轮的拉票降为follower)

### code review
	TODO

