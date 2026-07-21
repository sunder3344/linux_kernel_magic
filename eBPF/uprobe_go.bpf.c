// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright (c) 2021 Sartura */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct event {
	__u32 pid;
	char stock_code[16];
};

struct ret_event {
	__u32 pid;
	char ret_str[64];
};

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256*1024);
} rb SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256*1024);
} rb_ret SEC(".maps");

SEC("uprobe")
int BPF_UPROBE(uprobe_fetch_stock) {
	char code_name[16] = {};
	struct pt_regs *regs = (struct pt_regs *)ctx;
	//void *str_ptr = (void *)PT_REGS_PARM2(regs);
	//size_t str_len = (size_t)PT_REGS_PARM3(regs);
	u64 ax = BPF_CORE_READ(regs, ax);
	u64 bx = BPF_CORE_READ(regs, bx);
	u64 cx = BPF_CORE_READ(regs, cx);
	void *str_ptr = (void *)bx;
	size_t str_len = cx;

	struct event *e;
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	if (!e) return 0;

	e->pid = bpf_get_current_pid_tgid() >> 32;

	if (str_len > sizeof(e->stock_code) - 1) {
		str_len = sizeof(e->stock_code) - 1;
	}
	bpf_probe_read_user_str(code_name, sizeof(code_name), str_ptr);
	bpf_printk("UPROBE fetch pid = %d, code_name = %s\n", e->pid, code_name);
	/*bpf_printk("di=%llx", PT_REGS_PARM1(regs));
	bpf_printk("si=%llx", PT_REGS_PARM2(regs));
	bpf_printk("dx=%llx", PT_REGS_PARM3(regs));
	bpf_printk("cx=%llx", PT_REGS_PARM4(regs));
	bpf_printk("r8=%llx", PT_REGS_PARM5(regs));*/

	bpf_probe_read_user(&e->stock_code, str_len, str_ptr);
	bpf_ringbuf_submit(e, 0);
	return 0;
}

//这里抓go的函数返回值会导致go程序崩溃（当你在 Go 程序中挂载 uretprobe 时，内核会强行篡改 Go 协程栈上的返回地址（Return PC），将其替换为一个隐藏的内核特权地址（如你日志中看到的 0x7fffffffe000），当Go程序运行到 FetchStock 内部，刚好触发了 Go 的栈扩容（[copystack] / runtime.newstack）或垃圾回收时，Go 运行时会扫描整个调用栈。一看到 0x7fffffffe000 这个不属于 Go 领域的异类地址，Go 就会认为栈被破坏了，直接抛出 fatal error: unknown caller pc 崩溃退出。）
/*SEC("uretprobe")
int BPF_URETPROBE(uretprobe_fetch_stock)
{
    struct pt_regs *regs = (struct pt_regs *)ctx;

    u64 ax = BPF_CORE_READ(regs, ax);
    u64 bx = BPF_CORE_READ(regs, bx);
    u64 cx = BPF_CORE_READ(regs, cx);

    void *ret_ptr = (void *)ax;
    size_t ret_len = bx;

    struct ret_event *e;
    e = bpf_ringbuf_reserve(&rb_ret, sizeof(*e), 0);

    if (!e) {
		return 0;
	}

    e->pid = bpf_get_current_pid_tgid() >> 32;

    if (ret_len > sizeof(e->ret_str) - 1) {
		ret_len = sizeof(e->ret_str) - 1;
	}

    bpf_probe_read_user_str(&e->ret_str, ret_len, ret_ptr);
    bpf_printk("UPROBE fetch pid = %d, ret_len = %d\n", e->pid, ret_len);
    bpf_ringbuf_submit(e, 0);

    return 0;
}*/
