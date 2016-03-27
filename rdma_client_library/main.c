
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/un.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/module.h>
#include <linux/delay.h>

#include "rdma_library.h"
#include "log.h"

#define PORT 18515

static int __init main_module_init(void)
{
    int retval;
    rdma_ctx_t ctx;
    rdma_request req;
    char *mem;

    retval = rdma_library_init();
    
    if (retval == 0) {
        LOG_KERN(LOG_INFO, ("RDMA_LIB_INIT SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA_LIB_INIT FAILED"));
    }

    while(!rdma_library_ready())
        ;

    ctx = rdma_init(100, "127.0.0.1", 18515);

    if (ctx != NULL) {
        LOG_KERN(LOG_INFO, ("RDMA_INIT SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA_INIT FAILED"));
    }

    mem = kmalloc(10000, GFP_KERNEL);

#if 0
    req.rw = RDMA_READ;
    req.local = mem;
    req.remote_offset = 0;
    req.length = 10;
    LOG_KERN(LOG_INFO, ("Launching read op"));
    retval = rdma_op(ctx, &req, 1);
    if (retval == 0) {
        LOG_KERN(LOG_INFO, (" RDMA READ SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA READ FAILED"));
    }
#else
    strcpy(mem, "HELLO WORLD");
    req.rw = RDMA_WRITE;
    req.local = mem;
    req.remote_offset = 0;
    req.length = 20;
    LOG_KERN(LOG_INFO, ("Launching write op"));
    retval = rdma_op(ctx, &req, 1);
    if (retval == 0) {
        LOG_KERN(LOG_INFO, (" RDMA WRITE SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA WRITE FAILED"));
    }
#endif
    

    return 0;
}

static void __exit main_module_exit( void )
{
    rdma_library_exit();
}

module_init( main_module_init );
module_exit( main_module_exit );
MODULE_LICENSE("GPL");

