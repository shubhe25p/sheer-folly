#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define DEVICE "/dev/kernel_driver"
#define PAGE_SIZE 4096

int main() {
    int fd = open(DEVICE, O_RDWR);
    if(fd < 0) {
        perror("open");
        return 1;
    }

    char *k_page = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(k_page == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    printf("User space driver reads from shared page: %d\n", (*(uint8_t *)k_page));
    munmap(k_page, PAGE_SIZE);
    close(fd);
    return 0;
}