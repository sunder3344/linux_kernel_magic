#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "my_device"
#define CLASS_NAME "my_class"
#define MY_IOCTL_MAGIC 'k'

#define MY_IOCTL_SET_STATUS _IOW(MY_IOCTL_MAGIC, 1, int)
#define MY_IOCTL_GET_STATUS _IOR(MY_IOCTL_MAGIC, 2, int)

static int device_status = 0;

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *driver_class;

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	int ret = 0;

	switch(cmd) {
		case MY_IOCTL_SET_STATUS:
			if (copy_from_user(&device_status, (int __user *)arg, sizeof(int))) {
				ret = -EFAULT;
			} else {
				printk(KERN_INFO "Device status set to: %d\n", device_status);
			}
			break;
		case MY_IOCTL_GET_STATUS:
			if (copy_to_user((int __user *)arg, &device_status, sizeof(int))) {
				ret = -EFAULT;
			} else {
				printk(KERN_INFO "Device status read: %d\n", device_status);
			}
			break;
		default:
			ret = -ENOTTY;			//unsupport command
			break;
	}
	return ret;
}

static const struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = my_ioctl
};

static int __init my_module_init(void) {
	if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
		printk(KERN_ERR "Failed to allocate device number\n");
		return -1;
	}
	driver_class = class_create(THIS_MODULE, CLASS_NAME);

	cdev_init(&my_cdev, &my_fops);
	my_cdev.owner = THIS_MODULE;

	if (cdev_add(&my_cdev, dev_num, 1) < 0) {
		printk(KERN_ERR "Failed to add cdev\n");
		unregister_chrdev_region(dev_num, 1);
		return -1;
	}
	
	device_create(driver_class, NULL, dev_num, NULL, DEVICE_NAME);

	printk(KERN_INFO "Device registered with major number: %d\n", MAJOR(dev_num));
	return 0;
}

static void __exit my_module_exit(void) {
	unregister_chrdev_region(dev_num, 1);
	device_destroy(driver_class, dev_num);	
	cdev_del(&my_cdev);
	class_destroy(driver_class);
	printk(KERN_INFO "Device unregistered\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("ioctl demo");
