#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define DEVICE_NAME "kernel_driver_mod\t"
#define PAGE_SIZE 4096
#define MD_OFFSET 2
/* Compiler will automatically add null at the end */
#define KERNEL_STR "Hello from Kernel"
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

static int set_write_ptr(uint16_t *sz) {
    uint16_t curr_sz;
    /*
     * Why the fuck do we need the strange looking bit operation?
     * 
     * Let's take a step a back, I am on a little endian machine (M3)
     * Format for first 16 bits:[-- 4 bit lock -- 12 bit curr size --]
     *  0x10 0x00
     * 0x100 0x101
     * Now when we read, Little endian CPU thinks, lowest address
     * is at 0x100 and MSB at 0x101
     * So what's get stored in CPU register: 00 10
     * How can we extract 12 bits now, think about it?
     * CPU register = 0000(nimble 2) 0000(nimble 3) 0001(lock nimble don't touch) 0000(nimble 1)
     * curr_sz (16 bits) should be: 0000 (nimble 0) << 12 | nimble 1 << 8 | nimble 2 << 4 | nimble 3
     * 
     * And to prevent this, we extract byte at a time
     * (*(uint8_t *)raw_pg & 0x0F) this leaves the lock nimble untouched and gets nimble 1 and shifts it
     * ((*((uint8_t *)raw_pg + 1) & 0xFF) this get us the lower byte
     * SPENT 2 hrs trying to figure this shit out, and no AI help still!!
     */
    curr_sz = ((*(uint8_t *)raw_pg & 0x0F) << 8) |
              ((*((uint8_t *)raw_pg + 1) & 0xFF));

    write_ptr = raw_pg + MD_OFFSET + curr_sz;
    if (write_ptr > raw_pg + PAGE_SIZE)
        return 2;

    *sz = curr_sz;
    return 0;
}

static int write_string_to_kernel_page(void) {
    uint16_t curr_sz;

    strncpy(write_ptr, KERNEL_STR, sizeof(KERNEL_STR));

    write_ptr += sizeof(KERNEL_STR);
    curr_sz = write_ptr - raw_pg - MD_OFFSET;
    /*
     * If you want to know why this weird bit ops, look above
     * reading and write to a multi byte pointer will lead to
     * some weird behavior on little endian systems
     */
    (*(uint8_t *)raw_pg) = curr_sz >> 8;
    (*((uint8_t *)raw_pg + 1)) = (curr_sz & 0x00FF);

    return 0;
}

static int test_mw(void) {
    int err, nerr;
    uint8_t expected, desired;
    uint16_t curr_sz;


    err = test_setup();
    if (err) {
        printf(DEVICE_NAME "Test setup failed!");
        nerr++;
        goto out;
    }

    for (int i = 0; i < 20; i++) {
        err = check_and_set_flag(&expected, &desired);
        if (err) {
            printf("Check and set flag failed!");
            nerr++;
            goto out;
        }
        err = set_write_ptr(&curr_sz);
        if (err) {
            printf("Setting write ptr failed");
            nerr++;
            goto out;
        }

        err = write_string_to_kernel_page();
        if (err) {
            printf("Writing string to kernel page failed");
            nerr++;
            goto out;
        }
        printf("raw pg first 8 bits: " PRINTF_BINARY_PATTERN_INT8 "\t", 
           PRINTF_BYTE_TO_BINARY_INT8((*(uint8_t *)raw_pg)));
        printf("raw pg next 8 bits: " PRINTF_BINARY_PATTERN_INT8 "\n",
           PRINTF_BYTE_TO_BINARY_INT8((*((uint8_t *)raw_pg + 1))));
    }

    return 0;
out: 
    if(err == 2) {
        printf("Page full\n");
        return 0;
    }
    return -1;
}

int main() {
    int err, nerr=0;
    uint8_t expected, desired;
    uint16_t curr_sz;
    char msg[100];
    
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
        nerr++;
        printf("Expected Value in binary: " PRINTF_BINARY_PATTERN_INT8 "\n",
           PRINTF_BYTE_TO_BINARY_INT8(expected));
        goto out;
    }

    if (desired != 0b00011010) {
        nerr++;
        printf("Desired Value in binary: " PRINTF_BINARY_PATTERN_INT8 "\n",
           PRINTF_BYTE_TO_BINARY_INT8(desired));
        goto out;
    }
    if ((*(uint8_t *)raw_pg & 0xFF) != 0b00011010) {
        nerr++;
        printf("Desired Value in binary: " PRINTF_BINARY_PATTERN_INT8 "\n",
           PRINTF_BYTE_TO_BINARY_INT8(desired));
        goto out;
    }

    printf(DEVICE_NAME "Check and set flag test done\n");

    curr_sz = -1;
    /* 
     * Zero out bits for curr sz as they are garbage, 0x00F0 is interesting
     * and explained above.
     */
    (*(uint16_t *)raw_pg) = (*(uint16_t *)raw_pg) & 0x00F0;

    err = set_write_ptr(&curr_sz);
    if (err) {
        printf("Setting write ptr failed");
        nerr++;
        goto out;
    }
    if(curr_sz != 0) {
        nerr++;
        printf("curr sz should be zero for empty page, found = %d\n", curr_sz);
        goto out;
    }
    if (write_ptr != (raw_pg + MD_OFFSET)) {
        nerr++;
        printf("write ptr not set correctly");
        goto out;
    }

    printf(DEVICE_NAME "Setting write ptr Done\n");

    err = write_string_to_kernel_page();
    if (err) {
        printf("Writing string to kernel page failed");
        nerr++;
        goto out;
    }
    
    if (*(uint16_t *)raw_pg != 4608){
        printf("raw pg MD not updated properly \n");
        nerr++;
        goto out;
    }
    printf(DEVICE_NAME "Write to kernel page Done\n");

    memcpy(msg, ((char *)raw_pg + 2), sizeof(KERNEL_STR));
    printf("Message: %s\n", msg);

    free(raw_pg);

    err = test_mw();
    if (err) {
        printf("Multiple write failed \n");
        nerr++;
        goto out;
    }
    printf(DEVICE_NAME "Multiple Write to kernel page Done\n");

    free(raw_pg);

    goto success;
    
out:
    printf(" Unit test failed!");

success:
    if(nerr == 0)
        printf("Unit test passed");
}

