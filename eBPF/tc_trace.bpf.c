// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright (c) 2021 Sartura */
#include "vmlinux.h"
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define TC_ACT_OK 0
#define ETH_P_IP  0x0800 /* Internet Protocol packet	*/

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct http_event {
	__u32 src_ip;
	__u32 dst_ip;
	__u16 src_port;
	__u16 dst_port;
	char payload[256];
};

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256*1024);
} rb SEC(".maps");

SEC("tc")
int tc_http_parse(struct __sk_buff *ctx) {
	void *data_end = (void *)(__u64)ctx->data_end;
	void *data = (void *)(__u64)ctx->data;

	struct ethhdr *l2;
	struct iphdr *l3;
	struct tcphdr *l4;

	//限制只处理 IPv4 流量
	if (ctx->protocol != bpf_htons(ETH_P_IP))
		return TC_ACT_OK;

	//解析以太网头 (L2) 并做边界检查
	l2 = data;
	if ((void *)(l2 + 1) > data_end)
		return TC_ACT_OK;

	//解析 IP 头 (L3) 并做边界检查
	l3 = (struct iphdr *)(l2 + 1);
	if ((void *)(l3 + 1) > data_end)
		return TC_ACT_OK;

	//限制只处理 TCP 协议
	if (l3->protocol != IPPROTO_TCP)
		return TC_ACT_OK;

	//解析 TCP 头 (L4)
    //注意：IP 头的长度需要通过 l3->ihl * 4 动态计算，因为可能有可选字段
	l4 = (struct tcphdr *)((void *)l3 + (l3->ihl * 4));
	if ((void *)(l4 + 1) > data_end)
		return TC_ACT_OK;

	//过滤端口：通常 HTTP 走 80 端口（这里监控源或目的为 80 端口的流量）
	// 如果你要监控 8080 等自定义端口，修改这里的过滤条件即可
	__u16 src_port = bpf_ntohs(l4->source);
	__u16 dst_port = bpf_ntohs(l4->dest);
	if (src_port != 80 && dst_port != 80) {
		return TC_ACT_OK;
	}

	//计算 HTTP 载荷（Payload）在数据包中的绝对起始位置
	//l4->doff * 4 是 TCP 报文头部的实际长度
	void *payload = (void *)l4 + (l4->doff * 4);

	//确保 Payload 中至少有一些数据（防止握手期间的纯 ACK 包空跑）
	if (payload > data_end)
		return TC_ACT_OK;

	struct http_event *e;
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	if (!e) return TC_ACT_OK;

	e->src_ip = l3->saddr;
	e->dst_ip = l3->daddr;
	e->src_port = src_port;
	e->dst_port = dst_port;

	//计算当前网络包里剩余的可读 Payload 长度
	__u64 available_len = (__u64)data_end - (__u64)payload;
	__u64 copy_len = available_len < 255 ? available_len : 256;

	//清空缓冲区
	__builtin_memset(e->payload, 0, sizeof(e->payload));

	//核心安全读取】使用 bpf_probe_read_kernel 将数据包中的 HTTP 明文拷贝到 BPF 栈结构体
    //必须再次确保护理边界，以下判断让 Verifier 能够通过安全校验
	if ((void *)payload + copy_len <= data_end) {
		bpf_probe_read_kernel(e->payload, copy_len, payload);
	}
	bpf_ringbuf_submit(e, 0);
	return TC_ACT_OK;
}
