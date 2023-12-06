#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("timer_list demo");

static struct timer_list my_timer;

//callback function of timer
void my_timer_callback(struct timer_list *timer) {
	pr_info("Timer expired!\n");

	mod_timer(timer, jiffies + msecs_to_jiffies(1000));
}

static int __init my_module_init(void) {
	pr_info("Module loaded\n");
	
	//init timer
	timer_setup(&my_timer, my_timer_callback, 0);
	//set timer trigger time after 1000 millsecs
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000));
	
	return 0;
}

static void __exit my_module_exit(void) {
	del_timer(&my_timer);
	pr_info("Module unloaded\n");
}

module_init(my_module_init);
module_exit(my_module_exit);
