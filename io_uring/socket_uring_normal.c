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

enum {
	EVENT_ACCEPT = 0,
	EVENT_READ,
	EVENT_WRITE
};

struct ConnInfo {
	int connfd;
	int event;
};

void set_accept_event(struct io_uring *ring, int sfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_accept(sqe, sfd, addr, addrlen, flags);
	
	struct ConnInfo info = {
		.connfd = sfd,
		.event = EVENT_ACCEPT
	};
	memcpy(&sqe->user_data, &info, sizeof(struct ConnInfo));
}

void set_recv_event(struct io_uring *ring, int sfd, void *buf, size_t len, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_recv(sqe, sfd, buf, len, flags);

	struct ConnInfo info = {
		.connfd = sfd,
		.event = EVENT_READ
	};
	memcpy(&sqe->user_data, &info, sizeof(struct ConnInfo));
}

void set_send_event(struct io_uring *ring, int sfd, void *buf, size_t len, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_send(sqe, sfd, buf, len, flags);

	struct ConnInfo info = {
		.connfd = sfd,
		.event = EVENT_WRITE
	};
	memcpy(&sqe->user_data, &info, sizeof(struct ConnInfo));
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
	char buffer[MAXLINE] = {0};
	//params.flags = IORING_SETUP_SQPOLL;
    //params.sq_thread_idle = 2000;

	struct io_uring ring;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
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
		io_uring_submit(&ring);
		
		struct io_uring_cqe *cqe;
		io_uring_wait_cqe(&ring, &cqe);

		struct io_uring_cqe *cqes[CQE_LEN];
		int cqecount = io_uring_peek_batch_cqe(&ring, cqes, CQE_LEN);
		
		int i;
		for (i = 0; i < cqecount; i++) {
			cqe = cqes[i];
			struct ConnInfo *ci = (struct ConnInfo *)&cqe->user_data;
			//memcpy(ci, &cqe->user_data, sizeof());
			
			if (ci->event == EVENT_ACCEPT) {
				if (cqe->res < 0) continue;
				int connfd = cqe->res;			//if cqe->res > 0, it means success，res is new fd
				
				set_recv_event(&ring, connfd, buffer, MAXLINE, 0);
				set_accept_event(&ring, ci->connfd, (struct sockaddr *)&clientaddr, &clilen, 0);
			} else if (ci->event == EVENT_READ) {
				if (cqe->res < 0) {
					close(ci->connfd);
					continue;
				} else if (cqe->res == 0) {
					close(ci->connfd);
				} else {
					//printf("recv: %s, %d\n", buffer, cqe->res);

					set_send_event(&ring, ci->connfd, buffer, cqe->res, 0);
				}
			} else if (ci->event == EVENT_WRITE) {
				if (cqe->res < 0) {
					close(ci->connfd);
				}
				set_recv_event(&ring, ci->connfd, buffer, MAXLINE, 0);
			}
			cqe = NULL;
		}
		io_uring_cq_advance(&ring, cqecount);
	}
}
