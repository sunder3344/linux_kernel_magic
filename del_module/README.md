# rmmod problem（linux 3.10）

当使用rmmod移除linux内核模块时，系统报"rmmod: ERROR: Module XXX is in use"，网上搜到的是[内核3.19以后的解决方法](https://zhuanlan.zhihu.com/p/597326505)，这里列一下3.19之前版本的解决方法，可以从kernel/moudle.c中的module_unload_init方法看到如何去初始化incs的，照着拿来重置计数就行了

```
static int module_unload_init(struct module *mod)
{
	mod->refptr = alloc_percpu(struct module_ref);
	if (!mod->refptr)
		return -ENOMEM;

	INIT_LIST_HEAD(&mod->source_list);
	INIT_LIST_HEAD(&mod->target_list);

	/* Hold reference count during initialization. */
	__this_cpu_write(mod->refptr->incs, 1);
	/* Backwards compatibility macros put refcount during init. */
	mod->waiter = current;

	return 0;
}
```

1. make编译
2. lsmod查看该模块计数有几个占用
3.  insmod force_rmmod.ko modname=*** m_incs=0 m_decs=1 将m_decs后面的数填成该模块计数占用的数即可
4. rmmod *** 直接移除
