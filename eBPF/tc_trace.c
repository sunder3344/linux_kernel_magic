// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2020 Facebook */
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <signal.h>
#include <string.h>
#include "tc_trace.skel.h"

static volatile sig_atomic_t exiting = 0;

struct http_event {
	__u32 src_ip;
	__u32 dst_ip;
	__u16 src_port;
	__u16 dst_port;
	char payload[256];
};

static void sig_int(int signo)
{
	exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	return vfprintf(stderr, format, args);
}

static int handle_event(void *ctx, void *data, size_t data_sz) {
	const struct http_event *e = data;
	struct in_addr src_addr = {.s_addr = e->src_ip};
	struct in_addr dst_addr = {.s_addr = e->dst_ip};

	if (strstr(e->payload, "GET") || strstr(e->payload, "POST") || strstr(e->payload, "HTTP")) {
		printf("\n================ [ 捕获到 HTTP 报文 ] ================\n");
		printf("来源: %s:%d -> 目的: %s:%d\n", inet_ntoa(src_addr), e->src_port, inet_ntoa(dst_addr), e->dst_port);
		printf("---- 内容明文 (前128字节) ----\n%s\n", e->payload);
		printf("======================================================\n");
	}
	return 0;
}

int main(int argc, char **argv)
{
	bool hook_created = false;
	struct tc_trace_bpf *skel;
	int err;
	struct ring_buffer *rb = NULL;

	const char *ifname = (argc > 1) ? argv[1] : "lo";
	int ifindex = if_nametoindex(ifname);
	if (ifindex == 0) {
		fprintf(stderr, "无效网卡: %s\n", ifname);
		return 1;
	}
	//声明tc_hook,tc_opts
	DECLARE_LIBBPF_OPTS(bpf_tc_hook, tc_hook, .ifindex = ifindex, .attach_point = BPF_TC_INGRESS);
	DECLARE_LIBBPF_OPTS(bpf_tc_opts, tc_opts, .handle = 1, .priority = 1);


	libbpf_set_print(libbpf_print_fn);

	skel = tc_trace_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	/* The hook (i.e. qdisc) may already exists because:
	 *   1. it is created by other processes or users
	 *   2. or since we are attaching to the TC ingress ONLY,
	 *      bpf_tc_hook_destroy does NOT really remove the qdisc,
	 *      there may be an egress filter on the qdisc
	 */
	err = bpf_tc_hook_create(&tc_hook);
	if (!err)
		hook_created = true;
	if (err && err != -EEXIST) {
		fprintf(stderr, "Failed to create TC hook: %d\n", err);
		goto cleanup;
	}

	tc_opts.prog_fd = bpf_program__fd(skel->progs.tc_http_parse);
	err = bpf_tc_attach(&tc_hook, &tc_opts);
	if (err) {
		fprintf(stderr, "Failed to attach TC: %d\n", err);
		goto cleanup;
	}

	if (signal(SIGINT, sig_int) == SIG_ERR) {
		err = errno;
		fprintf(stderr, "Can't set signal handler: %s\n", strerror(errno));
		goto cleanup;
	}

	printf("Successfully started! Please run `sudo cat /sys/kernel/debug/tracing/trace_pipe` "
	       "to see output of the BPF program.\n");

	rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
	if (!rb) {
		fprintf(stderr, "Ringbuf初始化失败\n");
		goto cleanup;
	}

	printf("HTTP 流量探测器已在网卡 [%s] 上启动，正在监听 80 端口数据包...\n", ifname);
	while (!exiting) {
		ring_buffer__poll(rb, 100);
	}

	tc_opts.flags = tc_opts.prog_fd = tc_opts.prog_id = 0;
	err = bpf_tc_detach(&tc_hook, &tc_opts);
	if (err) {
		fprintf(stderr, "Failed to detach TC: %d\n", err);
		goto cleanup;
	}

cleanup:
	if (hook_created)
		bpf_tc_hook_destroy(&tc_hook);
	ring_buffer__free(rb);
	tc_trace_bpf__destroy(skel);
	return -err;
}
