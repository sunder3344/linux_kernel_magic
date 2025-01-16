#include <linux/percpu.h>
#include <linux/module.h>
#include <linux/kernel.h>

// 定义一个 per-CPU 变量
DEFINE_PER_CPU(int, my_counter);

// 模块初始化函数
static int __init my_module_init(void)
{
    int cpu;

    // 初始化每个 CPU 的变量
    for_each_possible_cpu(cpu) {
        int *ptr = per_cpu_ptr(&my_counter, cpu);
        *ptr = 0; // 初始化为 0
    }

    // 修改当前 CPU 的变量
    int *ptr = &get_cpu_var(my_counter);
    (*ptr)++;
    pr_info("Current CPU counter: %d\n", *ptr);
    put_cpu_var(my_counter);

    // 打印所有 CPU 的变量值
    for_each_possible_cpu(cpu) {
        pr_info("CPU %d counter: %d\n", cpu, *per_cpu_ptr(&my_counter, cpu));
    }

    return 0;
}

// 模块退出函数
static void __exit my_module_exit(void)
{
    pr_info("Module exited\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("Example of using per-CPU variables");
