#include <linux/module.h>
#include <linux/kernel.h>

int init_module() {
        printk(KERN_DEBUG "Hello world!\n");

        return 0;
}

void cleanup_module() {
}
