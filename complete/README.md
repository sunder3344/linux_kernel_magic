# linux completion（linux 6.10）

complete 是 Linux 内核中用于任务同步的一种机制，通常用于一个任务等待另一个任务完成某个操作。它的核心思想是通过 wait_for_completion 和 complete 实现任务之间的同步。

- 一个任务调用 wait_for_completion 等待某个事件完成。

- 另一个任务调用 complete 或 complete_all 通知等待的任务事件已完成。

## 执行方法
1. make编译，加载模块insmod complete_demo.ko

2. dmesg -w监控输出

4. 卸载模块rmmod complete_demo


## complete特点

- complete 是 Linux 内核中用于任务同步的一种机制，适合一个任务等待另一个任务完成某个操作。

- 使用 init_completion 初始化 completion，使用 wait_for_completion 等待事件完成，使用 complete 通知事件完成。

- complete 的变体（如 wait_for_completion_timeout、try_wait_for_completion、complete_all）适用于不同的场景。



## complete优缺点

### complete 的优点

- 简单易用：complete 的接口非常简单，只需要初始化 completion 结构体，然后调用 wait_for_completion 和 complete 即可。适合快速实现任务同步。

- 可中断的等待：wait_for_completion 的变体 wait_for_completion_interruptible 支持可中断的等待，允许任务在等待时被信号中断。适合需要处理用户空间信号的场景。

- 支持多个等待任务：complete_all 可以通知所有等待的任务，适合一对多的同步场景。

- 适用于进程上下文：complete 只能在进程上下文中使用，适合需要睡眠的任务。


### complete 的缺点

- 不适用于中断上下文：complete 不能在中断上下文中使用，因为 wait_for_completion 可能会导致任务睡眠。如果需要在中斷上下文中同步，可以使用自旋锁或其他机制。

- 性能开销：complete 的实现依赖于内核的等待队列机制，涉及任务调度和上下文切换，因此性能开销较高。不适合对性能要求极高的场景。

- 不支持超时：标准的 wait_for_completion 不支持超时机制，任务可能会一直等待。如果需要超时机制，可以使用 wait_for_completion_timeout。


## complete 的性能

complete 的性能主要受以下因素影响：

- 任务调度开销：wait_for_completion 会将任务加入等待队列，并触发任务调度。任务调度和上下文切换会引入一定的性能开销。

- 锁竞争：complete 的实现依赖于内核的等待队列机制，可能会涉及锁竞争。如果多个任务同时等待同一个 completion，锁竞争可能会影响性能。

- 适用场景：complete 适合低频、短时间的同步操作。如果需要高频同步或对性能要求极高，可以考虑使用自旋锁或其他轻量级同步机制。


## complete 的适用场景

- 任务同步：一个任务需要等待另一个任务完成某个操作。例如，内核线程完成初始化后通知主线程。

- 资源初始化：等待某个资源（如设备、缓冲区）初始化完成。例如，设备驱动等待硬件初始化完成。

- 事件通知：通知某个事件的发生。例如，网络协议栈通知数据包已到达。