
### 软链和硬链
参考：

https://xzchsia.github.io/2020/03/05/linux-hard-soft-link/

### linux 内核参数tcp_tw_reuse、tcp_tw_recycle、tcp_timestamp详解
https://zeven0707.github.io/2019/09/22/linux%20%E5%86%85%E6%A0%B8%E5%8F%82%E6%95%B0tcp_tw_reuse%E3%80%81tcp_tw_recycle%E3%80%81tcp_timestam/

### ARP协议
https://www.cnblogs.com/cxuanBlog/p/14265315.html

### NFS
https://zhuanlan.zhihu.com/p/31626338

### Linux Cgroup
https://cizixs.com/2017/08/25/linux-cgroup/

### TCP TIME_WAIT
https://draveness.me/whys-the-design-tcp-time-wait/

TIME_WAIT状态存在的理由：
1）可靠地实现TCP全双工连接的终止
	在进行关闭连接四次挥手协议时，最后的ACK是由主动关闭端发出的，如果这个最终的ACK丢失，服务器将重发最终的FIN，
	因此客户端必须维护状态信息允许它重发最终的ACK。如果不维持这个状态信息，那么客户端将响应RST分节，服务器将此分节解释成一个错误
	（在java中会抛出connection reset的SocketException)。
	因而，要实现TCP全双工连接的正常终止，必须处理终止序列四个分节中任何一个分节的丢失情况，主动关闭的客户端必须维持状态信息进入TIME_WAIT状态。
 
2）允许老的重复分节在网络中消逝 
	TCP分节可能由于路由器异常而“迷途”，在迷途期间，TCP发送端可能因确认超时而重发这个分节，迷途的分节在路由器修复后也会被送到最终目的地，
	这个原来的迷途分节就称为lost duplicate。
	在关闭一个TCP连接后，马上又重新建立起一个相同的IP地址和端口之间的TCP连接，后一个连接被称为前一个连接的化身（incarnation)，
	那么有可能出现这种情况，前一个连接的迷途重复分组在前一个连接终止后出现，从而被误解成从属于新的化身。
	为了避免这个情况，TCP不允许处于TIME_WAIT状态的连接启动一个新的化身，因为TIME_WAIT状态持续2MSL，就可以保证当成功建立一个TCP连接的时候，
	来自连接先前化身的重复分组已经在网络中消逝。



存在即是合理的，既然TCP协议能盛行四十多年，就证明他的设计合理性。所以我们尽可能的使用其原本功能。
依靠TIME_WAIT 状态来保证我的服务器程序健壮，服务功能正常。
那是不是就不要性能了呢？并不是。如果服务器上跑的短连接业务量到了我真的必须处理这个TIMEWAIT状态过多的问题的时候，
我的原则是尽量处理，而不是跟TIMEWAIT干上，非先除之而后快。
如果尽量处理了，还是解决不了问题，仍然拒绝服务部分请求，那我会采取负载均衡来抗这些高并发的短请求。持续十万并发的短连接请求，
两台机器，每台5万个，应该够用了吧。一般的业务量以及国内大部分网站其实并不需要关注这个问题，一句话，达不到时才需要关注这个问题的访问量。



TCP协议发表：1974年12月，卡恩、瑟夫的第一份TCP协议详细说明正式发表。当时美国国防部与三个科学家小组签定了完成TCP/IP的协议，
结果由瑟夫领衔的小组捷足先登，首先制定出了通过详细定义的TCP/IP协议标准。当时作了一个试验，将信息包通过点对点的卫星网络，再通过陆地电缆
，再通过卫星网络，再由地面传输，贯串欧洲和美国，经过各种电脑系统，全程9.4万公里竟然没有丢失一个数据位，远距离的可靠数据传输证明了TCP/IP协议的成功。



netstat -ant|awk '/^tcp/ {++S[$NF]} END {for(a in S) print (a,S[a])}'

CLOSED：无连接是活动的或正在进行
LISTEN：服务器在等待进入呼叫
SYN_RECV：一个连接请求已经到达，等待确认
SYN_SENT：应用已经开始，打开一个连接
ESTABLISHED：正常数据传输状态
FIN_WAIT1：应用说它已经完成
FIN_WAIT2：另一边已同意释放
ITMED_WAIT：等待所有分组死掉
CLOSING：两边同时尝试关闭
TIME_WAIT：另一边已初始化一个释放
LAST_ACK：等待所有分组死掉


如何尽量处理TIMEWAIT过多?

net.ipv4.tcp_syncookies = 1 表示开启SYN Cookies。当出现SYN等待队列溢出时，启用cookies来处理，可防范少量SYN攻击，默认为0，表示关闭；
net.ipv4.tcp_tw_reuse = 1 表示开启重用。允许将TIME-WAIT sockets重新用于新的TCP连接，默认为0，表示关闭；
net.ipv4.tcp_tw_recycle = 1 表示开启TCP连接中TIME-WAIT sockets的快速回收，默认为0，表示关闭。
net.ipv4.tcp_fin_timeout 修改系默认的 TIMEOUT 时间

简单来说，就是打开系统的TIMEWAIT重用和快速回收。



vi /etc/sysctl.conf

net.ipv4.tcp_keepalive_time = 1200 
#表示当keepalive起用的时候，TCP发送keepalive消息的频度。缺省是2小时，改为20分钟。
net.ipv4.ip_local_port_range = 1024 65000 
#表示用于向外连接的端口范围。缺省情况下很小：32768到61000，改为1024到65000。
net.ipv4.tcp_max_syn_backlog = 8192 
#表示SYN队列的长度，默认为1024，加大队列长度为8192，可以容纳更多等待连接的网络连接数。
net.ipv4.tcp_max_tw_buckets = 5000 
#表示系统同时保持TIME_WAIT套接字的最大数量，如果超过这个数字，TIME_WAIT套接字将立刻被清除并打印警告信息。
默认为180000，改为5000。对于Apache、Nginx等服务器，上几行的参数可以很好地减少TIME_WAIT套接字数量，
但是对于 Squid，效果却不大。此项参数可以控制TIME_WAIT套接字的最大数量，避免Squid服务器被大量的TIME_WAIT套接字拖死。



/etc/sysctl.conf是一个允许改变正在运行中的Linux系统的接口，它包含一些TCP/IP堆栈和虚拟内存系统的高级选项，修改内核参数永久生效。


### LVM管理
https://www.dwhd.org/20150521_225146.html
https://www.cnblogs.com/xibuhaohao/p/11731699.html

Striped Logic Volume
Striped LV的底层存储布局类似于RAID0，它是跨多个PV的，具体是跨多少个PV用-i指定，但是肯定不能超过VG中PV的数量，Striped LV的最大size取决于剩余PE最少的那个PV。

Striping的意思是将每个参与Striping的PV划分成等大小的chunk（也叫做stripe unit），每个PV同一位置的这些chunk共同组成一个stripe。比如下面这张图（来自于RedHat6官方文档），包含三个PV，那么红色标识的1、2、3这3个chunk就组成了stripe1，4、5、6组成stripe2。chunk的大小可以通过-I或者--stripesize来指定，但是不能超过PE的大小。

比如，向Striped LV写入数据时，数据被分成等大小的chunk，然后将这些chunk顺序写入这些PV中。这样的话就会有多个底层disk drive并发处理I/O请求，可以得到成倍的聚合I/O性能。还是下面这张图，假如现在有一个4M数据块需要写入LV，stripesize设置的512K，LVM把它切成8个chunk，分别标识为chunk1、chunk2...，这些chunk写入PV的顺序如下：

chunk1写入PV1
chunk2写入PV2
chunk3写入PV3
chunk4写入PV1
...


因为LVM无法判断多个Physics Volume是否来自同一个底层disk，如果Striped LV使用的多个Physics Volume实际上是同一个物理磁盘上的不同分区，就会导致一个数据块被切成多个chunk分多次发给同一个disk drive，这种情况实际上Striped LV并不能提升性能，反而会使性能下降。所以说，Striped LV提升I/O性能的本质是让多个底层disk drive并行处理I/O请求，而不是表面上的把I/O分散到了多个PV上。

Striped LV主要满足性能需求，没有做任何冗余，所以没有容错能力，如果单个disk损坏，就会导致数据损坏。

root@hunk-virtual-machine:/home# lvcreate -L 20G --stripes 4 --stripesize 256 --name stripevol VolGroup1
 
 
### Linux共享对象之编译参数fPIC
https://www.cnblogs.com/cswuyg/p/3830703.html

    PIC的共享对象也会有重定位表，数据段中的GOT、数据的绝对地址引用，这些都是需要重定位的。
    readelf -r libtest.so 
    可以看到共享对象的重定位表，.rel.dyn是对数据引用的修正，.rel.plt是对函数引用的修正。


	Relocation section '.rela.dyn' at offset 0x450 contains 8 entries:
	  Offset          Info           Type           Sym. Value    Sym. Name + Addend
	000000200df8  000000000008 R_X86_64_RELATIVE                    670
	000000200e00  000000000008 R_X86_64_RELATIVE                    630
	000000200e10  000000000008 R_X86_64_RELATIVE                    200e10
	000000200fd8  000100000006 R_X86_64_GLOB_DAT 0000000000000000 _ITM_deregisterTMClone + 0
	000000200fe0  000300000006 R_X86_64_GLOB_DAT 0000000000000000 __gmon_start__ + 0
	000000200fe8  000400000006 R_X86_64_GLOB_DAT 0000000000000000 _Jv_RegisterClasses + 0
	000000200ff0  000500000006 R_X86_64_GLOB_DAT 0000000000000000 _ITM_registerTMCloneTa + 0
	000000200ff8  000600000006 R_X86_64_GLOB_DAT 0000000000000000 __cxa_finalize@GLIBC_2.2.5 + 0

	Relocation section '.rela.plt' at offset 0x510 contains 3 entries:
	  Offset          Info           Type           Sym. Value    Sym. Name + Addend
	000000201018  000200000007 R_X86_64_JUMP_SLO 0000000000000000 puts@GLIBC_2.2.5 + 0
	000000201020  000300000007 R_X86_64_JUMP_SLO 0000000000000000 __gmon_start__ + 0
	000000201028  000600000007 R_X86_64_JUMP_SLO 0000000000000000 __cxa_finalize@GLIBC_2.2.5 + 0




