
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
#define MEM_SIZE 10000

static int __init main_module_init(void)
{
    int retval;
    rdma_ctx_t ctx;
    rdma_request req;
    char *mem1, *mem2;
    u64 mem1_addr, mem2_addr;

    retval = rdma_library_init();
    
    if (retval == 0) {
        LOG_KERN(LOG_INFO, ("RDMA_LIB_INIT SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA_LIB_INIT FAILED"));
        return -1;
    }

    while(!rdma_library_ready())
        ;

    ctx = rdma_init(100, "127.0.0.1", 18515);

    if (ctx != NULL) {
        LOG_KERN(LOG_INFO, ("RDMA_INIT SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA_INIT FAILED"));
        return -1;
    }

    mem1 = kmalloc(MEM_SIZE, GFP_KERNEL);
    mem2 = kmalloc(MEM_SIZE, GFP_KERNEL);

    mem1_addr = rdma_map_address(mem1, MEM_SIZE);
    mem2_addr = rdma_map_address(mem2, MEM_SIZE);

    strcpy(mem1, "HELLO WORLD");
    req.rw = RDMA_WRITE;
    req.dma_addr = mem1_addr;
    req.remote_offset = 0;
    req.length = 20;
    LOG_KERN(LOG_INFO, ("Launching write op"));
    retval = rdma_op(ctx, &req, 1);
    if (retval == 0) {
        LOG_KERN(LOG_INFO, (" RDMA WRITE SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA WRITE FAILED"));
        return -1;
    }

    strcpy(mem2, "WRONG DATA"); 
    req.rw = RDMA_READ;
    req.dma_addr = mem2_addr;
    req.remote_offset = 0;
    req.length = 20;
    LOG_KERN(LOG_INFO, ("Launching read op"));
    retval = rdma_op(ctx, &req, 1);
    if (retval == 0) {
        LOG_KERN(LOG_INFO, (" RDMA READ SUCCESS: %s", mem2));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA READ FAILED"));
        return -1;
    }

    strcpy(mem1, "HERE WE GO AGAIN");
    req.rw = RDMA_WRITE;
    req.dma_addr = mem1_addr;
    req.remote_offset = 0;
    req.length = 20;
    LOG_KERN(LOG_INFO, ("Launching write op"));
    retval = rdma_op(ctx, &req, 1);
    if (retval == 0) {
        LOG_KERN(LOG_INFO, (" RDMA WRITE SUCCESS"));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA WRITE FAILED"));
        return -1;
    }
    
    strcpy(mem2, "PLAIN WRONG"); 
    req.rw = RDMA_READ;
    req.dma_addr = mem2_addr;
    req.remote_offset = 0;
    req.length = 20;
    LOG_KERN(LOG_INFO, ("Launching read op"));
    retval = rdma_op(ctx, &req, 1);
    if (retval == 0) {
        LOG_KERN(LOG_INFO, (" RDMA READ SUCCESS: %s", mem2));
    }
    else  {
        LOG_KERN(LOG_INFO, ("RDMA READ FAILED"));
        return -1;
    }

    return 0;
}

static void __exit main_module_exit( void )
{
    rdma_library_exit();
}

module_init( main_module_init );
module_exit( main_module_exit );
MODULE_LICENSE("GPL");

