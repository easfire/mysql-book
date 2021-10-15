先看Iran神图

![mem](https://img-blog.csdnimg.cn/8b3efef2db8e4fa48edeefd9ee8e0169.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBASm9nZ2VyX0xpbmc=,size_20,color_FFFFFF,t_70,g_se,x_16)

***虚拟内存***

当Cache没有命中的时候，访问虚拟内存获取数据的过程。在访问内存，实际访问的是虚拟内存，虚拟内存通过页表查看，当前要访问的虚拟内存地址，是否已经加载到了物理内存。如果已经在物理内存，则取物理内存数据，如果没有对应的物理内存，则从磁盘加载数据到物理内存，并把物理内存地址和虚拟内存地址更新到页表。

物理内存就是磁盘存储缓存层，在没有虚拟内存的时代，物理内存对所有进程是共享的，多进程同时访问同一个物理内存会存在并发问题。而引入虚拟内存后，每个进程都有各自的虚拟内存，内存的并发访问问题的粒度从多进程级别，可以降低到多线程级别。

编程语言的内存分配器一般包含两种分配方法，一种是线性分配器（Sequential Allocator，Bump Allocator），另一种是空闲链表分配器（Free-List Allocator），这两种分配方法有着不同的实现机制和特性，本节会依次介绍它们的分配过程。

空闲链表分配器

- 首次适应（First-Fit）— 从链表头开始遍历，选择第一个大小大于申请内存的内存块；
- 循环首次适应（Next-Fit）— 从上次遍历的结束位置开始遍历，选择第一个大小大于申请内存的内存块；
- 最优适应（Best-Fit）— 从链表头遍历整个链表，选择最合适的内存块；
- 隔离适应（Segregated-Fit）— 将内存分割成多个链表，每个链表中的内存块大小相同，申请内存时先找到满足条件的链表，再从链表中选择合适的内存块；

***go 采用和隔离适应 类似的算法***

因为程序中的绝大多数对象的大小都在 32KB 以下，而申请的内存大小影响 Go 语言运行时分配内存的过程和开销，***所以分别处理大对象和小对象有利于提高内存分配器的性能。***

**多级缓存内存分配**

线程缓存属于每一个独立的线程，它能够满足线程上绝大多数的内存分配需求，因为不涉及多线程，所以也不需要使用互斥锁来保护内存，这能够减少锁竞争带来的性能损耗。当线程缓存不能满足需求时，运行时会使用中心缓存作为补充解决小对象的内存分配，在遇到 32KB 以上的对象时，内存分配器会选择页堆直接分配大内存。

这种多层级的内存分配设计与计算机操作系统中的多级缓存有些类似，因为多数的对象都是小对象，我们可以通过线程缓存和中心缓存提供足够的内存空间，发现资源不足时从上一级组件中获取更多的内存资源。

**虚拟内存布局**

***线性内存***

**堆区的线性内存**

- `spans` 区域存储了指向内存管理单元 [runtime.mspan](https://draveness.me/golang/tree/runtime.mspan) 的指针，每个内存单元会管理几页的内存空间，每页大小为 8KB；
- `bitmap` 用于标识 `arena` 区域中的那些地址保存了对象，位图中的每个字节都会表示堆区中的 32 字节是否包含空闲；
- `arena` 区域是真正的堆区，运行时会将 8KB 看做一页，这些内存页中存储了所有在堆上初始化的对象；

***稀疏内存***

在 amd64 的 Linux 操作系统上，runtime.mheap 会持有 4,194,304 runtime.heapArena，每个 runtime.heapArena 都会管理 64MB 的内存，单个 Go 语言程序的内存上限也就是 256TB。

### 地址空间

因为所有的内存最终都是要从操作系统中申请的，所以 Go 语言的运行时构建了操作系统的内存管理抽象层

### 线程缓存 [#](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/#%e7%ba%bf%e7%a8%8b%e7%bc%93%e5%ad%98)

[runtime.mcache](https://draveness.me/golang/tree/runtime.mcache) 是 Go 语言中的线程缓存，它会与线程上的处理器一一绑定，主要用来缓存用户程序申请的微小对象。每一个线程缓存都持有 68 * 2 个 [runtime.mspan](https://draveness.me/golang/tree/runtime.mspan)，这些内存管理单元都存储在结构体的 `alloc` 字段中：

### 中心缓存 [#](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/#%e4%b8%ad%e5%bf%83%e7%bc%93%e5%ad%98)

[runtime.mcentral](https://draveness.me/golang/tree/runtime.mcentral) 是内存分配器的中心缓存，与线程缓存不同，访问中心缓存中的内存管理单元需要使用互斥锁：

### 7.1.2 内存管理单元 [#](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/#%e5%86%85%e5%ad%98%e7%ae%a1%e7%90%86%e5%8d%95%e5%85%83-1)

线程缓存会通过中心缓存的 [runtime.mcentral.cacheSpan](https://draveness.me/golang/tree/runtime.mcentral.cacheSpan) 方法获取新的内存管理单元，该方法的实现比较复杂，我们可以将其分成以下几个部分：

1. 调用 [runtime.mcentral.partialSwept](https://draveness.me/golang/tree/runtime.mcentral.partialSwept) 从清理过的、包含空闲空间的 [runtime.spanSet](https://draveness.me/golang/tree/runtime.spanSet) 结构中查找可以使用的内存管理单元；
2. 调用 [runtime.mcentral.partialUnswept](https://draveness.me/golang/tree/runtime.mcentral.partialUnswept) 从未被清理过的、有空闲对象的 [runtime.spanSet](https://draveness.me/golang/tree/runtime.spanSet) 结构中查找可以使用的内存管理单元；
3. 调用 [runtime.mcentral.fullUnswept](https://draveness.me/golang/tree/runtime.mcentral.fullUnswept) 获取未被清理的、不包含空闲空间的 [runtime.spanSet](https://draveness.me/golang/tree/runtime.spanSet) 中获取内存管理单元并通过 [runtime.mspan.sweep](https://draveness.me/golang/tree/runtime.mspan.sweep) 清理它的内存空间；
4. 调用 [runtime.mcentral.grow](https://draveness.me/golang/tree/runtime.mcentral.grow) 从堆中申请新的内存管理单元；
5. 更新内存管理单元的 `allocCache` 等字段帮助快速分配内存；

MARK!

本节将介绍页堆的初始化、内存分配以及内存管理单元分配的过程，这些过程能够帮助我们理解全局变量页堆与其他组件的关系以及它管理内存的方式。

### 页堆 [#](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/#%e9%a1%b5%e5%a0%86)

[runtime.mheap](https://draveness.me/golang/tree/runtime.mheap) 是内存分配的核心结构体，Go 语言程序会将其作为全局变量存储，而堆上初始化的所有对象都由该结构体统一管理，该结构体中包含两组非常重要的字段，其中一个是全局的中心缓存列表 `central`，另一个是管理堆区内存区域的 `arenas` 以及相关字段。

### 初始化 [#](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/#%e5%88%9d%e5%a7%8b%e5%8c%96-1)

堆区的初始化会使用 [runtime.mheap.init](https://draveness.me/golang/tree/runtime.mheap.init) 方法，我们能看到该方法初始化了非常多的结构体和字段，不过其中初始化的两类变量比较重要：

1. `spanalloc`、`cachealloc` 以及 `arenaHintAlloc` 等 [runtime.fixalloc](https://draveness.me/golang/tree/runtime.fixalloc) 类型的空闲链表分配器；
2. `central` 切片中 [runtime.mcentral](https://draveness.me/golang/tree/runtime.mcentral) 类型的中心缓存；

堆中初始化的多个空闲链表分配器与设计原理中提到的分配器没有太多区别，当我们调用 [runtime.fixalloc.init](https://draveness.me/golang/tree/runtime.fixalloc.init) 初始化分配器时，需要传入待初始化的结构体大小等信息，这会帮助分配器分割待分配的内存，它提供了以下两个用于分配和释放内存的方法：

1. [runtime.fixalloc.alloc](https://draveness.me/golang/tree/runtime.fixalloc.alloc) — 获取下一个空闲的内存空间；
2. [runtime.fixalloc.free](https://draveness.me/golang/tree/runtime.fixalloc.free) — 释放指针指向的内存空间；

除了这些空闲链表分配器之外，我们还会在该方法中初始化所有的中心缓存，这些中心缓存会维护全局的内存管理单元，**各个线程会通过中心缓存获取新的内存单元**。

## 7.1.3 内存分配 [#](https://draveness.me/golang/docs/part3-runtime/ch07-memory/golang-memory-allocator/#713-%e5%86%85%e5%ad%98%e5%88%86%e9%85%8d)

- 微对象 `(0, 16B)` — 先使用微型分配器，再依次尝试线程缓存、中心缓存和堆分配内存；
- 小对象 `[16B, 32KB]` — 依次尝试使用线程缓存、中心缓存和堆分配内存；
- 大对象 `(32KB, +∞)` — 直接在堆上分配内存；

Go 语言的逃逸分析遵循以下两个不变性：

1. 指向栈对象的指针不能存在于堆中；
2. 指向栈对象的指针不能在栈对象回收后存活；