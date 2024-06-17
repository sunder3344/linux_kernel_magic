#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <string.h>
#include <errno.h>

#define QUEUE_NAME "/test_queue"
#define MAX_SIZE 1024
#define MSG_STOP "exit"

int main() {
	mqd_t mq;
	struct mq_attr attr;
	char buffer[MAX_SIZE];
	int must_stop = 0;

	attr.mq_flags = 0;
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = MAX_SIZE;
	attr.mq_curmsgs = 0;
	
	mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr);
	if (mq == (mqd_t)-1) {
		perror("mq_error");
		printf("%s\n", strerror(errno));
		exit(-1);
	}

	while (!must_stop) {
		ssize_t bytes_read;
		
		bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
		if (bytes_read >= 0) {
			buffer[bytes_read] = '\0';
			if (!strncmp(buffer, MSG_STOP, strlen(MSG_STOP))) {
				must_stop = 1;
			} else {
				printf("Received: %s\n", buffer);
			}
		} else {
			perror("mq_receive");
		}
	}

	if (mq_close(mq) == -1) {
		perror("close mq error!");
		exit(-1);
	}

	if (mq_unlink(QUEUE_NAME) == -1) {
		perror("mq_unlink error!");
		exit(-1);
	}
	return 0;
}
