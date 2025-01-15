#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MY_DEVICE_NAME "/dev/my_device"
#define MY_IOCTL_MAGIC 'k'

#define MY_IOCTL_SET_STATUS _IOW(MY_IOCTL_MAGIC, 1, int)
#define MY_IOCTL_GET_STATUS _IOR(MY_IOCTL_MAGIC, 2, int)

int main() {
	int fd;
	int status;

	fd = open(MY_DEVICE_NAME, O_RDWR);
	if (fd < 0) {
		perror("Failed to open device");
		return -1;
	}

	status = 3344;
	if (ioctl(fd, MY_IOCTL_SET_STATUS, &status) < 0) {
		perror("Failed to set device status");
		close(fd);
		return -1;
	}
	printf("Device status set to: %d\n", status);

	status = 0;
	if (ioctl(fd, MY_IOCTL_GET_STATUS, &status) < 0) {
		perror("Failed to get device status");
		close(fd);
		return -1;
	}
	printf("Device status read: %d\n", status);

	close(fd);
}
