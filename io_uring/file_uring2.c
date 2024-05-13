#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <liburing.h>

#define BUF_SIZE (32*1024)
#define DEPTH 64	//depth determin the capacity of the ring
static int source_fd, dest_fd;

struct io_data {
	int read;
	off_t first_offset, offset;
	size_t first_len;
	struct iovec iov;
};

int get_file_size(int fd, off_t *size) {
	struct stat st;
	if (fstat(fd, &st) < 0) {
		return -1;
	}
	if (S_ISBLK(st.st_mode)) {
		unsigned long long bytes;
		if (ioctl(fd, BLKGETSIZE64, &bytes) != 0) {
			return -1;
		}
		*size = bytes;
		return 0;
	} else if (S_ISREG(st.st_mode)) {
		*size = st.st_size;
		return 0;
	}
	return -1;
}

static void queue_prepped(struct io_uring *ring, struct io_data *data) {
	struct io_uring_sqe *sqe;

	sqe = io_uring_get_sqe(ring);

	if (data->read)
		io_uring_prep_readv(sqe, source_fd, &data->iov, 1, data->offset);
	else
		io_uring_prep_writev(sqe, dest_fd, &data->iov, 1, data->offset);

	io_uring_sqe_set_data(sqe, data);
}


static int queue_read(struct io_uring *ring, off_t size, off_t offset) {
	struct io_uring_sqe *sqe;
	struct io_data *data;

	data = malloc(size + sizeof(*data));
	if (!data)
		return 1;
	
	sqe = io_uring_get_sqe(ring);
	if (!sqe) {
		free(data);
		return 1;
	}

	data->read = 1;
	data->offset = data->first_offset = offset;

	data->iov.iov_base = data + 1;
	data->iov.iov_len = size;
	data->first_len = size;

	io_uring_prep_readv(sqe, source_fd, &data->iov, 1, offset);
	io_uring_sqe_set_data(sqe, data);
	return 0;
}

static void queue_write(struct io_uring *ring, struct io_data *data) {
	data->read = 0;
	data->offset = data->first_offset;

	data->iov.iov_base = data + 1;
	data->iov.iov_len = data->first_len;

	queue_prepped(ring, data);
	io_uring_submit(ring);
}

static int copy_file(struct io_uring *ring, off_t insize) {
	unsigned long reads, writes;
	struct io_uring_cqe *cqe;
	off_t write_left, offset;
	int ret;

	write_left = insize;
	writes = reads = offset = 0;
	
	while (insize || write_left) {
		unsigned long had_reads;
		int got_comp;

		had_reads = reads;
		while (insize) {
			off_t this_size = insize;
			
			if (reads + writes >= DEPTH)
				break;
			if (this_size > BUF_SIZE) {
				this_size = BUF_SIZE;
			} else if (!this_size) {
				break;
			}

			if (queue_read(ring, this_size, offset))
				break;

			insize -= this_size;
			offset += this_size;
			reads++;
		}

		if (had_reads != reads) {
			ret = io_uring_submit(ring);
			if (ret < 0) {
				fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
				break;
			}
		}

		got_comp = 0;
		while (write_left) {
			struct io_data *data;

			if (!got_comp) {
				ret = io_uring_wait_cqe(ring, &cqe);
				got_comp = 1;
			} else {
				ret = io_uring_peek_cqe(ring, &cqe);
				if (ret == -EAGAIN) {
					cqe = NULL;
					ret = 0;
				}
			}
			if (ret < 0) {
				fprintf(stderr, "io_uring_peek_cqe: %s\n",
							strerror(-ret));
				return 1;
			}
			if (!cqe)
				break;

			data = io_uring_cqe_get_data(cqe);
			if (cqe->res < 0) {
				if (cqe->res == -EAGAIN) {
					queue_prepped(ring, data);
					io_uring_submit(ring);
					io_uring_cqe_seen(ring, cqe);
					continue;
				}
				fprintf(stderr, "cqe failed: %s\n",
						strerror(-cqe->res));
				return 1;
			} else if ((size_t)cqe->res != data->iov.iov_len) {
				data->iov.iov_base += cqe->res;
				data->iov.iov_len -= cqe->res;
				data->offset += cqe->res;
				queue_prepped(ring, data);
				io_uring_submit(ring);
				io_uring_cqe_seen(ring, cqe);
				continue;
			}

			if (data->read) {
				queue_write(ring, data);
				write_left -= data->first_len;
				reads--;
				writes++;
			} else {
				free(data);
				writes--;
			}
			io_uring_cqe_seen(ring, cqe);
		}
	}

	/**wait for write*/
	while (writes) {
		struct io_data *data;

		ret = io_uring_wait_cqe(ring, &cqe);
		if (ret) {
			fprintf(stderr, "wait_cqe=%d\n", ret);
			return 1;
		}
		if (cqe->res < 0) {
			fprintf(stderr, "write res=%d\n", cqe->res);
			return 1;
		}
		data = io_uring_cqe_get_data(cqe);
		free(data);
		writes--;
		io_uring_cqe_seen(ring, cqe);
	}

	return 0;
}

int main(int argc, char *argv[]) {
	const char *source_file = argv[1];
	const char *dest_file = argv[2];
	struct io_uring ring;
	off_t insize;
	int ret;

	if (argc != 3) {
		perror("wrong parameters!");
		exit(1);
	}

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

	if (io_uring_queue_init(DEPTH, &ring, 0) < 0) {
		perror("io_uring init error!");
		exit(-1);
	}

	if (get_file_size(source_fd, &insize))
		exit(-1);

	ret = copy_file(&ring, insize);
	close(source_fd);
	close(dest_fd);
	io_uring_queue_exit(&ring);
	return ret;
}
