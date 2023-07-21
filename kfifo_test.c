#include <linux/kfifo.h>

struct EntityElement {
	char *name;
	int age;
	char gender;
};

struct kfifo fifo;

#define FIFO_ELEMENT_MAX 32

static int __init fifo_in(void) {
	int ret = 0;
	struct EntityElement element;
	
	ret = kfifo_alloc(&fifo, sizeof(struct EntityElement) * FIFO_ELEMENT_MAX, GFP_KERNEL);
	if (ret) {
		printk(KERN_ERR "kfifo_alloc fail ret = %d\n", ret);
	}

	element.name = "derek";
	element.age = 37;
	element.gender = 'M';

	//in
	ret = kfifo_in(&fifo, &element, sizeof(struct EntityElement));
	if (!ret) {
		printk(KERN_ERR "kfifo_in fail, fifo is full\n");
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
	struct EntityElement element2;
	ret = kfifo_out(&fifo, &element2, sizeof(struct EntityElement));
	if (ret) {
		printk(KERN_INFO "kfifo_out element name = %s, age = %d, gender = %c\n", element2.name, element2.age, element2.gender);
	} else {
		printk(KERN_INFO "kfifo_out fail, fifo is empty\n");
	}
	kfifo_free(&fifo);
}

module_init(fifo_in);
module_exit(fifo_out);
