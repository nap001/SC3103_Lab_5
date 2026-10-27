/**
 * hello_lkm.c - SC3103 Practical Exercise #5 (Exercise B1)
 *
 * Description:
 *   Simple Loadable Kernel Module (LKM) for Raspberry Pi.
 *   Prints a greeting message upon insertion (insmod) and a departure message
 *   upon removal (rmmod) via printk() to kernel ring buffer / dmesg.
 *
 * References:
 *   - Lab Manual Appendix B
 *   - Lecture W10L4 Slides 32-38
 *
 * Demo Steps on RPi or Ubuntu:
 *   1. Build:   make
 *   2. Load:    sudo insmod hello_lkm.ko
 *   3. Check:   dmesg | tail          -> should show "Hello from kernel world"
 *   4. List:    lsmod | grep hello
 *   5. Info:    modinfo hello_lkm.ko
 *   6. Unload:  sudo rmmod hello_lkm
 *   7. Check:   dmesg | tail          -> should show "Goodbye from kernel world"
 */

#include <linux/init.h>   // Macros module_init and module_exit, __init and __exit
#include <linux/module.h> // Core headers for loading LKMs into the kernel
#include <linux/kernel.h> // printk() and log levels (KERN_ALERT, etc.)

/**
 * hello_init() - The LKM initialization function
 *
 * Called when the module is loaded via insmod.
 * Return: 0 on success, negative error code on failure.
 */
static int __init hello_init(void)
{
    printk(KERN_ALERT "Hello from kernel world\n");
    return 0;
}

/**
 * hello_exit() - The LKM cleanup function
 *
 * Called when the module is removed via rmmod.
 */
static void __exit hello_exit(void)
{
    printk(KERN_ALERT "Goodbye from kernel world\n");
}

// Register the module entry and exit points
module_init(hello_init);
module_exit(hello_exit);

// Metadata declarations (visible via modinfo)
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CE3103");
MODULE_DESCRIPTION("Simple Hello module");
MODULE_VERSION("V1");
