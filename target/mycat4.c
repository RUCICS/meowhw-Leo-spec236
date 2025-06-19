#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <math.h>

// 计算最大公约数
static size_t gcd(size_t a, size_t b) {
    while (b != 0) {
        size_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// 计算最小公倍数
static size_t lcm(size_t a, size_t b) {
    if (a == 0 || b == 0) return 0;
    return (a * b) / gcd(a, b);
}

// 检查是否为2的幂
static int is_power_of_two(size_t n) {
    return (n != 0) && ((n & (n - 1)) == 0);
}

// 获取最优块大小
size_t io_blocksize(const char* filename) {
    // 获取内存页大小
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        page_size = 4096;
    }
    
    // 获取文件系统块大小
    struct stat file_stat;
    size_t block_size = page_size; // 默认使用页大小
    
    if (stat(filename, &file_stat) == 0) {
        // 获取文件系统的I/O块大小
        size_t fs_block_size = file_stat.st_blksize;
        
        // 验证块大小的合理性
        if (fs_block_size > 0 && fs_block_size <= 1024 * 1024 && // 合理范围
            is_power_of_two(fs_block_size)) { // 必须是2的幂
            block_size = fs_block_size;
        }
    }
    
    // 计算内存页大小和文件系统块大小的最小公倍数
    size_t optimal_size = lcm((size_t)page_size, block_size);
    
    // 设置合理的最小和最大限制
    const size_t min_size = 4096;      // 4KB
    const size_t max_size = 1024 * 1024; // 1MB
    
    // 确保大小在合理范围内
    if (optimal_size < min_size) {
        optimal_size = min_size;
    } else if (optimal_size > max_size) {
        optimal_size = max_size;
    }
    
    // 确保大小是内存页的倍数
    if (optimal_size % page_size != 0) {
        optimal_size = ((optimal_size + page_size - 1) / page_size) * page_size;
    }
    
    return optimal_size;
}

// 分配对齐内存的函数
void* align_alloc(size_t size) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        page_size = 4096;
    }
    
    // 分配额外的空间用于存储原始指针和确保对齐
    size_t total_size = size + page_size + sizeof(void*);
    void* raw_ptr = malloc(total_size);
    
    if (raw_ptr == NULL) {
        return NULL;
    }
    
    // 计算对齐的起始地址
    uintptr_t start_addr = (uintptr_t)raw_ptr + sizeof(void*);
    uintptr_t aligned_addr = (start_addr + page_size - 1) & ~(page_size - 1);
    
    // 在分配的内存块开始处存储原始指针
    void** original_ptr = (void**)(aligned_addr - sizeof(void*));
    *original_ptr = raw_ptr;
    
    return (void*)aligned_addr;
}

// 释放对齐内存的函数
void align_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // 获取原始指针
    void** original_ptr = (void**)((uintptr_t)ptr - sizeof(void*));
    void* raw_ptr = *original_ptr;
    
    free(raw_ptr);
}

int main(int argc, char *argv[]) {
    // 检查参数数量
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // 获取最优缓冲区大小
    size_t buffer_size = io_blocksize(argv[1]);
    printf("Using buffer size: %zu bytes\n", buffer_size);
    
    // 分配对齐的缓冲区
    char *buffer = align_alloc(buffer_size);
    if (buffer == NULL) {
        fprintf(stderr, "Error allocating aligned memory: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // 打开文件
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file %s: %s\n", argv[1], strerror(errno));
        align_free(buffer);
        exit(EXIT_FAILURE);
    }
    
    ssize_t bytes_read;
    
    // 使用缓冲区读取并写入
    while ((bytes_read = read(fd, buffer, buffer_size)) > 0) {
        ssize_t bytes_written = 0;
        ssize_t total_written = 0;
        
        // 确保所有读取的数据都被写入
        while (total_written < bytes_read) {
            bytes_written = write(STDOUT_FILENO, buffer + total_written, bytes_read - total_written);
            
            if (bytes_written == -1) {
                fprintf(stderr, "Write error: %s\n", strerror(errno));
                close(fd);
                align_free(buffer);
                exit(EXIT_FAILURE);
            }
            
            total_written += bytes_written;
        }
    }
    
    // 检查读取错误
    if (bytes_read == -1) {
        fprintf(stderr, "Read error: %s\n", strerror(errno));
        close(fd);
        align_free(buffer);
        exit(EXIT_FAILURE);
    }
    
    // 关闭文件和释放资源
    if (close(fd) == -1) {
        fprintf(stderr, "Error closing file: %s\n", strerror(errno));
        align_free(buffer);
        exit(EXIT_FAILURE);
    }
    
    align_free(buffer);
    return EXIT_SUCCESS;
}