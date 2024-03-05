#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define DEVICE_NAME "module_connect"
#define CLASS_NAME "driver_connect"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sunder3344");

static dev_t dev_num;
struct cdev cdev;
struct class *driver_class;

static int device_open(struct inode *inode, struct file *file) {
	printk(KERN_INFO "devic open\n");
	return 0;
}

static int device_release(struct inode *inode, struct file *file) {
	printk(KERN_INFO "device release\n");
	return 0;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
	char *data = kmalloc(length + 1, GFP_KERNEL);
	int ret = 0;
	memset(data, 0, length + 1);
	printk(KERN_INFO "device write %zu\n", length);
	
	if (!data) {
		printk(KERN_INFO "Failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = copy_from_user(data, buffer, length);
	if (ret < 0) {
		printk(KERN_ERR "Failed to copy data from user space\n");
		kfree(data);
		return -EFAULT;
	}

	data[length] = '\0';
	printk(KERN_INFO "Received %zu data from user space: %s\n", length, data);

	kfree(data);
	return length;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
	//char *output = kmalloc(1024, GFP_KERNEL);
	int ret = 0;
	//memset(output, 0, 1024);
	//output = "this is an echo from core to user!";
	printk(KERN_INFO "length = %zu\n", length);
	ret = copy_to_user(buffer, "derek sunder", 12);
	if (ret < 0) {
		printk(KERN_ERR "Failed to send data to user space\n");
        return -EFAULT;
	}
	printk(KERN_INFO "send data to user space successful!\n");
	return length;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.release = device_release,
	.write = device_write,
	.read = device_read,
};

static int __init demo_module_init(void) {
	int ret;
	printk(KERN_INFO "driver demo module initialized\n");
	//register_chrdev(0, DEVICE_NAME, &fops);
	if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
		printk(KERN_DEBUG "can't register device\n");
		return -1;
	}

	driver_class = class_create(THIS_MODULE, CLASS_NAME);
	cdev_init(&cdev, &fops);
	cdev.owner = THIS_MODULE;
	
	ret = cdev_add(&cdev, dev_num, 1);
	if (ret) {
		printk("add cdev error!\n");
		return ret;
	}
	device_create(driver_class, NULL, dev_num, NULL, DEVICE_NAME);
	return 0;
}

static void __exit demo_module_exit(void) {
	printk(KERN_INFO "driver demo module exited.\n");
	//unregister_chrdev(0, DEVICE_NAME);
	unregister_chrdev_region(dev_num, 1);
	device_destroy(driver_class, dev_num);
	cdev_del(&cdev);
	class_destroy(driver_class);
}

module_init(demo_module_init);
module_exit(demo_module_exit);
