#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <liburing.h>

#define LISTENQ 10
#define CQE_LEN 10
#define RING_LEN 1024			//ring queue num
#define MAXLINE 4096
#define SOCKET_NUM 4096
#define SERV_PORT 8888
#define IP_ADDR "127.0.0.1"
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
	struct ConnInfo *next;
};

struct ConnInfo *conn_pool = NULL;
int conn_pool_count = 0;

void init_conn_pool(int size) {
	conn_pool_count = size;
	int i;
	for (i = 0; i < size; i++) {
		struct ConnInfo *conn = malloc(sizeof(struct ConnInfo));
		conn->next = conn_pool;
		conn_pool = conn;
	}
}

struct ConnInfo* get_conn_info() {
	if (!conn_pool)
		return NULL;
	struct ConnInfo *conn = conn_pool;
	conn_pool = conn_pool->next;
	conn_pool_count--;
	return conn;
}

void return_conn_info(struct ConnInfo *conn) {
	conn->next = conn_pool;
	conn_pool = conn;
	conn_pool_count++;
}

void set_accept_event(struct io_uring *ring, int sfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_accept(sqe, sfd, addr, addrlen, flags);
	
	struct ConnInfo *info = get_conn_info();
	printf("info=%p\n", info);
	if (!info) {
		printf("Get conn failed!!!\n");
		return;
	}
	info->connfd = sfd;
	info->event = EVENT_ACCEPT;
	io_uring_sqe_set_data(sqe, info);
	//memcpy(&sqe->user_data, info, sizeof(struct ConnInfo));
}

void set_recv_event(struct io_uring *ring, int sfd, struct ConnInfo *info, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_recv(sqe, sfd, info->buffer, MAXLINE, flags);

	info->connfd = sfd;
	info->event = EVENT_READ;
	io_uring_sqe_set_data(sqe, info);
	//memcpy(&sqe->user_data, info, sizeof(struct ConnInfo));
}

void set_send_event(struct io_uring *ring, int sfd, struct ConnInfo *info, int length, int flags) {
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	io_uring_prep_send(sqe, sfd, info->buffer, length, flags);

	info->connfd = sfd;
	info->event = EVENT_WRITE;
	io_uring_sqe_set_data(sqe, info);
	//memcpy(&sqe->user_data, info, sizeof(struct ConnInfo));
}

void render(int sock_fd, char *buf) {
	char buffer[MAXLINE];
	char *url = strtok(buf + 4, " ");
	char *path = strtok(url, "?");
	char file_path[MAXLINE];
	sprintf(file_path, "%s%s", SERVER_DIR, path);
	//printf("file_path: = %s\n", file_path);
	FILE *file = fopen(file_path, "r");
	if (file != NULL) {
		char *response = "HTTP/1.1 200 OK\n\n";
		send(sock_fd, response, strlen(response), 0);
		
		memset(buffer, 0, MAXLINE);
		while (fgets(buffer, MAXLINE, file) != NULL) {
			send(sock_fd, buffer, strlen(buffer), 0);
		}
		fclose(file);
	} else {
		char not_found[] = "HTTP/1.1 404 Not Found\n\n";
		send(sock_fd, not_found, strlen(not_found), 0);
	}
	close(sock_fd);
}

int main(int argc, char *argv[]) {
	struct sockaddr_in clientaddr, serveraddr;
	struct io_uring_params params;
	memset(&params, 0, sizeof (struct io_uring_params));
	params.flags = IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2000;
	struct io_uring ring;
	int sockfd;

	init_conn_pool(MAX_CONN);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	const int val = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));		//addr reuse
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));		//port reuse
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
			//struct ConnInfo *ci = (struct ConnInfo *)&cqe->user_data;
			struct ConnInfo *ci = (struct ConnInfo *)io_uring_cqe_get_data(cqe);
			if (!ci) {
				continue;
			}
			
			if (ci->event == EVENT_ACCEPT) {
				if (cqe->res < 0) {
					printf("cqe->res=%d\n", cqe->res);
					return_conn_info(ci);
					continue;
				}
				int connfd = cqe->res;			//if cqe->res > 0, it means success，res is new fd
				
				struct ConnInfo *new_info = get_conn_info();
				printf("new_info=%p\n", new_info);
				if (new_info) {
					set_recv_event(&ring, connfd, new_info, 0);
				} else {
					printf("Get conn failed!\n");
					close(connfd);
					return_conn_info(ci);
				}
				set_accept_event(&ring, ci->connfd, (struct sockaddr *)&clientaddr, &clilen, 0);
				return_conn_info(ci);
			} else if (ci->event == EVENT_READ) {
				if (cqe->res == 0) {
					printf("client close\n");
					close(ci->connfd);
					return_conn_info(ci);
				} else if (cqe->res < 0) {
					printf("read_cqe_res=%d\n", cqe->res);
					close(ci->connfd);
					return_conn_info(ci);
				} else {
					printf("recv: %s, %d\n", ci->buffer, cqe->res);
					ci->buffer_length = cqe->res;
					set_send_event(&ring, ci->connfd, ci, cqe->res, 0);
				}
			} else if (ci->event == EVENT_WRITE) {
				if (cqe->res <= 0) {
					printf("write_cqe_res=%d\n", cqe->res);
					close(ci->connfd);
                	return_conn_info(ci);
				}
				set_recv_event(&ring, ci->connfd, ci, 0);
			}
			cqe = NULL;
		}
		io_uring_cq_advance(&ring, cqecount);
	}
}
