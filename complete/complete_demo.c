#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct completion compl;

static int shared_resource = 0;

static struct task_struct *my_thread;

static int my_thread_func(void *data) {
	printk(KERN_INFO "Thread started\n");

	msleep(2000);

	shared_resource += 1;
	printk(KERN_INFO "Shared resource updated: %d\n", shared_resource);

	complete(&compl);
	printk(KERN_INFO "Thread completed!\n");
	return 0;
}

static int __init my_module_init(void) {
	init_completion(&compl);
	
	my_thread = kthread_run(my_thread_func, NULL, "my_thread");
	if (IS_ERR(my_thread)) {
		printk(KERN_ERR "Failed to create thread\n");
		return PTR_ERR(my_thread);
	}
	
	printk(KERN_INFO "Waiting for completion...\n");
	wait_for_completion(&compl);

	printk(KERN_INFO "Completion received!\n");
	my_thread = NULL;
	return 0;
}

static void __exit my_module_exit(void) {
	if (my_thread) {
		printk(KERN_INFO "start kill thread\n");
		int ret = kthread_stop(my_thread);
		if (ret == -EINTR) {
			printk(KERN_INFO "Thread has already stopped!\n");
		} else {
			printk(KERN_INFO "Thread stopped with code: %d\n", ret);
		}
	} else {
		printk(KERN_INFO "Thread was not created!\n");
	}

	printk(KERN_INFO "Module exited\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("completion demo");
