#include "vmlinux.h"
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

#define ETH_P_IP  0x0800 /* Internet Protocol packet	*/
#define IPPROTO_UDP 17

struct http_event {
	__u32 src_ip;
	__u32 dst_ip;
	__u16 src_port;
	__u16 dst_port;
	char payload[1024];
};

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256*1024);
} rb SEC(".maps");

//根据lsm_hook_defs.h里面的hooks找
SEC("xdp")
int xdp_pass(struct xdp_md *ctx)
{
	void *data = (void *)(long)ctx->data;
	void *data_end = (void *)(long)ctx->data_end;
	int pkt_sz = data_end - data;

	bpf_printk("packet size: %d\n", pkt_sz);

	//解析以太网头 (Ethernet Header)
	struct ethhdr *eth = data;
	if ((void *)(eth + 1) > data_end)
		return XDP_PASS;

	//限制只处理 IPv4 流量
	if (bpf_htons(eth->h_proto) != ETH_P_IP)
		return XDP_PASS;

	//解析IP头
	struct iphdr *iph = (void *)(eth + 1);

	//IP头边界检查
	if ((void *)(iph + 1) > data_end)
		return XDP_PASS;

	//判断是不是TCP包
	if (iph->protocol != IPPROTO_TCP)
		return XDP_PASS;

	//解析TCP头
	struct tcphdr *tcph = (void *)iph + (iph->ihl * 4);
	if ((void *)(tcph + 1) > data_end)
		return XDP_PASS;

	//如果目的端口是 8888，直接在网卡层丢弃它！
	if (bpf_htons(tcph->dest) == 8888) {
		bpf_printk("【XDP 拦截】成功干掉了一个发往 8888 端口的恶意TCP包！\n");
		return XDP_DROP;
	}

	//过滤端口：通常 HTTP 走 80 端口（这里监控源或目的为 80 端口的流量）
	// 如果你要监控 8080 等自定义端口，修改这里的过滤条件即可
	__u16 src_port = bpf_ntohs(tcph->source);
	__u16 dst_port = bpf_ntohs(tcph->dest);
	if (src_port != 80 && dst_port != 80) {
		return XDP_PASS;
	}

	//计算 HTTP 载荷（Payload）在数据包中的绝对起始位置
	//tcph->doff * 4 是 TCP 报文头部的实际长度
	void *payload = (void *)tcph + (tcph->doff * 4);

	//确保 Payload 中至少有一些数据（防止握手期间的纯 ACK 包空跑）
	if (payload > data_end)
		return XDP_PASS;

	struct http_event *e;
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	if (!e) return XDP_PASS;

	e->src_ip = iph->saddr;
	e->dst_ip = iph->daddr;
	e->src_port = src_port;
	e->dst_port = dst_port;

	//计算当前网络包里剩余的可读 Payload 长度
	__u64 available_len = (__u64)data_end - (__u64)payload;
	__u64 copy_len = available_len < 1024 ? available_len : 1024;

	//清空缓冲区
	__builtin_memset(e->payload, 0, sizeof(e->payload));

	//核心安全读取】使用 bpf_probe_read_kernel 将数据包中的 HTTP 明文拷贝到 BPF 栈结构体
	//必须再次确保护理边界，以下判断让 Verifier 能够通过安全校验
	if ((void *)payload + copy_len <= data_end) {
		bpf_probe_read_kernel(e->payload, copy_len, payload);
	}
	bpf_ringbuf_submit(e, 0);
	return XDP_PASS;
}
