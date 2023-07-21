#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

struct EntityElement {
	char *name;
	int age;
	char gender;
};

struct kfifo fifo;

#define FIFO_ELEMENT_MAX 32

static int __init fifo_in(void) {
	int ret = 0;
	int i;
	int count = 2;
	struct EntityElement element[2];
	
	ret = kfifo_alloc(&fifo, sizeof(struct EntityElement) * FIFO_ELEMENT_MAX, GFP_KERNEL);
	if (ret) {
		printk(KERN_ERR "kfifo_alloc fail ret = %d\n", ret);
	}

	element[0].name = "derek";
	element[0].age = 33;
	element[0].gender = 'M';

	element[1].name = "sunder";
	element[1].age = 44;
	element[1].gender = 'F';

	//in
	for (i = 0; i < count; i++) {
		ret = kfifo_in(&fifo, &element[i], sizeof(struct EntityElement));
		printk(KERN_INFO "insert into kfifo num = %d\n", ret);
		if (!ret) {
			printk(KERN_ERR "kfifo_in fail, fifo is full\n");
		}
	}

	//out
	/*struct EntityElement element2;
	ret = kfifo_out(fifo, &element2, sizeof(struct EntityElement));
	if (ret) {
		printk(KERN_INFO "kfifo_out element name = %s, age = %d, gender = %c\n", element2.name, element2.age, element.gender);
	} else {
		printk(KERN_INFO "kfifo_out fail, fifo is empty\n");
	}*/
	return 0;
}

static void __exit fifo_out(void) {	
	int ret = 0;
	struct EntityElement *element2 = kmalloc(sizeof(struct EntityElement), GFP_KERNEL);
	printk(KERN_INFO "element2 = %p\n", element2);
	ret = kfifo_out(&fifo, element2, sizeof(struct EntityElement)*2);
	if (ret) {
		printk(KERN_INFO "ret = %d, element2 = %p\n", ret, element2);
		printk(KERN_INFO "element2[0] name = %s, age = %d, gender = %c\n", element2->name, element2->age, element2->gender);
		element2 += 1;
		printk(KERN_INFO "element2[1] name = %s, age = %d, gender = %c\n", element2->name, element2->age, element2->gender);
		//printk(KERN_INFO "kfifo_out element[1] name = %s, age = %d, gender = %c\n", element2.name, element2.age, element2.gender);
	} else {
		printk(KERN_INFO "kfifo_out fail, fifo is empty\n");
	}
	kfifo_free(&fifo);
}

module_init(fifo_in);
module_exit(fifo_out);
