#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pthread.h>
#include <liburing.h>

#define BUF_SIZE (32*1024)
#define DEPTH 64	//depth determin the capacity of the ring
unsigned long count = 0;

struct File_data {
	unsigned long index;
	off_t offset;
	char *data;
};

off_t get_file_size(int fd) {
	struct stat st;
	if (fstat(fd, &st) < 0) {
		return -1;
	}
	if (S_ISBLK(st.st_mode)) {
		unsigned long long bytes;
		if (ioctl(fd, BLKGETSIZE64, &bytes) != 0) {
			return -1;
		}
		return bytes;
	} else if (S_ISREG(st.st_mode)) {
		return st.st_size;
	}
	return -1;
}

void cqe_recv(struct io_uring *ring, struct io_uring_cqe *cqe, int ret) {
	while (ret) {
		int res = io_uring_wait_cqe(ring, &cqe);
		//printf("cqe res = %lu, %s\n", cqe->res, cqe->user_data);
		//release mem
		struct io_data *data = io_uring_cqe_get_data(cqe);
		data = NULL;
		
		io_uring_cqe_seen(ring, cqe);
		ret--;
	}
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		perror("wrong parameters!");
		exit(1);
	}
	const char *source_file = argv[1];
	const char *dest_file = argv[2];
	struct io_uring_cqe *cqe;

	int source_fd = open(source_file, O_RDONLY);
	if (source_fd == -1) {
		perror("open source file error!");
		exit(-1);
	}

	int dest_fd = open(dest_file, O_WRONLY| O_CREAT| O_TRUNC, 0644);
	if (dest_fd == -1) {
		perror("open dest file error!");
		exit(-1);
	}

	struct io_uring ring;
	if (io_uring_queue_init(DEPTH, &ring, 0) < 0) {
		perror("io_uring init error!");
		exit(-1);
	}

	off_t file_sz = get_file_size(source_fd);
	//printf("size:= %lu", file_sz);
	off_t offset = 0;
	off_t insize = file_sz;
	int ret, ret2, j;
	unsigned long i = 1;
	unsigned long flag = 0;
	char *buffer = malloc(sizeof(char)*BUF_SIZE);
	//memset(buffer, 0, BUF_SIZE);

	while (1) {
		off_t this_size;
		int depth;
		this_size = BUF_SIZE;
		if (this_size > insize) {
			this_size = insize;
		}
		
		struct io_uring_sqe *sqe;

		sqe = io_uring_get_sqe(&ring);
		io_uring_prep_read(sqe, source_fd, buffer, this_size, offset);
		//sqe->flags |= IOSQE_IO_LINK;
		io_uring_sqe_set_data(sqe, buffer);

		sqe = io_uring_get_sqe(&ring);
		io_uring_prep_write(sqe, dest_fd, buffer, this_size, offset);
		io_uring_sqe_set_data(sqe, buffer);
		flag += 2;
		count += flag;
		//printf("%d, %lu, %lu, %lu\n", flag, count, insize, this_size);
		
		if ((this_size == insize) || flag >= DEPTH) {		//where queue count equals DEPTH, then submit and read queue
			ret = io_uring_submit(&ring);
			flag = 0;
			//printf("submit ret = %d\n", ret);
			cqe_recv(&ring, cqe, ret);
		}
		
        insize -= this_size;
		offset += this_size;

		//printf("offset = %lu, file_sz = %lu\n", offset, file_sz);
		if (offset >= file_sz)
			break;
	}

	free(buffer);
	close(source_fd);
	close(dest_fd);
	io_uring_queue_exit(&ring);
	return 0;
}
