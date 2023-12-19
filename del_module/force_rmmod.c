#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/list.h>
#include <asm-generic/local.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>


static char *modname = NULL;
static int m_incs = 0;
static int m_decs = 0;
module_param(modname, charp, 0644);
module_param(m_incs, int, 0644);
module_param(m_decs, int, 0644);
MODULE_PARM_DESC(modname, "The name of module you want do clean or delete...\n");

static int __init force_rmmod_init(void) {
	struct module *mod;
	unsigned long incs = 0, decs = 0;
	int cpu;
	
	mod = find_module(modname);
	printk("modname:=%s\n", modname);	
	if (mod) {
		printk("we find module name:=%s\n", mod->name);
		
		//将refptr的decs和incs设置一样，两者相减就是0，表示没有程序占用了
		__this_cpu_write(mod->refptr->decs, m_decs);
		__this_cpu_write(mod->refptr->incs, m_incs);
		
		for_each_possible_cpu(cpu) {
			decs += per_cpu_ptr(mod->refptr, cpu)->decs;
		}

		smp_rmb();

		for_each_possible_cpu(cpu) {
			incs += per_cpu_ptr(mod->refptr, cpu)->incs;
		}


		
		printk("incs = %lu, decs = %lu\n", incs, decs);
	} else {
		printk("find nothing...\n");
	}
	return 0;	
}

static void __exit force_rmmod_exit(void) {
	printk("=======name : %s, state : %d EXIT=======\n", THIS_MODULE->name, THIS_MODULE->state);
}

module_init(force_rmmod_init);
module_exit(force_rmmod_exit);

MODULE_LICENSE("GPL");
