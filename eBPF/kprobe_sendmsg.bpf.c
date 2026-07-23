// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright (c) 2021 Sartura */
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

SEC("kprobe/tcp_sendmsg")
int BPF_KPROBE(tcp_sendmsg, struct sock *sk, struct msghdr *msg, size_t size)
{
	pid_t pid;
	pid = bpf_get_current_pid_tgid() >> 32;

	unsigned int iter_type;
	bpf_probe_read_kernel(&iter_type, sizeof(iter_type), &msg->msg_iter.iter_type);

	const struct iovec *iov;
	bpf_probe_read_kernel(&iov, sizeof(iov), &msg->msg_iter.iov);

	if (iov) {
		void *base;
		size_t iov_len;

		bpf_probe_read_kernel(&base, sizeof(base), &iov->iov_base);
		bpf_probe_read_kernel(&iov_len, sizeof(iov_len), &iov->iov_len);

		if (base && iov_len > 0) {
			char payload[256] = {0};
			bpf_probe_read_user(payload, sizeof(payload) - 1, base);
			bpf_printk("pid %d send payload: %s\n", pid, payload);
		}
	}
	return 0;
}
