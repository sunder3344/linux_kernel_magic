#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <liburing.h>

#define LISTENQ 10
#define CQE_LEN 10
#define RING_LEN 1024			//ring queue num
#define MAXLINE 4096
#define SOCKET_NUM 4096
#define SERV_PORT 8888
#define IP_ADDR "0.0.0.0"
#define SERVER_DIR "./"
#define MAX_CONN 1024

enum {
	EVENT_ACCEPT = 0,
	EVENT_READ,
	EVENT_WRITE
};

struct ConnInfo {
	int connfd;
	int event;
	char buffer[MAXLINE];
	size_t buffer_length;
} __attribute__((aligned(64)));

struct ConnInfo * set_accept_event(struct io_uring *ring, int sfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
	struct __kernel_timespec timeout;
	timeout.tv_sec = 3;  // 3秒超时
	timeout.tv_nsec = 0;

	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_timeout(sqe, &timeout, 0, 0);
	io_uring_prep_accept(sqe, sfd, addr, addrlen, flags);
	
	struct ConnInfo *info = malloc(sizeof(struct ConnInfo));
	memset(info, 0, sizeof(struct ConnInfo));
	if (!info) {
		printf("Get conn failed!!!\n");
		return 0;
	}
	info->connfd = sfd;
	info->event = EVENT_ACCEPT;
	io_uring_sqe_set_data(sqe, info);
	return info;
}

void set_recv_event(struct io_uring *ring, int sfd, struct ConnInfo *info, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_recv(sqe, sfd, info->buffer, info->buffer_length, flags);

	info->connfd = sfd;
	info->event = EVENT_READ;
	io_uring_sqe_set_data(sqe, info);
}

void set_send_event(struct io_uring *ring, int sfd, struct ConnInfo *info, int length, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_send(sqe, sfd, info->buffer, length, flags);

	info->connfd = sfd;
	info->event = EVENT_WRITE;
	io_uring_sqe_set_data(sqe, info);
}

int enable_keepalive(int sockfd) {
    int optval = 1;
    socklen_t optlen = sizeof(optval);

    //set TCP_KEEPALIVE
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
        perror("setsockopt(SO_KEEPALIVE) failed");
        return -1;
    }

    //set Keepalive probe interval
    int keep_idle = 60;  //60 sec
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, optlen) < 0) {
        perror("setsockopt(TCP_KEEPIDLE) failed");
        return -1;
    }

    //probe send interval
    int keep_interval = 10;  //10 sec
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, optlen) < 0) {
        perror("setsockopt(TCP_KEEPINTVL) failed");
        return -1;
    }

    //probe num
    int keep_count = 3;  //probe num
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, optlen) < 0) {
        perror("setsockopt(TCP_KEEPCNT) failed");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
	struct sockaddr_in clientaddr, serveraddr;
	struct io_uring_params params;
	memset(&params, 0, sizeof (struct io_uring_params));
	//params.flags = IORING_SETUP_SQPOLL;
	//params.sq_thread_idle = 2000;
	struct io_uring ring;
	int sockfd;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	const int val = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));		//addr reuse
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));		//port reuse

	//set timeout
	struct timeval socktimeout;
	socktimeout.tv_sec = 3;  // socket timeout 3 sec
	socktimeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&socktimeout, sizeof(socktimeout));
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&socktimeout, sizeof(socktimeout));

	//set linger
	struct linger so_linger;
	so_linger.l_onoff = 1;  // open linger
	so_linger.l_linger = 0; // set 0, close immediately
	setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

	if (enable_keepalive(sockfd) < 0) {
		close(sockfd);
		perror("set keepalive error!");
		return -1;
	}

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERV_PORT);
	
	if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		perror("bind error");
		exit(-1);
	}
	if (listen(sockfd, LISTENQ) < 0) {
		perror("listen error");
		exit(-1);
	}
	
	io_uring_queue_init_params(RING_LEN, &ring, &params);
	struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
	socklen_t clilen = sizeof(struct sockaddr);
	
	set_accept_event(&ring, sockfd, (struct sockaddr *)&clientaddr, &clilen, 0);

	while (1) {
		int tmp = io_uring_submit(&ring);
		
		struct io_uring_cqe *cqe;
		io_uring_wait_cqe(&ring, &cqe);

		struct io_uring_cqe *cqes[CQE_LEN];
		int cqecount = io_uring_peek_batch_cqe(&ring, cqes, CQE_LEN);
		
		int i;
		for (i = 0; i < cqecount; i++) {
			cqe = cqes[i];
			struct ConnInfo *ci = (struct ConnInfo *)io_uring_cqe_get_data(cqe);
			if (!ci) {
				free(ci);
				continue;
			}
			
			if (ci->event == EVENT_ACCEPT) {
				if (cqe->res < 0) {
					printf("cqe->res=%d\n", cqe->res);
					free(ci);
					continue;
				}
				int connfd = cqe->res;			//if cqe->res > 0, it means success，res is new fd
				
				struct ConnInfo *new_info = malloc(sizeof(struct ConnInfo));
				memset(new_info, 0, sizeof(struct ConnInfo));
				new_info->buffer_length = MAXLINE;
				printf("new_info=%p\n", new_info);

				set_recv_event(&ring, connfd, new_info, 0);
				set_accept_event(&ring, ci->connfd, (struct sockaddr *)&clientaddr, &clilen, 0);
				free(ci);
			} else if (ci->event == EVENT_READ) {
				if (cqe->res == 0) {
					printf("client close\n");
					close(ci->connfd);
					free(ci);
				} else if (cqe->res < 0) {
					printf("read_cqe_res=%d\n", cqe->res);
					close(ci->connfd);
					free(ci);
				} else {
					ci->buffer_length = cqe->res;
					printf("recv: %s, %d\n", ci->buffer, cqe->res);
					set_send_event(&ring, ci->connfd, ci, cqe->res, 0);
				}
			} else if (ci->event == EVENT_WRITE) {
				if (cqe->res <= 0) {
					printf("write_cqe_res=%d\n", cqe->res);
					close(ci->connfd);
                	free(ci);
                	continue;
				}
				set_recv_event(&ring, ci->connfd, ci, 0);
			}
			cqe = NULL;
		}
		io_uring_cq_advance(&ring, cqecount);
	}
}
