// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright (c) 2021 Sartura */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct event {
	unsigned int pid;
	long ret;
	char filename[256];
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, __u32);
	__type(value, char[256]);
} infomap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256*1024);
} rb SEC(".maps");

SEC("kprobe/do_unlinkat")
int BPF_KPROBE(do_unlinkat_entry, int dfd, struct filename *name) {
	pid_t pid;
	char filename[256] = {};
	pid = bpf_get_current_pid_tgid() >> 32;

	const char *name_ptr = BPF_CORE_READ(name, name);
	if (!name_ptr) return 0;

	bpf_probe_read_kernel_str(filename, sizeof(filename), name_ptr);

	bpf_map_update_elem(&infomap, &pid, filename, BPF_ANY);

	//bpf_printk("KPROBE do_unlinkat pid = %d, name = %s\n", pid, fileName);
	return 0;
}

SEC("kretprobe/do_unlinkat")
int BPF_KRETPROBE(do_unlinkat_exit, long error) {
	pid_t pid;
	pid = bpf_get_current_pid_tgid() >> 32;

	char *cached_filename = bpf_map_lookup_elem(&infomap, &pid);
	if (!cached_filename)
		return 0;

	struct event *e;
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	if (!e) {
		bpf_map_delete_elem(&infomap, &pid);
		return 0;
	}

	e->pid = pid;
	e->ret = error;
	bpf_probe_read_str(e->filename, sizeof(e->filename), cached_filename);
	bpf_ringbuf_submit(e, 0);

	bpf_map_delete_elem(&infomap, &pid);

	bpf_printk("KPROBE do_unlinkat pid = %d, ret = %ld\n", pid, error);
	return 0;
}
