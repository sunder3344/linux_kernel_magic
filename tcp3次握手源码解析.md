# tcp三次握手源码解析（linux 6.4.3）



## 客户端发送connect请求（第一次握手，发送SYN）

/net/socket.c

```
 	SYSCALL_DEFINE3(connect, int, fd, struct sockaddr __user *, uservaddr, int, addrlen)
	{
		return __sys_connect(fd, uservaddr, addrlen);
	}
```

调用__sys_connect，

```
	int __sys_connect(int fd, struct sockaddr __user *uservaddr, int addrlen)
	{
	int ret = -EBADF;
	struct fd f;

	f = fdget(fd);
	if (f.file) {
		struct sockaddr_storage address;

		ret = move_addr_to_kernel(uservaddr, addrlen, &address);
		if (!ret)
			ret = __sys_connect_file(f.file, &address, addrlen, 0);
		fdput(f);
	}

	return ret;
}
```

继续调用__sys_connect_file

```
	int __sys_connect_file(struct file *file, struct sockaddr_storage *address,
		       int addrlen, int file_flags)
	{
		...
		...
		err = sock->ops->connect(sock, (struct sockaddr *)address, addrlen,
				 sock->file->f_flags | file_flags);
		...
	}
```

我们这里以ipv4为例，遂调用/net/ipv4/tcp_ipv4.c中的tcp_v4_connect方法

```
	struct proto tcp_prot = {
		.name			= "TCP",
		.owner			= THIS_MODULE,
		.close			= tcp_close,
		.pre_connect	= tcp_v4_pre_connect,
		.connect		= tcp_v4_connect,
		.disconnect		= tcp_disconnect,
		.accept			= inet_csk_accept,
		.ioctl			= tcp_ioctl,
		.init			= tcp_v4_init_sock,
		.destroy		= tcp_v4_destroy_sock,
		.shutdown		= tcp_shutdown,
		.setsockopt		= tcp_setsockopt,
		.getsockopt		= tcp_getsockopt,
		.bpf_bypass_getsockopt	= tcp_bpf_bypass_getsockopt,
		.keepalive		= tcp_set_keepalive,
		.recvmsg		= tcp_recvmsg,
		.sendmsg		= tcp_sendmsg,
		.sendpage		= tcp_sendpage,
		.backlog_rcv	= tcp_v4_do_rcv,
		...
		...
	};
```

ipv4协议中的connect调用的是tcp_v4_connect方法，这里会把sk->sk_state设置成TCP_SYN_SENT我们继续深入

```
	int tcp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
	{
		struct sockaddr_in *usin = (struct sockaddr_in *)uaddr;
		struct inet_timewait_death_row *tcp_death_row;
		struct inet_sock *inet = inet_sk(sk);
		struct tcp_sock *tp = tcp_sk(sk);
		struct ip_options_rcu *inet_opt;
		struct net *net = sock_net(sk);
		__be16 orig_sport, orig_dport;
		__be32 daddr, nexthop;
		struct flowi4 *fl4;
		struct rtable *rt;
		int err;
	
		...
		...
		//这里设置sk->sk_state = TCP_SYN_SENT
		tcp_set_state(sk, TCP_SYN_SENT);
		...
		...
		//tcp连接关键代码
		err = tcp_connect(sk);
		...
		...
	}
	EXPORT_SYMBOL(tcp_v4_connect);
```

进入tcp_connect方法

```
	int tcp_connect(struct sock *sk)
	{
		struct tcp_sock *tp = tcp_sk(sk);
		struct sk_buff *buff;
		int err;
	
		tcp_call_bpf(sk, BPF_SOCK_OPS_TCP_CONNECT_CB, 0, NULL);
	
		if (inet_csk(sk)->icsk_af_ops->rebuild_header(sk))
			return -EHOSTUNREACH; /* Routing failure or similar. */
	
		tcp_connect_init(sk);
	
		...
		...
		//申请一块socket buffer的内存
		buff = tcp_stream_alloc_skb(sk, 0, sk->sk_allocation, true);
		if (unlikely(!buff))
			return -ENOBUFS;
	
		//设置skb的tcp头部信息，标记SYN为1
		tcp_init_nondata_skb(buff, tp->write_seq++, TCPHDR_SYN);
		...
		...
	
		//发送SYN报文
		/* Send off SYN; include data in Fast Open. */
		err = tp->fastopen_req ? tcp_send_syn_data(sk, buff) :
		      tcp_transmit_skb(sk, buff, 1, sk->sk_allocation);
		...
		...
	}
	EXPORT_SYMBOL(tcp_connect);
```

tp->fastopen_req会去快速创建子socket，这里以常规流程里为例，会进入tcp_transmit_skb方法，之后依次会调用__tcp_transmit_skb，ip_queue_xmit等等，最后调用网卡的驱动注册方法来把skb信息加到发送队列中，最后又物理网卡发送出去，可以参考[igb网卡](./igb网卡流程.txt)和[e1000网卡](./e1000网卡流程.txt)网卡发送数据中第14步的解析

我们来看tcp_init_nondata_skb方法

```
	static void tcp_init_nondata_skb(struct sk_buff *skb, u32 seq, u8 flags)
	{
		skb->ip_summed = CHECKSUM_PARTIAL;
		TCP_SKB_CB(skb)->tcp_flags = flags;
		tcp_skb_pcount_set(skb, 1);
		TCP_SKB_CB(skb)->seq = seq;
		if (flags & (TCPHDR_SYN | TCPHDR_FIN))
			seq++;
		TCP_SKB_CB(skb)->end_seq = seq;
	}
```

这里首先设置报文的头部标记是TCPHDR_SYN，然后设置数据段个数为1，最后维护一个seq的值（注意，对于SYN和FIN标记会对这个seq+1，这个值在服务器端ack的时候会进行校验）

最后我们看一下tcp_transmit_skb函数，这个函数会调用__tcp_transmit_skb(sk, skb, clone_it, gfp_mask, tcp_sk(sk)->rcv_nxt)

```
	static int __tcp_transmit_skb(struct sock *sk, struct sk_buff *skb,
			      int clone_it, gfp_t gfp_mask, u32 rcv_nxt)
	{
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct inet_sock *inet;
		struct tcp_sock *tp;
		struct tcp_skb_cb *tcb;
		struct tcp_out_options opts;
		unsigned int tcp_options_size, tcp_header_size;
		struct sk_buff *oskb = NULL;
		struct tcp_md5sig_key *md5;
		struct tcphdr *th;
	
		...
		...
		/* Build TCP header and checksum it. */
		th = (struct tcphdr *)skb->data;
		th->source		= inet->inet_sport;
		th->dest		= inet->inet_dport;
		th->seq			= htonl(tcb->seq);
		th->ack_seq		= htonl(rcv_nxt);
		*(((__be16 *)th) + 6)	= htons(((tcp_header_size >> 2) << 12) |
						tcb->tcp_flags);
	
		th->check		= 0;
		th->urg_ptr		= 0;
	
		...
		...
	}
```

注意*(((__be16 *)th) + 6)	= htons(((tcp_header_size >> 2) << 12) | tcb->tcp_flags)这段代码，我们看一下struct tcphdr的结构

```
	struct tcphdr {
	    __be16    source;     // 0-1 字节
	    __be16    dest;       // 2-3 字节
	    __be32    seq;        // 4-7 字节
	    __be32    ack_seq;    // 8-11 字节
	#if defined(__LITTLE_ENDIAN_BITFIELD)
	    __u16    res1:4,    // 位字段：低4位 (bit 0-3) - 保留
	             doff:4,    // 位字段：接下来4位 (bit 4-7) - Data Offset
	             fin:1,     // 位字段：bit 8
	             syn:1,     // 位字段：bit 9
	             rst:1,     // 位字段：bit 10
	             psh:1,     // 位字段：bit 11
	             ack:1,     // 位字段：bit 12
	             urg:1,     // 位字段：bit 13
	             ece:1,     // 位字段：bit 14
	             cwr:1;     // 位字段：bit 15
	#elif defined(__BIG_ENDIAN_BITFIELD)
	    __u16    doff:4,    // 位字段：高4位 (bit 12-15) - Data Offset
	             res1:4,    // 位字段：接下来4位 (bit 8-11) - 保留
	             cwr:1,     // 位字段：bit 7
	             ece:1,     // 位字段：bit 6
	             urg:1,     // 位字段：bit 5
	             ack:1,     // 位字段：bit 4
	             psh:1,     // 位字段：bit 3
	             rst:1,     // 位字段：bit 2
	             syn:1,     // 位字段：bit 1
	             fin:1;     // 位字段：bit 0
	#else
	#error    "Adjust your <asm/byteorder.h> defines"
	#endif
	    __be16    window;     // 14-15 字节 (从 TCP 头开始算)
	    __sum16    check;     // 16-17 字节
	    __be16    urg_ptr;    // 18-19 字节
	};
```

之前那段位运算的代码正确地填充tcphdr结构体中位于12-13字节处的16位字段，这个字段包含了Data Offset(doff)和所有TCP标志位，其中就包括将th->syn设置为1(这里的tcp_flags之前设置的是TCPHDR_SYN)。

__tcp_transmit_skb方法

以上为TCP建立的第一次握手，即由客户端发送SYN头到服务器端

## 服务器响应SYN（第二次握手，发送ACK）

这里服务器响应客户端请求是函数是tcp_v4_rcv，可以参考[igb网卡](./igb网卡流程.txt)和[e1000网卡](./e1000网卡流程.txt)中网卡接受消息流程的第24步。这里我们跳过前面的函数，直接看tcp_v4_rcv

```
	int tcp_v4_rcv(struct sk_buff *skb)
	{
		struct net *net = dev_net(skb->dev);
		enum skb_drop_reason drop_reason;
		int sdif = inet_sdif(skb);
		int dif = inet_iif(skb);
		const struct iphdr *iph;
		const struct tcphdr *th;
		bool refcounted;
		struct sock *sk;
		int ret;
	
		...
		...
		if (sk->sk_state == TCP_LISTEN) {
			ret = tcp_v4_do_rcv(sk, skb);
			goto put_and_return;
		}
		...
		...
	}
```

这里简单说明一下，在服务端代码执行listen的时候，调用了

```
	int inet_listen(struct socket *sock, int backlog)
	{
		struct sock *sk = sock->sk;
		unsigned char old_state;
		int err, tcp_fastopen;
	
		lock_sock(sk);
	
		...
		...
		if (old_state != TCP_LISTEN) {
			...
			...
			//这一行回去设置服务器端上sk->sk_state
			err = inet_csk_listen_start(sk);
		}
		...
		...
	}
	EXPORT_SYMBOL(inet_listen);
```

这里会在inet_csk_listen_start方法中修改当前sk->sk_state的状态为TCP_LISTEN

```
	int inet_csk_listen_start(struct sock *sk)
	{
		...
		...
		inet_sk_state_store(sk, TCP_LISTEN);
		...
	}
	EXPORT_SYMBOL_GPL(inet_csk_listen_start);
```

所以，判断sk->sk_state == TCP_LISTEN之后，会进入tcp_v4_do_rcv函数，继续看tcp_v4_do_rcv函数

```
	int tcp_v4_do_rcv(struct sock *sk, struct sk_buff *skb)
	{
		enum skb_drop_reason reason;
		struct sock *rsk;
	
		...
		...
		if (sk->sk_state == TCP_LISTEN) {
			struct sock *nsk = tcp_v4_cookie_check(sk, skb);
	
			if (!nsk)
				goto discard;
			if (nsk != sk) {
				if (tcp_child_process(sk, nsk, skb)) {
					rsk = nsk;
					goto reset;
				}
				return 0;
			}
		} else
			sock_rps_save_rxhash(sk, skb);
		if (tcp_rcv_state_process(sk, skb)) {
			rsk = sk;
			goto reset;
		}
		return 0;
		...
	}
	EXPORT_SYMBOL(tcp_v4_do_rcv);
```

```
	static struct sock *tcp_v4_cookie_check(struct sock *sk, struct sk_buff *skb)
	{
	#ifdef CONFIG_SYN_COOKIES
		const struct tcphdr *th = tcp_hdr(skb);
	
		if (!th->syn)
			sk = cookie_v4_check(sk, skb);
	#endif
		return sk;
	}
```
注意，在服务器第二次握手的时候，tcp_v4_cookie_check中会判断th->syn是否是1，第二次握手的时候，客户端在第一次握手时发送的syn确实是1，所以这里不会走到cookie_v4_check方法，该方法其实是第三次握手后，服务端进行的响应操作，这里我们直接看后面的tcp_rcv_state_process函数

```
	int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb)
	{
		struct tcp_sock *tp = tcp_sk(sk);
		struct inet_connection_sock *icsk = inet_csk(sk);
		const struct tcphdr *th = tcp_hdr(skb);
		struct request_sock *req;
		int queued = 0;
		bool acceptable;
		SKB_DR(reason);
	
		switch (sk->sk_state) {
		case TCP_CLOSE:
			SKB_DR_SET(reason, TCP_CLOSE);
			goto discard;
	
		case TCP_LISTEN:
			if (th->ack)
				return 1;
	
			if (th->rst) {
				SKB_DR_SET(reason, TCP_RESET);
				goto discard;
			}
			if (th->syn) {
				if (th->fin) {
					SKB_DR_SET(reason, TCP_FLAGS);
					goto discard;
				}
				/* It is possible that we process SYN packets from backlog,
				 * so we need to make sure to disable BH and RCU right there.
				 */
				rcu_read_lock();
				local_bh_disable();
				acceptable = icsk->icsk_af_ops->conn_request(sk, skb) >= 0;
				local_bh_enable();
				rcu_read_unlock();
	
				if (!acceptable)
					return 1;
				consume_skb(skb);
				return 0;
			}
			SKB_DR_SET(reason, TCP_FLAGS);
			goto discard;
	
		case TCP_SYN_SENT:
			tp->rx_opt.saw_tstamp = 0;
			tcp_mstamp_refresh(tp);
			queued = tcp_rcv_synsent_state_process(sk, skb, th);
			if (queued >= 0)
				return queued;
	
			/* Do step6 onward by hand. */
			tcp_urg(sk, skb, th);
			__kfree_skb(skb);
			tcp_data_snd_check(sk);
			return 0;
		}
	
		...
		...
	
		/* step 5: check the ACK field */
		acceptable = tcp_ack(sk, skb, FLAG_SLOWPATH |
					      FLAG_UPDATE_TS_RECENT |
					      FLAG_NO_CHALLENGE_ACK) > 0;
	
		if (!acceptable) {
			if (sk->sk_state == TCP_SYN_RECV)
				return 1;	/* send one RST */
			tcp_send_challenge_ack(sk);
			SKB_DR_SET(reason, TCP_OLD_ACK);
			goto discard;
		}
		switch (sk->sk_state) {
		case TCP_SYN_RECV:
			tp->delivered++; /* SYN-ACK delivery isn't tracked in tcp_ack */
			...
			...
			break;
	
		case TCP_FIN_WAIT1: {
			...
			...
		}
	
		case TCP_CLOSING:
			...
			...
			break;
	
		case TCP_LAST_ACK:
			...
			...
			break;
		}
	
		/* step 6: check the URG bit */
		tcp_urg(sk, skb, th);
	
		/* step 7: process the segment text */
		switch (sk->sk_state) {
		case TCP_CLOSE_WAIT:
		case TCP_CLOSING:
		case TCP_LAST_ACK:
			if (!before(TCP_SKB_CB(skb)->seq, tp->rcv_nxt)) {
				/* If a subflow has been reset, the packet should not
				 * continue to be processed, drop the packet.
				 */
				if (sk_is_mptcp(sk) && !mptcp_incoming_options(sk, skb))
					goto discard;
				break;
			}
			fallthrough;
		case TCP_FIN_WAIT1:
		case TCP_FIN_WAIT2:
			/* RFC 793 says to queue data in these states,
			 * RFC 1122 says we MUST send a reset.
			 * BSD 4.4 also does reset.
			 */
			if (sk->sk_shutdown & RCV_SHUTDOWN) {
				if (TCP_SKB_CB(skb)->end_seq != TCP_SKB_CB(skb)->seq &&
				    after(TCP_SKB_CB(skb)->end_seq - th->fin, tp->rcv_nxt)) {
					NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPABORTONDATA);
					tcp_reset(sk, skb);
					return 1;
				}
			}
			fallthrough;
		case TCP_ESTABLISHED:
			tcp_data_queue(sk, skb);
			queued = 1;
			break;
		}
	
		/* tcp_data could move socket to TIME-WAIT */
		if (sk->sk_state != TCP_CLOSE) {
			tcp_data_snd_check(sk);
			tcp_ack_snd_check(sk);
		}
	
		if (!queued) {
	discard:
			tcp_drop_reason(sk, skb, reason);
		}
		return 0;
	
	consume:
		__kfree_skb(skb);
		return 0;
	}
	EXPORT_SYMBOL(tcp_rcv_state_process);
```
这里服务器的sk->sk_state == TCP_LISTEN，另外根据接收到的信息skb（客户端第一次握手发送的请求数据包，在客户端发送前设置过th->syn = 1），因此继续看icsk->icsk_af_ops->conn_request(sk, skb)这行。这里在/net/ipv4/tcp_ipv4.c中已经设置过一个钩子函数:

```
	const struct inet_connection_sock_af_ops ipv4_specific = {
		.queue_xmit	   = ip_queue_xmit,
		.send_check	   = tcp_v4_send_check,
		.rebuild_header	   = inet_sk_rebuild_header,
		.sk_rx_dst_set	   = inet_sk_rx_dst_set,
		.conn_request	   = tcp_v4_conn_request,
		.syn_recv_sock	   = tcp_v4_syn_recv_sock,
		.net_header_len	   = sizeof(struct iphdr),
		.setsockopt	   	   = ip_setsockopt,
		.getsockopt	       = ip_getsockopt,
		.addr2sockaddr	   = inet_csk_addr2sockaddr,
		.sockaddr_len	   = sizeof(struct sockaddr_in),
		.mtu_reduced	   = tcp_v4_mtu_reduced,
	};
```

这里实际调用的是tcp_v4_conn_request函数，我们继续深入：

```
	struct request_sock_ops tcp_request_sock_ops __read_mostly = {
		.family		=	PF_INET,
		.obj_size	=	sizeof(struct tcp_request_sock),
		.rtx_syn_ack	=	tcp_rtx_synack,
		.send_ack	=	tcp_v4_reqsk_send_ack,
		.destructor	=	tcp_v4_reqsk_destructor,
		.send_reset	=	tcp_v4_send_reset,
		.syn_ack_timeout =	tcp_syn_ack_timeout,
	};
	
	const struct tcp_request_sock_ops tcp_request_sock_ipv4_ops = {
		.mss_clamp	=	TCP_MSS_DEFAULT,
	#ifdef CONFIG_TCP_MD5SIG
		.req_md5_lookup	=	tcp_v4_md5_lookup,
		.calc_md5_hash	=	tcp_v4_md5_hash_skb,
	#endif
	#ifdef CONFIG_SYN_COOKIES
		.cookie_init_seq =	cookie_v4_init_sequence,
	#endif
		.route_req	=	tcp_v4_route_req,
		.init_seq	=	tcp_v4_init_seq,
		.init_ts_off	=	tcp_v4_init_ts_off,
		.send_synack	=	tcp_v4_send_synack,
	};
	
	int tcp_v4_conn_request(struct sock *sk, struct sk_buff *skb)
	{
		/* Never answer to SYNs send to broadcast or multicast */
		if (skb_rtable(skb)->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST))
			goto drop;
	
		return tcp_conn_request(&tcp_request_sock_ops,
					&tcp_request_sock_ipv4_ops, sk, skb);
	
	drop:
		tcp_listendrop(sk);
		return 0;
	}
	EXPORT_SYMBOL(tcp_v4_conn_request);
```

这里将tcp_request_sock_ops和tcp_request_sock_ipv4_ops两个钩子配置传入tcp_conn_request函数，之后服务器会调用send_synack来应答客户端第一次握手的请求：

```
	int tcp_conn_request(struct request_sock_ops *rsk_ops,
		     const struct tcp_request_sock_ops *af_ops,
		     struct sock *sk, struct sk_buff *skb)
	{
		struct tcp_fastopen_cookie foc = { .len = -1 };
		__u32 isn = TCP_SKB_CB(skb)->tcp_tw_isn;
		struct tcp_options_received tmp_opt;
		struct tcp_sock *tp = tcp_sk(sk);
		struct net *net = sock_net(sk);
		struct sock *fastopen_sk = NULL;
		struct request_sock *req;
		...
		...
		req = inet_reqsk_alloc(rsk_ops, sk, !want_cookie);
		...
		...
		if (fastopen_sk) {
			af_ops->send_synack(fastopen_sk, dst, &fl, req,
					    &foc, TCP_SYNACK_FASTOPEN, skb);
			...
			...
		} else {
			tcp_rsk(req)->tfo_listener = false;
			if (!want_cookie) {
				req->timeout = tcp_timeout_init((struct sock *)req);
				inet_csk_reqsk_queue_hash_add(sk, req, req->timeout);
			}
			af_ops->send_synack(sk, dst, &fl, req, &foc,
					    !want_cookie ? TCP_SYNACK_NORMAL :
							   TCP_SYNACK_COOKIE,
					    skb);
			if (want_cookie) {
				reqsk_free(req);
				return 0;
			}
		}
		...
		...
	} EXPORT_SYMBOL(tcp_conn_request);
```

先看req = inet_reqsk_alloc(rsk_ops, sk, !want_cookie);

```
	struct request_sock *inet_reqsk_alloc(const struct request_sock_ops *ops,
				      struct sock *sk_listener,
				      bool attach_listener)
	{
		struct request_sock *req = reqsk_alloc(ops, sk_listener,
						       attach_listener);
	
		if (req) {
			struct inet_request_sock *ireq = inet_rsk(req);
	
			ireq->ireq_opt = NULL;
	#if IS_ENABLED(CONFIG_IPV6)
			ireq->pktopts = NULL;
	#endif
			atomic64_set(&ireq->ir_cookie, 0);
			ireq->ireq_state = TCP_NEW_SYN_RECV;
			write_pnet(&ireq->ireq_net, sock_net(sk_listener));
			ireq->ireq_family = sk_listener->sk_family;
			req->timeout = TCP_TIMEOUT_INIT;
		}
	
		return req;
	}
	EXPORT_SYMBOL(inet_reqsk_alloc);
```

这里是新建一个半连接request_sock，将半连接的状态设置为TCP_NEW_SYN_RECV，表示刚刚收到 SYN（第一次握手之后分配的 req）

我们再看inet_csk_reqsk_queue_hash_add(sk, req, req->timeout);

```
	void inet_csk_reqsk_queue_hash_add(struct sock *sk, struct request_sock *req,
				   unsigned long timeout)
	{
		reqsk_queue_hash_req(req, timeout);
		inet_csk_reqsk_queue_added(sk);
	}
	EXPORT_SYMBOL_GPL(inet_csk_reqsk_queue_hash_add);
	
		
	//将半连接的socket（request_sock）加入到accept的队列中	
	static inline void inet_csk_reqsk_queue_added(struct sock *sk)
	{
		reqsk_queue_added(&inet_csk(sk)->icsk_accept_queue);
	}
	
	static u32 sk_ehashfn(const struct sock *sk)
	{
	#if IS_ENABLED(CONFIG_IPV6)
		if (sk->sk_family == AF_INET6 &&
		    !ipv6_addr_v4mapped(&sk->sk_v6_daddr))
			return inet6_ehashfn(sock_net(sk),
					     &sk->sk_v6_rcv_saddr, sk->sk_num,
					     &sk->sk_v6_daddr, sk->sk_dport);
	#endif
		return inet_ehashfn(sock_net(sk),
				    sk->sk_rcv_saddr, sk->sk_num,
				    sk->sk_daddr, sk->sk_dport);
	}

	bool inet_ehash_insert(struct sock *sk, struct sock *osk, bool *found_dup_sk)
	{
		//用于从给定的 sock（struct sock *sk）获取TCP使用的连接哈希表信息，即返回指向 struct inet_hashinfo的指针
		struct inet_hashinfo *hashinfo = tcp_or_dccp_get_hashinfo(sk);
		struct inet_ehash_bucket *head;
		struct hlist_nulls_head *list;
		spinlock_t *lock;
		bool ret = true;
	
		WARN_ON_ONCE(!sk_unhashed(sk));
	
		sk->sk_hash = sk_ehashfn(sk);
		head = inet_ehash_bucket(hashinfo, sk->sk_hash);
		list = &head->chain;
		lock = inet_ehash_lockp(hashinfo, sk->sk_hash);
	
		spin_lock(lock);
		if (osk) {
			...
			...
			goto unlock;
		}
		if (found_dup_sk) {
			...
			...
		}
	
		if (ret)
			__sk_nulls_add_node_rcu(sk, list);
	
	unlock:
		spin_unlock(lock);
		return ret;
	}
	
	
	//这个函数主要是设置一个定时器，超过时间的半连接都会被丢弃；另外就是会把半连接socket插入到TCP的established hash表中，方便后面第三次握手时服务端去查找这个半链接socket
	static void reqsk_queue_hash_req(struct request_sock *req,
				 unsigned long timeout)
	{
		timer_setup(&req->rsk_timer, reqsk_timer_handler, TIMER_PINNED);
		mod_timer(&req->rsk_timer, jiffies + timeout);
	
		inet_ehash_insert(req_to_sk(req), NULL, NULL);
		/* before letting lookups find us, make sure all req fields
		 * are committed to memory and refcnt initialized.
		 */
		smp_wmb();
		refcount_set(&req->rsk_refcnt, 2 + 1);
	}
```

上面的几段代码作用是为半连接的socket设置一个计时器，超过规定时间的半连接都将被丢弃；另外就是为每个半连接生成一个hash值，并将该hash值插入到半连接队列中（icsk_accept_queue）去，方便第三次握手后服务端快速从该队列中找到此半连接并建立完全连接。
这里我们关注sk_ehashfn这个函数，可以看到内核是如何为一个半连接socket生成hash值的，sk->sk_rcv_saddr（源地址）、sk->sk_num（源端口）、sk->sk_daddr（目标地址）、sk->sk_dport（目标端口）。可以看到内核是利用四元组来划分一个有效连接的。

我们返过来再看tcp_conn_request函数，这里fastopen指的是 TCP Fast Open(TFO)，传统的三次握手连接是只有在客户端发送了最终的ACK之后，应用程序数据才能开始传输。通过TFO，后续连接中，客户端可以在发送SYN的同时发送应用数据；服务端可以在发送SYN-ACK的同时发送应用数据。这种情况我们暂不考虑，只看传统的三次握手协议，因此，调用af_ops->send_synack，钩子对应的方法是tcp_v4_send_synack。

```
	static int tcp_v4_send_synack(const struct sock *sk, struct dst_entry *dst,
			      struct flowi *fl,
			      struct request_sock *req,
			      struct tcp_fastopen_cookie *foc,
			      enum tcp_synack_type synack_type,
			      struct sk_buff *syn_skb)
	{
		const struct inet_request_sock *ireq = inet_rsk(req);
		struct flowi4 fl4;
		int err = -1;
		struct sk_buff *skb;
		u8 tos;
	
		...
		...
	
		skb = tcp_make_synack(sk, dst, req, foc, synack_type, syn_skb);
	
		if (skb) {
			__tcp_v4_send_check(skb, ireq->ir_loc_addr, ireq->ir_rmt_addr);
			...
			...	
			err = ip_build_and_send_pkt(skb, sk, ireq->ir_loc_addr,
						    ireq->ir_rmt_addr,
						    rcu_dereference(ireq->ireq_opt),
						    tos);
			...
			...
		}
	
		return err;
	}
```

直接看tcp_make_synack函数，这里会去构件第一次握手服务端的SYN_ACK头信息：

```
	struct sk_buff *tcp_make_synack(const struct sock *sk, struct dst_entry *dst,
				struct request_sock *req,
				struct tcp_fastopen_cookie *foc,
				enum tcp_synack_type synack_type,
				struct sk_buff *syn_skb)
	{
		struct inet_request_sock *ireq = inet_rsk(req);
		const struct tcp_sock *tp = tcp_sk(sk);
		struct tcp_md5sig_key *md5 = NULL;
		struct tcp_out_options opts;
		struct sk_buff *skb;
		int tcp_header_size;
		struct tcphdr *th;
		...
		TCP_SKB_CB(skb)->tcp_flags = TCPHDR_SYN | TCPHDR_ACK;
		...
		skb_reset_transport_header(skb);
	
		th = (struct tcphdr *)skb->data;
		memset(th, 0, sizeof(struct tcphdr));
		th->syn = 1;
		th->ack = 1;
		tcp_ecn_make_synack(req, th);
		th->source = htons(ireq->ir_num);
		th->dest = ireq->ir_rmt_port;
		skb->mark = ireq->ir_mark;
		skb->ip_summed = CHECKSUM_PARTIAL;
		th->seq = htonl(tcp_rsk(req)->snt_isn);
		/* XXX data is queued and acked as is. No buffer/window check */
		th->ack_seq = htonl(tcp_rsk(req)->rcv_nxt);
	
		...
		...
		return skb;
	}
	EXPORT_SYMBOL(tcp_make_synack);
		
```

这里服务端会将tcp_flags设置成TCPHDR_SYN | TCPHDR_ACK，struct tcphdr的syn和ack设置成1，作为对客户端SYN的回应；同时服务端会生成一个新的序列号（th->seq），同时服务端也会在客户端发送的seq的基础上+1作为校验回应（之前提到过客户端的seq会在服务端做校验），赋值给th->ack_seq。

在以上相应头构造完毕后，服务端会调用ip_build_and_send_pkt将SYN-ACK的相应发送给客户端。这里会调用ip_local_out直到最后writel(i, tx_ring->tail)写入发送队列，交由物理网卡发送。（可以参考[igb网卡](./igb网卡流程.txt)和[e1000网卡](./e1000网卡流程.txt)中网卡发送消息流程的第17步）

以上为TCP建立的第二次握手，即由服务端发送SYN-ACK头到客户端，客户端进行接收。


## 客户端响应SYN-ACK（第三次握手，发送ACK）

这里客户端响应服务端的SYN-ACK请求的函数也是tcp_v4_rcv，可以参考[igb网卡](./igb网卡流程.txt)和[e1000网卡](./e1000网卡流程.txt)中网卡接受消息流程的第24步。

```
	int tcp_v4_rcv(struct sk_buff *skb)
	{
		struct net *net = dev_net(skb->dev);
		enum skb_drop_reason drop_reason;
		int sdif = inet_sdif(skb);
		int dif = inet_iif(skb);
		const struct iphdr *iph;
		const struct tcphdr *th;
		bool refcounted;
		struct sock *sk;
		int ret;
	
		...
		...
		if (!sock_owned_by_user(sk)) {
			ret = tcp_v4_do_rcv(sk, skb);
		} else {
			if (tcp_add_backlog(sk, skb, &drop_reason))
				goto discard_and_relse;
		}
		...
		...
	}
```

这里同样会进入tcp_v4_do_rcv方法和tcp_rcv_state_process，我们继续：

```
	int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb)
	{
		struct tcp_sock *tp = tcp_sk(sk);
		struct inet_connection_sock *icsk = inet_csk(sk);
		const struct tcphdr *th = tcp_hdr(skb);
		struct request_sock *req;
		int queued = 0;
		bool acceptable;
		SKB_DR(reason);
	
		switch (sk->sk_state) {
		case TCP_CLOSE:
			...
			...
	
		case TCP_LISTEN:
			...
			...
	
		case TCP_SYN_SENT:
			tp->rx_opt.saw_tstamp = 0;
			tcp_mstamp_refresh(tp);
			queued = tcp_rcv_synsent_state_process(sk, skb, th);
			if (queued >= 0)
				return queued;
	
			/* Do step6 onward by hand. */
			tcp_urg(sk, skb, th);
			__kfree_skb(skb);
			tcp_data_snd_check(sk);
			return 0;
		}
		...
		...
	}
	EXPORT_SYMBOL(tcp_rcv_state_process);
```

这里客户端因为之前在发送第一次握手请求的时候在tcp_v4_connect方法中把sk->st_state设置成了TCP_SYN_SENT，所以这里客户端会直接调用tcp_rcv_synsent_state_process方法：

```
	static int tcp_rcv_synsent_state_process(struct sock *sk, struct sk_buff *skb,
					 const struct tcphdr *th)
	{
		struct inet_connection_sock *icsk = inet_csk(sk);
		struct tcp_sock *tp = tcp_sk(sk);
		struct tcp_fastopen_cookie foc = { .len = -1 };
		int saved_clamp = tp->rx_opt.mss_clamp;
		bool fastopen_fail;
		...
		...
		if (th->ack) {			//服务端第二次握手时已将ack设置成1
			if (!after(TCP_SKB_CB(skb)->ack_seq, tp->snd_una) ||
			    after(TCP_SKB_CB(skb)->ack_seq, tp->snd_nxt)) {
				/* Previous FIN/ACK or RST/ACK might be ignored. */
				if (icsk->icsk_retransmits == 0)
					inet_csk_reset_xmit_timer(sk,
							ICSK_TIME_RETRANS,
							TCP_TIMEOUT_MIN, TCP_RTO_MAX);
				goto reset_and_undo;
			}
	
			//如果收到RST包，表示服务器拒绝连接
			if (th->rst) {
				tcp_reset(sk, skb);
		
			...
			...
			tcp_ecn_rcv_synack(tp, th);
	
			tcp_init_wl(tp, TCP_SKB_CB(skb)->seq);
			tcp_try_undo_spurious_syn(sk);
			tcp_ack(sk, skb, FLAG_SLOWPATH);
	
			...
			...
			//将sk->sk_state设置成ESTABLISHED
			tcp_finish_connect(sk, skb);
			... 
			...
			tcp_send_ack(sk);
			return -1;
		}
		...
		...
		if (th->syn) {
			//这里服务端把sk->sk_state设置成TCP_SYN_RECV
			tcp_set_state(sk, TCP_SYN_RECV);
	
			
		}
	}
```

这里服务端第二次握手时已经将th->ack设置成1，所以会进入这个if判断中，服务端把sk->sk_state设置成TCP_SYN_RECV，然后再看tcp_finish_connect方法:

```
	void tcp_finish_connect(struct sock *sk, struct sk_buff *skb)
	{
		struct tcp_sock *tp = tcp_sk(sk);
		struct inet_connection_sock *icsk = inet_csk(sk);
	
		tcp_set_state(sk, TCP_ESTABLISHED);
	
		...
		...
		//长连接配置
		if (sock_flag(sk, SOCK_KEEPOPEN))
			inet_csk_reset_keepalive_timer(sk, keepalive_time_when(tp));
		...
		...
	}
```

这里会把客户端的sk->sk_state设置成TCP_ESTABLISHED，表示客户端已经完成tcp连接了。最后调用tcp_send_ack（里面会调用__tcp_send_ack）发送ACK请求到服务端。

```
	void __tcp_send_ack(struct sock *sk, u32 rcv_nxt)
	{
		struct sk_buff *buff;
	
		/* If we have been reset, we may not send again. */
		if (sk->sk_state == TCP_CLOSE)
			return;
		...
		...
		/* Reserve space for headers and prepare control bits. */
		skb_reserve(buff, MAX_TCP_HEADER);
		tcp_init_nondata_skb(buff, tcp_acceptable_seq(sk), TCPHDR_ACK);
	
		...
		...
		/* Send it off, this clears delayed acks for us. */
		__tcp_transmit_skb(sk, buff, 0, (__force gfp_t)0, rcv_nxt);
	}
	EXPORT_SYMBOL_GPL(__tcp_send_ack);
	
	static void tcp_init_nondata_skb(struct sk_buff *skb, u32 seq, u8 flags)
	{
		skb->ip_summed = CHECKSUM_PARTIAL;
	
		TCP_SKB_CB(skb)->tcp_flags = flags;
	
		tcp_skb_pcount_set(skb, 1);
	
		TCP_SKB_CB(skb)->seq = seq;
		if (flags & (TCPHDR_SYN | TCPHDR_FIN))
			seq++;
		TCP_SKB_CB(skb)->end_seq = seq;
	}
```

这里会调用tcp_init_nondata_skb方法去设置报文的头部标记是TCPHDR_ACK，然后设置数据段个数为1，最后将服务端第二次握手返回的seq保存在seq和end_seq中，最后调用__tcp_transmit_skb方法构建头部信息，随后发送给服务端。

```
	static int __tcp_transmit_skb(struct sock *sk, struct sk_buff *skb,
			      int clone_it, gfp_t gfp_mask, u32 rcv_nxt)
	{
		const struct inet_connection_sock *icsk = inet_csk(sk);
		struct inet_sock *inet;
		struct tcp_sock *tp;
		struct tcp_skb_cb *tcb;
		struct tcp_out_options opts;
		unsigned int tcp_options_size, tcp_header_size;
		struct sk_buff *oskb = NULL;
		struct tcp_md5sig_key *md5;
		struct tcphdr *th;
	
		...
		...
		/* Build TCP header and checksum it. */
		th = (struct tcphdr *)skb->data;
		th->source		= inet->inet_sport;
		th->dest		= inet->inet_dport;
		th->seq			= htonl(tcb->seq);
		th->ack_seq		= htonl(rcv_nxt);
		*(((__be16 *)th) + 6)	= htons(((tcp_header_size >> 2) << 12) |
						tcb->tcp_flags);
	
		th->check		= 0;
		th->urg_ptr		= 0;
	
		...
		...
	}
```

这里看下这行*(((__be16 *)th) + 6)	= htons(((tcp_header_size >> 2) << 12) | tcb->tcp_flags); 简单的说，这里tcp_flags之前传的是TCPHDR_ACK，也就是会把ack设成1，其余的包括syn的设成0。
这里我们可以看到第三次握手的时候，客户端会把tcp_init_nondata_skb方法中的seq赋值给th->seq，并将seq+1再赋值给th->ack_seq。

在以上相应头构造完毕后，客户端之后依次会调用__tcp_transmit_skb，ip_queue_xmit等等，最后调用网卡的驱动注册方法来把skb信息加到发送队列中，最后由物理网卡发送出去，可以参考[igb网卡](./igb网卡流程.txt)和[e1000网卡](./e1000网卡流程.txt)网卡发送数据中第14步的解析

到这里，客户端的前期准备已经全部完成了，最后是等待服务端对第三次握手的响应。


## 服务端响应ACK（第三次握手，服务端的处理）

和第二次握手服务端的处理一样，这里服务器响应客户端请求是函数是tcp_v4_rcv，可以参考[igb网卡](./igb网卡流程.txt)和[e1000网卡](./e1000网卡流程.txt)中网卡接受消息流程的第24步。这里我们跳过前面的函数，直接看tcp_v4_rcv：

```
	int tcp_v4_rcv(struct sk_buff *skb)
	{
		struct net *net = dev_net(skb->dev);
		enum skb_drop_reason drop_reason;
		int sdif = inet_sdif(skb);
		int dif = inet_iif(skb);
		const struct iphdr *iph;
		const struct tcphdr *th;
		bool refcounted;
		struct sock *sk;
		int ret;
	
		...
		...
		th = (const struct tcphdr *)skb->data;
		iph = ip_hdr(skb);
	lookup:
		sk = __inet_lookup_skb(net->ipv4.tcp_death_row.hashinfo,
				       skb, __tcp_hdrlen(th), th->source,
				       th->dest, sdif, &refcounted);
		if (!sk)
			goto no_tcp_socket;
	
	process:
		if (sk->sk_state == TCP_NEW_SYN_RECV) {
			struct request_sock *req = inet_reqsk(sk);
			bool req_stolen = false;
			struct sock *nsk;
	
			sk = req->rsk_listener;
			...
			...
			if (!tcp_filter(sk, skb)) {
				th = (const struct tcphdr *)skb->data;
				iph = ip_hdr(skb);
				tcp_v4_fill_cb(skb, iph, th);
				nsk = tcp_check_req(sk, skb, req, false, &req_stolen);
			} else {
				drop_reason = SKB_DROP_REASON_SOCKET_FILTER;
			}
			...
			...
		}
		...
		...
		if (!sock_owned_by_user(sk)) {
			ret = tcp_v4_do_rcv(sk, skb);
		} else {
			if (tcp_add_backlog(sk, skb, &drop_reason))
				goto discard_and_relse;
		}
		...
		...
	}
```

```
	static inline struct sock *__inet_lookup_skb(struct inet_hashinfo *hashinfo,
					     struct sk_buff *skb,
					     int doff,
					     const __be16 sport,
					     const __be16 dport,
					     const int sdif,
					     bool *refcounted)
	{
		struct sock *sk = skb_steal_sock(skb, refcounted);
		const struct iphdr *iph = ip_hdr(skb);
	
		if (sk)
			return sk;
	
		return __inet_lookup(dev_net(skb_dst(skb)->dev), hashinfo, skb,
				     doff, iph->saddr, sport,
				     iph->daddr, dport, inet_iif(skb), sdif,
				     refcounted);
	}
```
	
```
	static inline struct sock *__inet_lookup(struct net *net,
					 struct inet_hashinfo *hashinfo,
					 struct sk_buff *skb, int doff,
					 const __be32 saddr, const __be16 sport,
					 const __be32 daddr, const __be16 dport,
					 const int dif, const int sdif,
					 bool *refcounted)
	{
		u16 hnum = ntohs(dport);
		struct sock *sk;
	
		sk = __inet_lookup_established(net, hashinfo, saddr, sport,
					       daddr, hnum, dif, sdif);
		*refcounted = true;
		if (sk)
			return sk;
		*refcounted = false;
		return __inet_lookup_listener(net, hashinfo, skb, doff, saddr,
					      sport, daddr, hnum, dif, sdif);
	}
```
	
```	
	struct sock *__inet_lookup_established(struct net *net,
				  struct inet_hashinfo *hashinfo,
				  const __be32 saddr, const __be16 sport,
				  const __be32 daddr, const u16 hnum,
				  const int dif, const int sdif)
	{
		INET_ADDR_COOKIE(acookie, saddr, daddr);
		const __portpair ports = INET_COMBINED_PORTS(sport, hnum);
		struct sock *sk;
		const struct hlist_nulls_node *node;
		unsigned int hash = inet_ehashfn(net, daddr, hnum, saddr, sport);
		unsigned int slot = hash & hashinfo->ehash_mask;
		struct inet_ehash_bucket *head = &hashinfo->ehash[slot];
	
	begin:
		sk_nulls_for_each_rcu(sk, node, &head->chain) {
			if (sk->sk_hash != hash)
				continue;
			if (likely(inet_match(net, sk, acookie, ports, dif, sdif))) {
				if (unlikely(!refcount_inc_not_zero(&sk->sk_refcnt)))
					goto out;
				if (unlikely(!inet_match(net, sk, acookie,
							 ports, dif, sdif))) {
					sock_gen_put(sk);
					goto begin;
				}
				goto found;
			}
		}

		if (get_nulls_value(node) != slot)
			goto begin;
	out:
		sk = NULL;
	found:
		return sk;
	}
	EXPORT_SYMBOL_GPL(__inet_lookup_established);
```

我们先看__inet_lookup_skb这个函数，它会去调用__inet_lookup针对当前socket的hash值去两个队列中进行一个查找，首先查找的是已建立的连接，其次查找监听套接字，我们之前第一次握手的时候服务端已经将一个半连接socket加入了半连接队列（established），所以在__inet_lookup_established函数中我们可以查找到该半连接队列。注意inet_ehashfn(net, daddr, hnum, saddr, sport)这行代码，他依旧是使用四元组（目标地址、目标端口、源地址、源端口）去生成一个hash值，随后通过sk_nulls_for_each_rcu遍历查找，找到后直接返回这个有效的半连接socket。

新建的全连接sk->sk_state == TCP_NEW_SYN_RECV，所以接下来就会在tcp_check_req这个函数中去处理那个拿到的半连接socket：

```
	struct sock *tcp_check_req(struct sock *sk, struct sk_buff *skb,
			   struct request_sock *req,
			   bool fastopen, bool *req_stolen)
	{
		struct tcp_options_received tmp_opt;
		struct sock *child;
		const struct tcphdr *th = tcp_hdr(skb);
		__be32 flg = tcp_flag_word(th) & (TCP_FLAG_RST|TCP_FLAG_SYN|TCP_FLAG_ACK);
		bool paws_reject = false;
		bool own_req;
	
		...
		...
		/* OK, ACK is valid, create big socket and
		 * feed this segment to it. It will repeat all
		 * the tests. THIS SEGMENT MUST MOVE SOCKET TO
		 * ESTABLISHED STATE. If it will be dropped after
		 * socket is created, wait for troubles.
		 */
		child = inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, NULL,
								 req, &own_req);
		...
		...
	EXPORT_SYMBOL(tcp_check_req);
```

直接看inet_csk(sk)->icsk_af_ops->syn_recv_sock这行，这里根据/ne/ipv4/tcp_ipv4.c中ipv4_specific设置的钩子函数，syn_recv_sock实际调用的是tcp_v4_syn_recv_sock函数：

```
	struct sock *tcp_v4_syn_recv_sock(const struct sock *sk, struct sk_buff *skb,
				  struct request_sock *req,
				  struct dst_entry *dst,
				  struct request_sock *req_unhash,
				  bool *own_req)
	{
		struct inet_request_sock *ireq;
		bool found_dup_sk = false;
		struct inet_sock *newinet;
		struct tcp_sock *newtp;
		struct sock *newsk;
	#ifdef CONFIG_TCP_MD5SIG
		const union tcp_md5_addr *addr;
		struct tcp_md5sig_key *key;
		int l3index;
	#endif
		struct ip_options_rcu *inet_opt;
	
		if (sk_acceptq_is_full(sk))
			goto exit_overflow;
	
		//服务端为在这个方法中根据找到的半连接，新建一个新的全连接，并把它的sk_state设置成TCP_SYN_RCV
		newsk = tcp_create_openreq_child(sk, req, skb);
		if (!newsk)
			goto exit_nonewsk;
	
		newsk->sk_gso_type = SKB_GSO_TCPV4;
		inet_sk_rx_dst_set(newsk, skb);
	
		newtp		      = tcp_sk(newsk);
		newinet		      = inet_sk(newsk);
		ireq		      = inet_rsk(req);
		sk_daddr_set(newsk, ireq->ir_rmt_addr);
		sk_rcv_saddr_set(newsk, ireq->ir_loc_addr);
		newsk->sk_bound_dev_if = ireq->ir_iif;
		newinet->inet_saddr   = ireq->ir_loc_addr;
		inet_opt	      = rcu_dereference(ireq->ireq_opt);
		RCU_INIT_POINTER(newinet->inet_opt, inet_opt);
		newinet->mc_index     = inet_iif(skb);
		newinet->mc_ttl	      = ip_hdr(skb)->ttl;
		newinet->rcv_tos      = ip_hdr(skb)->tos;
		inet_csk(newsk)->icsk_ext_hdr_len = 0;
		if (inet_opt)
			inet_csk(newsk)->icsk_ext_hdr_len = inet_opt->opt.optlen;
		newinet->inet_id = get_random_u16();
	
		/* Set ToS of the new socket based upon the value of incoming SYN.
		 * ECT bits are set later in tcp_init_transfer().
		 */
		if (READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_reflect_tos))
			newinet->tos = tcp_rsk(req)->syn_tos & ~INET_ECN_MASK;
	
		if (!dst) {
			dst = inet_csk_route_child_sock(sk, newsk, req);
			if (!dst)
				goto put_and_exit;
		} else {
			/* syncookie case : see end of cookie_v4_check() */
		}
		sk_setup_caps(newsk, dst);
	
		tcp_ca_openreq_child(newsk, dst);
	
		tcp_sync_mss(newsk, dst_mtu(dst));
		newtp->advmss = tcp_mss_clamp(tcp_sk(sk), dst_metric_advmss(dst));
	
		tcp_initialize_rcv_mss(newsk);
	
	#ifdef CONFIG_TCP_MD5SIG
		l3index = l3mdev_master_ifindex_by_index(sock_net(sk), ireq->ir_iif);
		/* Copy over the MD5 key from the original socket */
		addr = (union tcp_md5_addr *)&newinet->inet_daddr;
		key = tcp_md5_do_lookup(sk, l3index, addr, AF_INET);
		if (key) {
			if (tcp_md5_key_copy(newsk, addr, AF_INET, 32, l3index, key))
				goto put_and_exit;
			sk_gso_disable(newsk);
		}
	#endif
	
		if (__inet_inherit_port(sk, newsk) < 0)
			goto put_and_exit;
		*own_req = inet_ehash_nolisten(newsk, req_to_sk(req_unhash),
					       &found_dup_sk);
		if (likely(*own_req)) {
			tcp_move_syn(newtp, req);
			ireq->ireq_opt = NULL;
		} else {
			newinet->inet_opt = NULL;
	
			if (!req_unhash && found_dup_sk) {
				/* This code path should only be executed in the
				 * syncookie case only
				 */
				bh_unlock_sock(newsk);
				sock_put(newsk);
				newsk = NULL;
			}
		}
		return newsk;
	
	exit_overflow:
		NET_INC_STATS(sock_net(sk), LINUX_MIB_LISTENOVERFLOWS);
	exit_nonewsk:
		dst_release(dst);
	exit:
		tcp_listendrop(sk);
		return NULL;
	put_and_exit:
		newinet->inet_opt = NULL;
		inet_csk_prepare_forced_close(newsk);
		tcp_done(newsk);
		goto exit;
	}
	EXPORT_SYMBOL(tcp_v4_syn_recv_sock);
```

tcp_v4_syn_recv_sock这个函数完成TCP三次握手之后被调用，用于创建一个新的，可通信的socket(struct sock *)，即子socket（child socket），这个子socket是后续accept()系统调用中由应用程序取出的socket。主要就是新建一个socket，配置socket的字段、远端，本地IP地址、接受接口、TTL、TOS等信息。

我们先看newsk = tcp_create_openreq_child(sk, req, skb);这段：

```
	struct sock *tcp_create_openreq_child(const struct sock *sk,
				      struct request_sock *req,
				      struct sk_buff *skb)
	{
		struct sock *newsk = inet_csk_clone_lock(sk, req, GFP_ATOMIC);
		const struct inet_request_sock *ireq = inet_rsk(req);
		struct tcp_request_sock *treq = tcp_rsk(req);
		struct inet_connection_sock *newicsk;
		const struct tcp_sock *oldtp;
		struct tcp_sock *newtp;
		u32 seq;
	
		if (!newsk)
			return NULL;
	
		newicsk = inet_csk(newsk);
		newtp = tcp_sk(newsk);
		oldtp = tcp_sk(sk);
		...
		...
	
		return newsk;
	}
	EXPORT_SYMBOL(tcp_create_openreq_child);
```

这段的作用是在三次握手完成后，服务端根据临时的request_sock（用于存储客户端连接请求的"半连接"）信息生成一个新的、真正可通信的 sock对象（也就是所谓的“全连接”）。重点在第一行struct sock *newsk = inet_csk_clone_lock(sk, req, GFP_ATOMIC);

```
	struct sock *inet_csk_clone_lock(const struct sock *sk,
				 const struct request_sock *req,
				 const gfp_t priority)
	{
		struct sock *newsk = sk_clone_lock(sk, priority);
	
		if (newsk) {
			struct inet_connection_sock *newicsk = inet_csk(newsk);
	
			newsk->sk_wait_pending = 0;
			inet_sk_set_state(newsk, TCP_SYN_RECV);
			newicsk->icsk_bind_hash = NULL;
			newicsk->icsk_bind2_hash = NULL;
	
			inet_sk(newsk)->inet_dport = inet_rsk(req)->ir_rmt_port;
			inet_sk(newsk)->inet_num = inet_rsk(req)->ir_num;
			inet_sk(newsk)->inet_sport = htons(inet_rsk(req)->ir_num);
	
			/* listeners have SOCK_RCU_FREE, not the children */
			sock_reset_flag(newsk, SOCK_RCU_FREE);
	
			inet_sk(newsk)->mc_list = NULL;
	
			newsk->sk_mark = inet_rsk(req)->ir_mark;
			atomic64_set(&newsk->sk_cookie,
				     atomic64_read(&inet_rsk(req)->ir_cookie));
	
			newicsk->icsk_retransmits = 0;
			newicsk->icsk_backoff	  = 0;
			newicsk->icsk_probes_out  = 0;
			newicsk->icsk_probes_tstamp = 0;
	
			/* Deinitialize accept_queue to trap illegal accesses. */
			memset(&newicsk->icsk_accept_queue, 0, sizeof(newicsk->icsk_accept_queue));
	
			inet_clone_ulp(req, newsk, priority);
	
			security_inet_csk_clone(newsk, req);
		}
		return newsk;
	}
	EXPORT_SYMBOL_GPL(inet_csk_clone_lock);
```

看这行inet_sk_set_state(newsk, TCP_SYN_RECV);这里将新socket的状态设置为 TCP_SYN_RECV，表示它正处于三次握手的中间状态（收到SYN后已发送 SYN-ACK）

最后再回到tcp_v4_rcv函数，进入tcp_v4_do_rcv：

```
	int tcp_v4_do_rcv(struct sock *sk, struct sk_buff *skb)
	{
		enum skb_drop_reason reason;
		struct sock *rsk;
		...
		...
		if (tcp_rcv_state_process(sk, skb)) {
			rsk = sk;
			goto reset;
		}
		return 0;
		...
		...
	}
	EXPORT_SYMBOL(tcp_v4_do_rcv);
```

这里直接进入tcp_rcv_state_process函数：

```
	int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb)
	{
		struct tcp_sock *tp = tcp_sk(sk);
		struct inet_connection_sock *icsk = inet_csk(sk);
		const struct tcphdr *th = tcp_hdr(skb);
		struct request_sock *req;
		int queued = 0;
		bool acceptable;
		SKB_DR(reason);
	
		switch (sk->sk_state) {
		case TCP_CLOSE:
			...
			...
		case TCP_LISTEN:
			...
			...
	
		case TCP_SYN_SENT:
			...
			...
		}
	
		switch (sk->sk_state) {
		case TCP_SYN_RECV:
			tp->delivered++; /* SYN-ACK delivery isn't tracked in tcp_ack */
			if (!tp->srtt_us)
				tcp_synack_rtt_meas(sk, req);
	
			if (req) {
				tcp_rcv_synrecv_state_fastopen(sk);
			} else {
				tcp_try_undo_spurious_syn(sk);
				tp->retrans_stamp = 0;
				tcp_init_transfer(sk, BPF_SOCK_OPS_PASSIVE_ESTABLISHED_CB,
						  skb);
				WRITE_ONCE(tp->copied_seq, tp->rcv_nxt);
			}
			smp_mb();
			tcp_set_state(sk, TCP_ESTABLISHED);
			sk->sk_state_change(sk);
	
			/* Note, that this wakeup is only for marginal crossed SYN case.
			 * Passively open sockets are not waked up, because
			 * sk->sk_sleep == NULL and sk->sk_socket == NULL.
			 */
			if (sk->sk_socket)
				sk_wake_async(sk, SOCK_WAKE_IO, POLL_OUT);
	
			tp->snd_una = TCP_SKB_CB(skb)->ack_seq;
			tp->snd_wnd = ntohs(th->window) << tp->rx_opt.snd_wscale;
			tcp_init_wl(tp, TCP_SKB_CB(skb)->seq);
	
			if (tp->rx_opt.tstamp_ok)
				tp->advmss -= TCPOLEN_TSTAMP_ALIGNED;
	
			if (!inet_csk(sk)->icsk_ca_ops->cong_control)
				tcp_update_pacing_rate(sk);
	
			/* Prevent spurious tcp_cwnd_restart() on first data packet */
			tp->lsndtime = tcp_jiffies32;
	
			tcp_initialize_rcv_mss(sk);
			tcp_fast_path_on(tp);
			break;
	
		case TCP_FIN_WAIT1: {
			...
			...
		case TCP_CLOSING:
			...
			...
	
		case TCP_LAST_ACK:
			...
			...
		}
	
		/* step 6: check the URG bit */
		tcp_urg(sk, skb, th);
	
		/* step 7: process the segment text */
		switch (sk->sk_state) {
		case TCP_CLOSE_WAIT:
		case TCP_CLOSING:
		case TCP_LAST_ACK:
			...
			...
		case TCP_FIN_WAIT1:
		case TCP_FIN_WAIT2:
			...
			...
		case TCP_ESTABLISHED:
			tcp_data_queue(sk, skb);
			queued = 1;
			break;
		}
	
		/* tcp_data could move socket to TIME-WAIT */
		if (sk->sk_state != TCP_CLOSE) {
			tcp_data_snd_check(sk);
			tcp_ack_snd_check(sk);
		}
	
		if (!queued) {
	discard:
			tcp_drop_reason(sk, skb, reason);
		}
		return 0;
	
	consume:
		__kfree_skb(skb);
		return 0;
	}
	EXPORT_SYMBOL(tcp_rcv_state_process);
```

这里由于之前新建的全连接socket的sk_state已经设置成了TCP_SYN_RECV，所以这里进入这个分支，调用tcp_set_state(sk, TCP_ESTABLISHED)直接把socket的状态变成TCP_ESTABLISHED。

到这里，从客户端到服务端的TCP三次握手协议的源码解析已经完成了，我们可以通过代码看到内核在三次握手时所做的操作，半连接socket和全连接socket的建立，通过四元组查在半连接队列中查找找半连接socket的方式等等。