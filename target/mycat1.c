#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // 检查参数数量
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // 打开文件
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file %s: %s\n", argv[1], strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    char buffer;
    ssize_t bytes_read;
    
    // 逐字节读取并写入
    while ((bytes_read = read(fd, &buffer, 1)) > 0) {
        if (write(STDOUT_FILENO, &buffer, 1) != 1) {
            fprintf(stderr, "Write error: %s\n", strerror(errno));
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    
    // 检查读取错误
    if (bytes_read == -1) {
        fprintf(stderr, "Read error: %s\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    // 关闭文件
    if (close(fd) == -1) {
        fprintf(stderr, "Error closing file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    return EXIT_SUCCESS;
}