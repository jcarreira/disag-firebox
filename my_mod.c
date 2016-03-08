#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/init.h> 

//#include <infiniband/verbs.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/anon_inodes.h>
#include <linux/slab.h>

#ifdef IB_VERBS_H
#error "error"
#endif

#include <asm/uaccess.h>

//#include <rdma/ib_umem_odp.h>

#include <linux/kref.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/cdev.h>

//#include <rdma/ib_verbs.h>
#include "/usr/src/mlnx-ofed-kernel-3.1/include/rdma/ib_verbs.h"
#include "/usr/src/mlnx-ofed-kernel-3.1/include/rdma/ib_verbs_exp.h"
//#include <rdma/ib_verbs_exp.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_user_verbs_exp.h>

struct ibv_mr *send_msg_mr;
struct ibv_mr *recv_msg_mr;


/*
 *  tcp_client_connect
 * ********************
 *	Creates a connection to a TCP server 
 */
static int tcp_client_connect(struct app_data *data)
{
    struct addrinfo *res, *t;
    struct addrinfo hints = {
        .ai_family		= AF_UNSPEC,
        .ai_socktype	= SOCK_STREAM
    };

    char *service;
    int n;
    int sockfd = -1;
    struct sockaddr_in sin;

    TEST_N(asprintf(&service, "%d", data->port),
            "Error writing port-number to port-string");

    TEST_N(getaddrinfo(data->servername, service, &hints, &res),
            "getaddrinfo threw error");

    for(t = res; t; t = t->ai_next){
        TEST_N(sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol),
                "Could not create client socket");

        TEST_N(connect(sockfd,t->ai_addr, t->ai_addrlen),
                "Could not connect to server");	
    }

    freeaddrinfo(res);

    return sockfd;
}

void launch() {

    TEST_Z(ctx = init_ctx(&data),
            "Could not create ctx, init_ctx");
    set_local_ib_connection(ctx, &data);
    data.sockfd = tcp_client_connect(&data);
}

static int hello_init(void) 
{
    printk(KERN_WARNING "Hello, world \n ");

    launch();

    return 0; 
} 

static void hello_exit(void)
{
    printk(KERN_INFO "Goodbye, world \n"); 
}

module_init(hello_init);
module_exit(hello_exit);


