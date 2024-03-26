#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <liburing.h>

#define BUF_SIZE 469000
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

	off_t file_sz = get_file_size(source_fd);
	//printf("size:= %lu", file_sz);
	off_t offset = 0;
	int ret, ret2, j;
	unsigned long i = 0;

	char *buffer = malloc(sizeof(char)*BUF_SIZE);
	memset(buffer, 0, BUF_SIZE);
	while (1) {	
		struct io_uring_sqe *sqe;
		sqe = io_uring_get_sqe(&ring);
		//sqe->flags |= IOSQE_IO_LINK;
		io_uring_prep_read(sqe, source_fd, buffer, BUF_SIZE, offset);
		struct File_data *file_data = malloc(sizeof(struct File_data));
		file_data->index = i;
		file_data->offset = offset;
		file_data->data = buffer;

		//printf("buffer=%s, offset:=%lu, index=%lu\n", buffer, offset, i);
		io_uring_sqe_set_data(sqe, file_data);
		offset += BUF_SIZE;
		i++;

		ret = io_uring_submit(&ring);
		sqe = NULL;
		free(file_data);
		
		struct io_uring_cqe *cqe;
		ret2 = io_uring_wait_cqe(&ring, &cqe);
		struct File_data *recv_data = (struct File_data *)io_uring_cqe_get_data(cqe);
		ssize_t bytes_written = write(dest_fd, (char *)recv_data->data, cqe->res);
		io_uring_cqe_seen(&ring, cqe);
		cqe = NULL;
		recv_data = NULL;

		if (offset > file_sz) break;
	}
	
	
	free(buffer);
	close(source_fd);
	close(dest_fd);
	io_uring_queue_exit(&ring);
	return 0;
}
