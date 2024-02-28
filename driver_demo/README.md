# linux driver demo（linux 3.10）

linux驱动开发demo

1. make编译，加载模块insmod driver_demo.ko
2. cat /proc/devices查看注册的设备文件的设备号（例如：241）
3. 新建设备文件mknod /dev/module_connect c 241 0
4. gcc -o main main.c并执行，dmesg -w监控可得结果，触发read事件也可以直接cat /dev/module_connect