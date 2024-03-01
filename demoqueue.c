#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sunder3344");

static struct workqueue_struct *my_workqueue;

static struct work_struct my_work;

static void my_work_function(struct work_struct *work) {
	printk(KERN_INFO "Workqueue example: Work function executed\n");
}

static int __init my_module_init(void) {
	printk(KERN_INFO "Workqueue example: Module initialized\n");

	my_workqueue = create_workqueue("my_workqueue");
	if (!my_workqueue) {
		printk(KERN_ERR "Workqueue example: Failed to create workqueue\n");
        return -ENOMEM;
	}

	INIT_WORK(&my_work, my_work_function);

	queue_work(my_workqueue, &my_work);
	
	return 0;
}

static void __exit my_module_exit(void) {
	cancel_work_sync(&my_work);

	destroy_workqueue(my_workqueue);

	printk(KERN_INFO "Workqueue example: Module exited\n");
}

module_init(my_module_init);
module_exit(my_module_exit);
