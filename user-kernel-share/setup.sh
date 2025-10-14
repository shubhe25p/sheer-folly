## ONLY IF YOU ARE DUMB ENOUGH RUN THIS FILE DIRECTLY ##
## ONLY FOR REFERENCE ##

# USERSPACE

gcc user_driver.c -o user_driver

# KERNEL

sudo insmod kernel_driver_mod.ko
# LEGACY USE device_create API instead
sudo mknod /dev/kernel_driver c <major> <minor>
sudo chmod 666 /dev/kernel_driver
sudo rmmod kernel_driver_mod.ko

# KERNEL LOGS

sudo dmesg | tail -n 20