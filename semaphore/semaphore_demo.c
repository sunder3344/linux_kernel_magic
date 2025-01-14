#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>

static struct semaphore my_sem;

static int shared_resource = 0;

static void access_shared_resource(void) {
	if (down_interruptible(&my_sem)) {
		printk(KERN_INFO "Failed to acquire semaphore\n");
		return;
	}
	
	shared_resource++;
	printk(KERN_INFO "Shared resource value: %d\n", shared_resource);
	
	up(&my_sem);
}

static int __init my_module_init(void) {
    sema_init(&my_sem, 1);

	access_shared_resource();
	access_shared_resource();

	
    printk(KERN_INFO "Semaphore test completed!\n");
    return 0;
}

static void __exit my_module_exit(void) {
	printk(KERN_INFO "Module exit!\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("demo of semaphore");
