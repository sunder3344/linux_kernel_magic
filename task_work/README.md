# linux task_work（linux 6.10）

task_work 是 Linux 内核中的一个机制，用于在 进程上下文（task context） 中延迟执行一些操作。这些操作通常是在某些系统调用结束前，或调度点（如系统调用退出、进程返回用户态之前）执行的。这是一种“延迟执行钩子”（deferred hook）机制。

- task_work 是一个挂在 task_struct（每个进程的结构体）上的一个链表，定义在 include/linux/task_work.h 中。

- 用于在系统调用即将返回用户态前执行某些函数。

- 使用方式类似回调链。

1. make编译，加载模块insmod task_work_demo.ko
2. dmesg -w监控输出
4. 卸载模块rmmod task_work_demo


## task_work应用场景

- io_uring 中，用户发起异步操作后，有些操作在返回用户态前需完成。

- 文件关闭、资源回收等延迟释放。

- 实现一种 “线程自己收尾自己工作” 的机制。

## 相关函数
## 和自旋锁spinlock的区别

| 函数 | 作用 |
| ------ | ------ |
| task_work_add(struct task_struct *task, struct callback_head *twork, enum task_work_notify_mode mode) | 添加一个任务工作 |
| task_work_run() | 在内核中调用，执行该任务的所有回调 |
| task_work_cancel() | 取消已注册的工作（比较少用） |


## 性能

- 在某些场景下比 tasklet 和 workqueue 更加高效

- 需要延迟到系统调用结束或返回用户态前执行的任务

- 只和当前进程有关的回调任务

- 延迟释放资源或“返回用户态前确保某件事完成”的操作（比如 io_uring 的完成事件推送）

## 和tasklet、workqueue的比较


| 类型 | 执行时机 | 执行上下文 | 性能 | 应用场景 |
| ------ | ------ | ------ | ------ | ------ |
| task_work | 当前进程即将返回用户态前 | 进程上下文（当前进程） | 极高 | syscall 清理、延迟资源释放、io_uring |
| tasklet | 中断完成后调度 | softirq 上下文 | 高 | 非阻塞、极短的中断后半处理 |
| workqueue | 内核线程执行 | 进程上下文（可睡眠） | 相对慢 | 可睡眠任务、复杂逻辑、需要等待资源 |