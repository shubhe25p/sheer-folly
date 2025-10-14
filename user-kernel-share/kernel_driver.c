#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#define DEVICE_NAME "kernel_driver"
#define PAGE_SIZE 4096

static int major;
static void *raw_pg;
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

static void dev_close(void) {
    return;
}

static int dev_mmap(struct file *file, struct vm_area_struct *vma) {
    unsigned long pfn, size;
    
    pfn = vmalloc_to_pfn(raw_pg);
    size = vma->vm_end - vma->vm_start;

    if(size > PAGE_SIZE)
        return -EINVAL;

    if(remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
        return -EAGAIN;

    uint8_t flag = 2;
    (*(uint8_t *)raw_pg) = flag;

    return 0;
}

static const file_operations fops {
    .owner = THIS_MODULE,
    .open = dev_open,
    .write = dev_write,
    .release = dev_release,
    .close = dev_close,
    .mmap = dev_mmap,
};

static int __init driver_module_init(void) {
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if(major < 0)
        return major;
    
    raw_pg = vmalloc(PAGE_SIZE);
    if(raw_pg == NULL) {
        unregister_chrdev(major, DEVICE_NAME);
        return -ENOMEM;
    }

    memset(raw_pg, 0, PAGE_SIZE);
    printk(KERN_INFO "Custom Kernel driver loaded: %d\n", major);
    return 0;
}

static void __exit driver_module_exit(void) {
    vfree(raw_pg);
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Custom Kernel driver unloaded");
}

MODULE_LICENSE("GPL");
module_init(driver_module_init);
module_exit(driver_module_exit);