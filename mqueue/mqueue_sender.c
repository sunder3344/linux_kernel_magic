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

	attr.mq_flags = 0;
	attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

	mq_unlink(QUEUE_NAME);

	mq = mq_open(QUEUE_NAME, O_CREAT | O_WRONLY, 0644, &attr);
	if (mq == (mqd_t)-1) {
		perror("mq_open");
        exit(1);
	}

	while (1) {
		printf("Enter a message: ");
		fgets(buffer, MAX_SIZE, stdin);
		
		buffer[strcspn(buffer, "\n")] = '\0';
		
		if (mq_send(mq, buffer, strlen(buffer)+1, 0) == -1) {
			perror("mq_send");
            continue;
		}
	
		if (!strncmp(buffer, MSG_STOP, strlen(MSG_STOP))) {
            break;
        }
	}

	if (mq_close(mq) == -1) {
        perror("mq_close");
        exit(1);
    }

	return 0;
}
