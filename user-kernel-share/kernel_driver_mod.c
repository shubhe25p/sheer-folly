#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/err.h>
#include <asm/cmpxchg.h>
#include <linux/kthread.h>

#define DEVICE_NAME "kernel_driver_mod"
#define PAGE_SIZE 4096
#define KERNEL_STR "Hello from Kernel\0"
#define MD_OFFSET 2

static int major;
static void *raw_pg, *write_ptr;
static struct task_struct *kthread_task = NULL;

static int dev_open(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t dev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    return 0;
}

static int dev_mmap(struct file *file, struct vm_area_struct *vma) {
    unsigned long pfn, size;
    
    pfn = vmalloc_to_pfn(raw_pg);
    size = vma->vm_end - vma->vm_start;

    if(size > PAGE_SIZE)
        return -EINVAL;

    if(remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
        return -EAGAIN;

    return 0;
}

/*
 * 2 bytes metadata: 4 bits - lock flag + 12 bits - page size filled
 * 4094 bytes data
 */
static void check_and_set_flag(void) {
    uint8_t expected, desired;
    do {

        expected = *(volatile uint8_t *)raw_pg;
        desired = 0x10 | (expected & 0x0F);

    } while (cmpxchg((uint8_t *)raw_pg, expected, desired) != expected);
}

static int set_write_ptr(void) {
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
    if (write_ptr + sizeof(KERNEL_STR) > raw_pg + PAGE_SIZE)
        return 2;

    return 0;
}

static void write_string_to_kernel_page(void) {
    uint16_t curr_sz;

    strscpy(write_ptr, KERNEL_STR, sizeof(KERNEL_STR));

    curr_sz = write_ptr + sizeof(KERNEL_STR) - raw_pg - MD_OFFSET;
    /*
     * If you want to know why this weird bit ops look above,
     * reading and write to a multi byte pointer will lead to
     * some weird behavior on little endian systems
     */
    (*(uint8_t *)raw_pg) = curr_sz >> 8;
    (*((uint8_t *)raw_pg + 1)) = (curr_sz & 0x00FF);
}

static int hello_kernel_fn(void *data) {
    printk(KERN_INFO "Kernel thread started.. \n");

    while (!kthread_should_stop()) {
        check_and_set_flag();
        int err = set_write_ptr();
        if (err)
            break;
        write_string_to_kernel_page();
    }

    printk(KERN_INFO "Kernel thread ended.. \n");
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .write = dev_write,
    .release = dev_release,
    .mmap = dev_mmap,
};

static int __init driver_module_init(void) {
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0)
        return major;
    
    raw_pg = vmalloc(PAGE_SIZE);
    if (raw_pg == NULL) {
        unregister_chrdev(major, DEVICE_NAME);
        return -ENOMEM;
    }

    kthread_task = kthread_run(hello_kernel_fn, NULL, "dumbest kernel thread alive");
    if (IS_ERR(kthread_task)) {
        vfree(raw_pg);
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ERR "Failed to start kernel thread: %ld\n", PTR_ERR(kthread_task));
        return PTR_ERR(kthread_task);
    }

    memset(raw_pg, 0, PAGE_SIZE);
    printk(KERN_INFO "Custom Kernel driver loaded: %d\n", major);
    return 0;
}

static void __exit driver_module_exit(void) {
    vfree(raw_pg);
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Custom Kernel driver unloaded\n");
}

MODULE_LICENSE("GPL");
module_init(driver_module_init);
module_exit(driver_module_exit);