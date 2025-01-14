#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

static struct tasklet_struct my_tasklet;

static void my_tasklet_handler(unsigned long data) {
	printk(KERN_INFO "Tasklet executed: data = %lu\n", data);
}

static int __init my_module_init(void) {
	//init tasklet
	tasklet_init(&my_tasklet, my_tasklet_handler, 3344);
	//dispatch tasklet
	tasklet_schedule(&my_tasklet);
	
	printk(KERN_INFO "Tasklet scheduled\n");
	return 0;
}

static void __exit my_module_exit(void) {
	//destroy tasklet
	tasklet_kill(&my_tasklet);
	printk(KERN_INFO "Tasklet killed\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("tasklet demo");
