#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <libaio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#define BUF_SIZE 4096
#define MAX_EVENTS 1 
io_context_t ctx;

int main() {
	struct iocb cb;
	struct iocb *cbs[1];
	struct io_event events[MAX_EVENTS];
	void *buf;
	ssize_t bytes_read;
	int ret, fd, epoll_fd;
	posix_memalign(&buf, 512, 1024);

	memset(&ctx, 0, sizeof(ctx));		//it's necessary, or io_setup will report err(-22)
	
	ret = io_setup(MAX_EVENTS, &ctx);
	//printf("ret = %d, %s\n", ret, strerror(-ret));
	if (ret < 0) {
		perror("io_setup");
		exit(EXIT_FAILURE);
	}
	
	fd = open("./example.txt", O_RDONLY | O_DIRECT);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	memset(buf, 0, sizeof(buf));
	io_prep_pread(&cb, fd, buf, BUF_SIZE, 0);

	cbs[0] = &cb;
	
	int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	io_set_eventfd(cbs[0], efd);
	
	ret = io_submit(ctx, MAX_EVENTS, cbs);
	if (ret < 0) {
		perror("io_submit");
		exit(-1);
	}

	struct epoll_event ev, ep_events[MAX_EVENTS];
	int epfd, i;
	uint64_t data;
	
	epfd = epoll_create(MAX_EVENTS);		//生成epoll句柄
	ev.data.fd = efd;
	ev.events = EPOLLIN;

	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, efd, &ev);
	if (ret == -1) {
		perror("epoll_ctl");
        exit(EXIT_FAILURE);
	}

	while(1) {
		ret = epoll_wait(epfd, &ev, MAX_EVENTS, 1000);
		printf("epoll_ret = %d\n", ret);
		for (i = 0; i < ret; ++i) {
			if (ep_events[i].events & EPOLLIN) {
				int ret2 = read(efd, &data, sizeof(data));
				printf("ret2 = %d, data = %lu\n", ret2, data);
				if (ret2 < 0) {
					perror("read failure!");
				} else {
					int ret3 = io_getevents(ctx, 1, MAX_EVENTS, events, NULL); 
					printf("ret3 = %d\n", ret3);
					if (ret3 > 0) { 
						int j = 0;
						while (j < ret3) {
							bytes_read = events[j].res;
							printf("bytes_read = %d\n", bytes_read);
							if (bytes_read > 0) {
								printf("read results = %s\n", events[j].obj->u.c.buf);
                				//printf("Read %ld bytes: %s\n", (long)bytes_read, buf);
							}
							j++;
						}
					}
				}
			}
		}
	}

	close(epfd);
	close(fd);
	io_destroy(ctx);
	return 0;
}
