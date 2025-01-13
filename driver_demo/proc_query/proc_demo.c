#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static void *my_seq_start(struct seq_file *m, loff_t *pos) {
	if (*pos >= 10) {	//只输出10行
		return NULL;
	}
	return pos;
}

static void *my_seq_next(struct seq_file *m, void *v, loff_t *pos) {
	(*pos)++;
	if (*pos >= 10) {
		return NULL;
	}
	return pos;
}

static void my_seq_stop(struct seq_file *m, void *v) {
	//do some cleaning work
}

static int my_seq_show(struct seq_file *m, void *v) {
	loff_t *pos = (loff_t *)v;
	seq_printf(m, "Line %lld\n", *pos);
	return 0;
}

static const struct seq_operations my_seq_ops = {
	.start = my_seq_start,
	.next = my_seq_next,
	.stop = my_seq_stop,
	.show = my_seq_show,
};

static int my_proc_open(struct inode *inode, struct file *file) {
	return seq_open(file, &my_seq_ops);
}

static const struct proc_ops my_proc_fops = {
	.proc_open = my_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

static int __init my_module_init(void) {
	proc_create("my_proc_entry_demo", 0444, NULL, &my_proc_fops);
	printk(KERN_INFO "/proc/my_proc_entry_demo created\n");
	return 0;
}

static void __exit my_module_exit(void) {
	remove_proc_entry("my_proc_entry_demo", NULL);
	printk(KERN_INFO "/proc/my_proc_entry_demo removed\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");
MODULE_DESCRIPTION("proc_create and seq_file demo");
