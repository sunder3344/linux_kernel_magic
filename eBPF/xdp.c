// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2024 David Di */
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <signal.h>
#include "xdp.skel.h"

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

/* Notice: Ensure your kernel version is 5.7 or higher, BTF (BPF Type Format) is enabled,
 * and the file '/sys/kernel/security/lsm' includes 'bpf'.
 */
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
	struct xdp_bpf *skel;
	int err;
	struct ring_buffer *rb = NULL;

	libbpf_set_print(libbpf_print_fn);

	const char *ifname = (argc > 1) ? argv[1] : "lo";
	int ifindex = if_nametoindex(ifname);
	if (ifindex == 0) {
		fprintf(stderr, "无效网卡: %s\n", ifname);
		return 1;
	}

	skel = xdp_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	//老式的挂载方式，即使用户端退出，xdp还是挂载在内核，不能自动退出，建议用bpf_program__attach_xdp
	/*int prog_fd = bpf_program__fd(skel->progs.xdp_pass);
	err = bpf_xdp_attach(ifindex, prog_fd, 0, NULL);
	if (err) { fprintf(stderr, "XDP 挂载网卡失败\n"); goto cleanup; }*/

	skel->links.xdp_pass = bpf_program__attach_xdp(skel->progs.xdp_pass, ifindex);
	if (!skel->links.xdp_pass) {
	    err = -errno;
	    fprintf(stderr, "XDP 挂载网卡失败: %s\n", strerror(errno));
	    goto cleanup;
	}

	if (signal(SIGINT, sig_int) == SIG_ERR) {
		err = errno;
		fprintf(stderr, "Can't set signal handler: %s\n", strerror(errno));
		goto cleanup;
	}

	rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);
	if (!rb) {
		fprintf(stderr, "Ringbuf初始化失败\n");
		goto cleanup;
	}

	printf("XDP已挂载至网卡%s,所有流量已被接管!按下 Ctrl+C 恢复...\n", argv[1]);

	while (!exiting) {
		ring_buffer__poll(rb, 100);
	}
cleanup:
	bpf_xdp_detach(ifindex, 0, NULL); // 退出时善后，恢复网络
    xdp_bpf__destroy(skel);
	return -err;
}
