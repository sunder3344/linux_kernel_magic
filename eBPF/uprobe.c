// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2020 Facebook */
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "uprobe.skel.h"

struct event {
	__u32 pid;
	char stock_code[16];
};

/*struct ret_event {
	__u32 pid;
	char ret_str[64];
};*/

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	return vfprintf(stderr, format, args);
}

static int handle_event(void *ctx, void *data, size_t data_sz) {
	const struct event *e = data;
	printf("[eBPF 探测器捕获] PID: %d 正在调用 FetchStock, 请求股票代码: %s\n", e->pid, e->stock_code);
	return 0;
}

/*static int handle_ret_event(void *ctx, void *data, size_t data_sz) {
	const struct ret_event *e = data;
	printf("[eBPF 探测器捕获] PID: %d FetchStock 返回值前段: %s\n", e->pid, e->ret_str);
	return 0;
}*/

int main(int argc, char **argv)
{
	struct uprobe_bpf *skel;
	int err;

	/* Set up libbpf errors and debug info callback */
	libbpf_set_print(libbpf_print_fn);

	/* Open BPF application */
	skel = uprobe_bpf__open();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	/* Load & verify BPF programs */
	err = uprobe_bpf__load(skel);
	if (err) {
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		goto cleanup;
	}

	/* Attach tracepoint handler */
	struct bpf_uprobe_opts opts = {
	    .sz = sizeof(opts),
	    .func_name = "main.FetchStock",
	};

	skel->links.uprobe_fetch_stock =
	    bpf_program__attach_uprobe_opts(
	        skel->progs.uprobe_fetch_stock,
	        -1,
	        "/home/sunder/bin/go/fetch",
	        0,
	        &opts
		);

	if (!skel->links.uprobe_fetch_stock) {
		fprintf(stderr, "Uprobe 挂载失败，请检查 ./fetch 路径及符号是否正确！\n");
		goto cleanup;
	}

	/*struct bpf_uprobe_opts ret_opts = {
		.sz = sizeof(ret_opts),
		.func_name = "main.FetchStockTraceWrapper",
		.retprobe = true, // <<< 关键：指定这是一个 retprobe
	};

	skel->links.uretprobe_fetch_stock =
		bpf_program__attach_uprobe_opts(
			skel->progs.uretprobe_fetch_stock,
			-1,
			"/home/sunder/bin/go/fetch",
			0,
			&ret_opts
		);

	if (!skel->links.uretprobe_fetch_stock) {
		fprintf(stderr, "Uretprobe 挂载失败，请检查 ./fetch 路径及符号是否正确！\n");
		goto cleanup;
	}*/


	printf("Successfully started! Please run `sudo cat /sys/kernel/debug/tracing/trace_pipe` "
	       "to see output of the BPF programs.\n");

	struct ring_buffer *rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
	//struct ring_buffer *rb_ret = ring_buffer__new(bpf_map__fd(skel->maps.rb_ret), handle_ret_event, NULL, NULL);
	while (1) {
		ring_buffer__poll(rb, 100);
		//ring_buffer__poll(rb_ret, 100);
	}

	for (;;) {
		/* trigger our BPF program */
		fprintf(stderr, ".");
		sleep(1);
	}

cleanup:
	uprobe_bpf__destroy(skel);
	return -err;
}
