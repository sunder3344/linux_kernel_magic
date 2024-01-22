#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <asm/pgtable_types.h>
#include <asm/pgtable_64.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");

extern struct list_head slab_caches;
LIST_HEAD(slab_caches);

static int __init my_oo_init(void) {
	char *a = "asdfasdfasfasd";
	struct page *pg;
	unsigned long phy_addr = 0;

	printk("address of a := %p\n", a);
	pg = virt_to_page(a);
	if (pg) {
		phy_addr = page_to_phys(pg);
	}
	printk("physical addr of a := %lu, %lx\n", phy_addr, phy_addr);
	return (0);
}

static void __exit my_oo_exit(void) {
	printk("Goodbye world\n");
}



module_init(my_oo_init);
module_exit(my_oo_exit);
