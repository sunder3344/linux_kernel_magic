#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <liburing.h>

#define BUF_SIZE 4096
#define DEPTH 1024		//depth determin the capacity of the ring

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

int main(int argc, char *argv[]) {
	if (argc != 3) {
		perror("wrong parameters!");
		exit(1);
	}
	const char *source_file = argv[1];
	const char *dest_file = argv[2];

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

	/*char *buffer = malloc(BUF_SIZE);
	memset(buffer, 0, BUF_SIZE);
	if (!buffer) {
		perror("malloc error");
		exit(-1);
	}*/

	off_t file_sz = get_file_size(source_fd);
	printf("size:= %lu", file_sz);
	off_t offset = 0;
	int ret, ret2, j;
	unsigned long i = 0;

	while (1) {
		char *buffer = malloc(BUF_SIZE);
		memset(buffer, 0, BUF_SIZE);
		struct io_uring_sqe *sqe;
		sqe = io_uring_get_sqe(&ring);
		//sqe->flags |= IOSQE_IO_LINK;
		io_uring_prep_read(sqe, source_fd, buffer, BUF_SIZE, offset);
		struct File_data *file_data = malloc(sizeof(struct File_data));
		file_data->index = i;
		file_data->offset = offset;
		//file_data->data = (char *)malloc(sizeof(char)*BUF_SIZE);
		file_data->data = buffer;

		printf("buffer=%s, offset:=%lu, index=%lu\n", buffer, offset, i);
		io_uring_sqe_set_data(sqe, file_data);
		offset += BUF_SIZE;
		i++;
		if (offset > file_sz) break;
	}
	
	ret = io_uring_submit(&ring);
	printf("ret: = %d\n", ret);
	if (ret < 0) {
		perror("submit error!");
		exit(-1);
	}	
	
	for (j = 0; j < ret; j++) {
		struct io_uring_cqe *cqe;
		ret2 = io_uring_wait_cqe(&ring, &cqe);
		printf("ret2 = %d\n", ret2);
		if (ret2 < 0) {
			perror("wait queue error!");
			printf("err desc = %s\n", strerror(ret));
			exit(-1);
		}
		
		struct File_data *file_data = (struct File_data *)io_uring_cqe_get_data(cqe);
		//printf("index = %lu, offset = %lu, buffer = %s, res = %lu\n", (unsigned long)file_data->index, (off_t)file_data->offset, (char *)file_data->data, cqe->res);
		ssize_t bytes_written = write(dest_fd, (char *)file_data->data, cqe->res);
		printf("size = %lu\n", bytes_written);
		if (bytes_written < 0) {
			perror("write error!");
			exit(-1);
		}		
		io_uring_cqe_seen(&ring, cqe);
		free(file_data);
	}
	close(source_fd);
	close(dest_fd);
	io_uring_queue_exit(&ring);
	return 0;
}
