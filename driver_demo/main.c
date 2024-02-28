#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_FILE "/dev/module_connect"

int main() {
	int fd = open(MODULE_FILE, O_RDWR);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	const char *message = "This is from user space!";
	char *read_msg = (char *)malloc(sizeof(char *) * 1024);
	memset(read_msg, 0, sizeof(read_msg));
	write(fd, message, strlen(message));
	fsync(fd);

	lseek(fd, 0, SEEK_SET);
	read(fd, read_msg, 1024);	
	printf("read from core: %s\n", read_msg);
	close(fd);

	return 0;
}
