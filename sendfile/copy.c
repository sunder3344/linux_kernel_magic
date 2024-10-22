#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>

int main(int argc,char *argv[]) {
    int source_fd, dest_fd;
    struct stat stat_source;
    off_t offset = 0;
    ssize_t bytes_sent;

    // 打开源文件和目标文件
    source_fd = open(argv[1], O_RDONLY);
    dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);

    // 获取源文件的大小
    fstat(source_fd, &stat_source);

    // 使用 sendfile 将源文件内容发送到目标文件
    bytes_sent = sendfile(dest_fd, source_fd, &offset, stat_source.st_size);

    if (bytes_sent == -1) {
        perror("sendfile");
        return 1;
    }

    printf("Successfully copied %ld bytes\n", bytes_sent);

    // 关闭文件描述符
    close(source_fd);
    close(dest_fd);

    return 0;
}
