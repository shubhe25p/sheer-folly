#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define DEVICE_NAME "kernel_driver_mod\t"
#define PAGE_SIZE 4096
#define KERNEL_STR "Hello from Kernel\0"
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)  \
    (((i) & 0x80) ? '1' : '0'),        \
    (((i) & 0x40) ? '1' : '0'),        \
    (((i) & 0x20) ? '1' : '0'),        \
    (((i) & 0x10) ? '1' : '0'),        \
    (((i) & 0x08) ? '1' : '0'),        \
    (((i) & 0x04) ? '1' : '0'),        \
    (((i) & 0x02) ? '1' : '0'),        \
    (((i) & 0x01) ? '1' : '0')

static void *raw_pg, *write_ptr;

static int test_setup(void) {

    raw_pg = (void *)calloc(PAGE_SIZE, 1);
    
    if (raw_pg == NULL)
        return 1;
    return 0;
}

static int check_and_set_flag(uint8_t *expected, uint8_t *desired) {
    if (raw_pg == NULL)
        return 1;

    do {

        *expected = *(volatile uint8_t *)raw_pg;
        *desired = 0x10 | (*expected & 0x0F);

    } while (__sync_val_compare_and_swap((uint8_t *)raw_pg, *expected, *desired) != *expected);

    return 0;
}
static int set_write_ptr(void) {
    return 0;
}
static int write_string_to_kernel_page(void) {
    return 0;
}

int main() {
    int err, nerr=0;
    uint8_t expected, desired;

    
    err = test_setup();
    if (err) {
        printf(DEVICE_NAME "Test setup failed!");
        nerr++;
        goto out;
    }

    printf(DEVICE_NAME "Test setup Done\n");

    (*(uint8_t *)raw_pg) = 0b00001010;
    err = check_and_set_flag(&expected, &desired);
    if (err) {
        printf("Check and set flag failed!");
        nerr++;
        goto out;
    }

    if (expected != 0b00001010) {
        printf("Expected Value in binary: " PRINTF_BINARY_PATTERN_INT8 "\n",
           PRINTF_BYTE_TO_BINARY_INT8(expected));
        goto out;
    }

    if(desired != 0b00011010) {
        printf("Desired Value in binary: " PRINTF_BINARY_PATTERN_INT8 "\n",
           PRINTF_BYTE_TO_BINARY_INT8(desired));
        goto out;
    }

    printf(DEVICE_NAME "Check and set flag test done\n");

    err = set_write_ptr();
    if (err) {
        printf("Setting write ptr failed");
        nerr++;
        goto out;
    }

    printf(DEVICE_NAME "Setting write ptr Done\n");

    err = write_string_to_kernel_page();
    if (err) {
        printf("Writing string to kernel page failed");
        nerr++;
        goto out;
    }
    goto success;
    
out:
    printf(" Unit test failed!");

success:
    if(nerr == 0)
        printf("Unit test passed");
}

