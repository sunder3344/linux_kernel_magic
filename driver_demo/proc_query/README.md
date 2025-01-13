# linux driver debug（linux 6.10）

linux内核驱动开发中利用/proc下文件系统进行调试，老的版本使用create_proc_read_entry实现，新的内核（我的是6.1）使用proc_create和sqe_file来实现，通过在/proc下建立一个文件，对其进行主动查询来获取调试信息。

1. make编译，加载模块insmod proc_demo.ko
2. dmesg -w监控输出
3. 通过cat /proc/my_proc_entry_demo可以看到输出的内容
4. 卸载模块rmmod proc_demo

## 有别与依靠打印printk输出调试信息，这种主动查询的方法更加灵活一些