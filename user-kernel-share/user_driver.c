#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define DEVICE "/dev/kernel_driver"
#define PAGE_SIZE 4096
#define MD_OFFSET 2
/* Compiler will automatically add null at the end */
#define USER_STR "Hello from User"

static void *raw_pg, *write_ptr;

static int check_and_set_flag(void) {
    uint8_t expected, desired;

    if (raw_pg == NULL)
        return 1;

    do {

        expected = *(volatile uint8_t *)raw_pg;
        desired = 0x10 | (expected & 0x0F);

    } while (__sync_val_compare_and_swap((uint8_t *)raw_pg, expected, desired) != expected);

    return 0;
}
static int set_write_ptr(void) {
    uint16_t curr_sz;
    curr_sz = ((*(uint8_t *)raw_pg & 0x0F) << 8) |
              ((*((uint8_t *)raw_pg + 1) & 0xFF));

    write_ptr = raw_pg + MD_OFFSET + curr_sz;
    if (write_ptr + sizeof(USER_STR) > raw_pg + PAGE_SIZE)
        return 2;

    return 0;
}

static int write_string_to_kernel_page(void) {
    uint16_t curr_sz;

    strncpy(write_ptr, USER_STR, sizeof(USER_STR));

    curr_sz = write_ptr + sizeof(USER_STR) - raw_pg - MD_OFFSET;

    (*(uint8_t *)raw_pg) = curr_sz >> 8;
    (*((uint8_t *)raw_pg + 1)) = (curr_sz & 0x00FF);

    return 0;
}

static void print_page(void) {
    for(int i = 0; i < PAGE_SIZE; i++) {
        printf("%c", ((char *)raw_pg)[i]);
    }
    printf("\n");
}

int main() {
    int fd, err;
    
    fd = open(DEVICE, O_RDWR);
    if(fd < 0) {
        perror("open");
        return 1;
    }

    raw_pg = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(raw_pg == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    printf("User space driver reads from shared page: %d\n", (*(uint16_t *)raw_pg));
    for (int i = 0; i < 100; i++) {
        err = check_and_set_flag();
        if (err) {
            printf(DEVICE "Check and set flag failed!");
            goto out;
        }  

        err = set_write_ptr();
        if (err) {
            printf(DEVICE "Setting write ptr failed");
            goto out;
        }

        err = write_string_to_kernel_page();
        if (err) {
            printf(DEVICE "Writing string to kernel page failed");
            goto out;
        }
    }

out:
    print_page();
    munmap(raw_pg, PAGE_SIZE);
    close(fd);
    return 0;
}