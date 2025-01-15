# linux ioctl（linux 6.10）

ioctl 是 Linux 系统中用于设备控制的系统调用，全称为 Input/Output Control。它主要用于与设备驱动程序进行交互，执行一些特定的操作（如配置设备、获取设备状态等），这些操作无法通过标准的读写操作（如 read 或 write）完成。

## 执行方法
1. make编译，加载模块insmod ioctl_demo.ko

2. 编译main.c并执行./main查看输出结果

3. 卸载模块rmmod ioclt_demo


## ioctl的作用

- 设备配置：设置设备的参数，如串口的波特率、网络接口的 IP 地址等。

- 获取设备状态：读取设备的状态信息，如传感器的数据、网络接口的统计信息等。

- 执行特殊操作：执行设备的特殊操作，如重置设备、启动/停止设备等。