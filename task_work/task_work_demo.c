#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/init_task.h>
#include <linux/uaccess.h>
#include <linux/task_work.h>

struct demo_work {
	struct callback_head work;
	int data;
};

static void my_task_work_callback(struct callback_head *work) {
	struct demo_work *dwork = container_of(work, struct demo_work, work);
	printk(KERN_INFO "task_work is running in process: %s(pid: %d), data=%d\n", current->comm, current->pid, dwork->data);
}

static int __init demo_init(void) {
	struct demo_work *dwork;
	struct task_struct *task = current;			//current process
	
	dwork = kzalloc(sizeof(*dwork), GFP_KERNEL);
	if (!dwork)
		return -ENOMEM;

	dwork->data = 3344;
	dwork->work.func = my_task_work_callback;

	printk(KERN_INFO "Registering task_work to current process: %s(pid: %d)\n", task->comm, task->pid);

	//add ur task work to the global task_work hlist
	task_work_add(task, &dwork->work, TWA_RESUME);
	return 0;
}

static void __exit demo_exit(void) {
	printk(KERN_INFO "task_work demo module exit!\n");
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("task work demo");
