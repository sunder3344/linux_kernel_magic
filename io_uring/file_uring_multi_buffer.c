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
int dest_fd, source_fd = 0;

struct file_data {
	off_t offset;
	struct iovec iov;
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

void read_cqe_recv(struct io_uring *ring, unsigned long count) {
	int block = 1;
	struct io_uring_cqe *cqe;
	for (int i = 0; i < count; i++) {
		int res;
		if (block == 1) {
			res = io_uring_wait_cqe(ring, &cqe);
			block = 0;
		} else {
			res = io_uring_peek_cqe(ring, &cqe);
			while (res == -EAGAIN) {
				res = io_uring_wait_cqe(ring, &cqe);
				if (res < 0) {
					perror("io_uring_wait_cqe error1");
					exit(1);
				}
			}
		}
        if (res < 0) {
            perror("io_uring_wait_cqe error2");
            exit(1);
        }

		struct file_data *data = io_uring_cqe_get_data(cqe);
		if (cqe->res < 0) {
            fprintf(stderr, "Read error: %s\n", strerror(-cqe->res));
            free(data);
            io_uring_cqe_seen(ring, cqe);
            continue;
        }

		struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
		int bytes_read = cqe->res;
		io_uring_prep_writev(sqe, dest_fd, &data->iov, 1, data->offset);
		io_uring_sqe_set_data(sqe, data);

		io_uring_cqe_seen(ring, cqe);
    }
	int ret = io_uring_submit(ring);
	if (ret < 0) {
		perror("io_uring_submit error3");
		exit(1);
	}
}

void write_cqe_recv(struct io_uring *ring, unsigned long count) {
	int block = 1;
	int ret;
	struct io_uring_cqe *cqe;
	for (int i = 0; i < count; i++) {
		ret = io_uring_wait_cqe(ring, &cqe);
		if (ret) {
			fprintf(stderr, "wait_cqe=%d\n", ret);
			return;
		}
		if (cqe->res < 0) {
			fprintf(stderr, "write res=%d\n", cqe->res);
			return;
		}

        // Release memory
        struct file_data *data = io_uring_cqe_get_data(cqe);
        free(data);

        io_uring_cqe_seen(ring, cqe);
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

	source_fd = open(source_file, O_RDONLY);
	if (source_fd == -1) {
		perror("open source file error!");
		exit(-1);
	}

	dest_fd = open(dest_file, O_WRONLY| O_CREAT| O_TRUNC, 0644);
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
	off_t offset = 0;
	off_t insize = file_sz;
	int ret, ret2, j;
	unsigned long i = 1;
	unsigned long reads = 0;
	unsigned long writes = 0;
	int flag = 1;		//1£ºread£º2£ºwrite

	off_t this_size;
	int depth = 0;
	while (1) {
		this_size = BUF_SIZE;
		if (this_size > insize) {
			this_size = insize;
		}
		
		if (flag == 1 && insize > 0) {
			struct file_data *data = malloc(this_size + sizeof(struct file_data));
			struct io_uring_sqe *sqe;
			sqe = io_uring_get_sqe(&ring);

			data->offset = offset;
			data->iov.iov_base = data + 1;
			data->iov.iov_len = this_size;

			io_uring_prep_readv(sqe, source_fd, &data->iov, 1, offset);
			io_uring_sqe_set_data(sqe, data);
			reads++;
			depth++;
			
			if ((this_size == insize) || reads >= DEPTH) {		//where queue count equals DEPTH, then submit and read queue
				ret = io_uring_submit(&ring);
				if (ret < 0) {
					perror("io_uring_submit error");
					exit(1);
				}
				flag = 2;
				writes = reads;
				reads = 0;
				depth = 0;
				read_cqe_recv(&ring, writes);
			}
			insize -= this_size;
			offset += this_size;
		}
		if (flag == 2) {
			write_cqe_recv(&ring, writes);
			flag = 1;
			writes = 0;
		}

		if (offset >= file_sz)
			break;
	}

	close(source_fd);
	close(dest_fd);
	io_uring_queue_exit(&ring);
	return 0;
}
