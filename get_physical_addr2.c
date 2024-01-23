#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("derek sunder");

extern struct list_head slab_caches;
LIST_HEAD(slab_caches);

static int __init my_oo_init(void) {
	char *a = "derek";
	char *b = "sunder";
	struct task_struct *process = current;
	//pgd_t *pgd = process->mm->pgd;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long addr = (unsigned long)a;
	phys_addr_t phy_addr;
	struct page *pg;
	unsigned long phy_addr2 = 0;
	//read from physical addr
	void __iomem *virt_addr, *virt_addr2;
	char *kernel_buffer;
	char *kernel_buffer2;
	
	pgd = pgd_offset(process->mm, addr);
	unsigned long pgd_idx = pgd_index(addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		printk("Invalid PGD %d, %d\n", pgd_none(*pgd), pgd_bad(*pgd));
		return -1;
	}

	pud = pud_offset(pgd, addr);
	unsigned long pud_idx = pud_index(addr);
	unsigned long pud_pfns = pud_pfn(*pud);
	if (pud_none(*pud) || pud_bad(*pud)) {
		printk("Invalid PUD %d, %d\n", pud_none(*pud), pud_bad(*pud));
		return -1;
	}

	pmd = pmd_offset(pud, addr);
	unsigned long pmd_idx = pmd_index(addr);
	unsigned long pmd_pfns = pmd_pfn(*pmd);
	if (pmd_none(*pmd) || pmd_bad(*pmd)) {
		printk("Invalid PMD %d, %d\n", pmd_none(*pmd), pmd_bad(*pmd));
		return -1;
	}

	pte = pte_offset_kernel(pmd, addr);
	unsigned long pte_idx = pte_index(addr);
	unsigned long pte_pfns = pte_pfn(*pte);

	printk("pointer a = %p\n", a);
	printk("pgd = %lu, hex = %lx, pgd_index = %lu\n", (unsigned long)pgd, (unsigned long)pgd, pgd_idx);	
	printk("pud = %lu, hex = %lx, pud_index = %lu, pfn = %lu, pfn hex = %lx\n", (unsigned long)pud, (unsigned long)pud, pud_idx, pud_pfns, pud_pfns);	
	printk("pmd = %lu, hex = %lx, pmd_index = %lu, pfn = %lu, pfn hex = %lx\n", (unsigned long)pmd, (unsigned long)pmd, pmd_idx, pmd_pfns, pmd_pfns);	
	printk("pte = %lu, hex = %lx, pte_index = %lu, pfn = %lu, pfn hex = %lx\n", (unsigned long)pte, (unsigned long)pte, pte_idx, pte_pfns, pte_pfns);	

	//finally, we cant get physical addr with pte
	phy_addr = page_to_phys(pte_page(*pte)) + offset_in_page(addr);
	printk("physical addr of a = %llu, %llx\n", phy_addr, phy_addr);
	
	//如果 a 是一个内核空间的指针，有可能会导致两种方式的结果不同。
	//virt_to_page 函数期望一个用户空间地址，如果给它传递了内核空间地址，可能会导致不一致的结果。不建议这种方法
	pg = virt_to_page(a);
	if (pg) {
		phy_addr2 = page_to_phys(pg);
	}
	printk("physical addr2 of a= %lu, %lx\n", phy_addr2, phy_addr2);
	
	virt_addr = phys_to_virt(phy_addr);
	if (!virt_addr) {
		printk(KERN_ERR "Failed to map physical address to virtual address\n");
        return -EFAULT;
	}
	kernel_buffer = kmalloc(128, GFP_KERNEL);
	if (!kernel_buffer) {
        printk(KERN_ERR "Failed to allocate kernel buffer\n");
        return -ENOMEM;
    }
	memcpy(kernel_buffer, virt_addr, 128);
	printk(KERN_INFO "Value at virtal address %p: %s\n", virt_addr, kernel_buffer);

	//第二种方法获取的物理内存是用户空间的，因此拿到的实际物理内存会有问题，推荐第一种方法
	virt_addr2 = phys_to_virt(phy_addr2);
    if (!virt_addr2) {
        printk(KERN_ERR "Failed to map physical address to virtual address2\n");
        return -EFAULT;
    }

	kernel_buffer2 = kmalloc(128, GFP_KERNEL);
	if (!kernel_buffer2) {
        printk(KERN_ERR "Failed to allocate kernel buffer2\n");
        return -ENOMEM;
    }
	memcpy(kernel_buffer2, virt_addr2, 128);
	printk(KERN_INFO "Value at virtal2 address %p: %s\n", virt_addr2, kernel_buffer2);

	kfree(kernel_buffer);
	kfree(kernel_buffer2);
	
	return 0;
}

static void __exit my_oo_exit(void) {
	printk("Goodbye world\n");
}



module_init(my_oo_init);
module_exit(my_oo_exit);
