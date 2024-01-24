#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");

struct student{
	int id;
	char *name;
	struct list_head list;
};

void print_student(struct student *);

static int __init testlist_init(void) {
	struct student *stu1, *stu2, *stu3, *stu4, *stu5, *tmp;
	struct student *stu;
	// init a list head
	LIST_HEAD(stu_head);
	// init your list nodes
	stu1 = kmalloc(sizeof(*stu1), GFP_KERNEL);
	stu1->id = 1;
	stu1->name = "sunder";
	INIT_LIST_HEAD(&stu1->list);
	
	stu2 = kmalloc(sizeof(*stu2), GFP_KERNEL);
	stu2->id = 2;
	stu2->name = "derek";
	INIT_LIST_HEAD(&stu2->list);
	
	stu3 = kmalloc(sizeof(*stu3), GFP_KERNEL);
	stu3->id = 3;
	stu3->name = "jascha";
	INIT_LIST_HEAD(&stu3->list);

	stu4 = kmalloc(sizeof(*stu4), GFP_KERNEL);
	stu4->id = 4;
	stu4->name = "senior";
	INIT_LIST_HEAD(&stu4->list);

	list_add(&stu1->list, &stu_head);
	list_add(&stu2->list, &stu_head);
	list_add(&stu3->list, &stu_head);
	list_add(&stu4->list, &stu_head);

	list_for_each_entry(stu, &stu_head, list) {
		print_student(stu);
	}

	//delete an entry stu2
	list_del(&stu2->list);
	kfree(stu2);
	list_for_each_entry(stu, &stu_head, list) {
		print_student(stu);
	}

	//replace stu3 with stu1
	stu5 = kmalloc(sizeof(*stu5), GFP_KERNEL);
	stu5->id = 5;
	stu5->name = "rommedahl";
	INIT_LIST_HEAD(&stu5->list);

    list_replace(&stu3->list, &stu5->list);
    list_for_each_entry_safe(stu, tmp, &stu_head, list) {
        print_student(stu);
    }

	return 0;
}

static void __exit testlist_exit(void) {
	printk(KERN_ALERT "*************************\n");
	printk(KERN_ALERT "testlist is exited!\n");
    printk(KERN_ALERT "*************************\n");
}

void print_student(struct student *stu) {
	printk (KERN_ALERT "======================\n");
    printk (KERN_ALERT "id  =%d\n", stu->id);
	printk (KERN_ALERT "name=%s\n", stu->name);
    printk (KERN_ALERT "======================\n");
}

module_init(testlist_init);
module_exit(testlist_exit);
