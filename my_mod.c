#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/init.h> 

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

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#include <asm/uaccess.h>

#include <linux/kref.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/cdev.h>

//#include <rdma/ib_verbs.h>
//#include "/usr/src/linux-headers-3.13.0-32/include/rdma/ib_verbs.h"
//#include "/usr/src/linux-headers-3.13.0-32/include/rdma/ib_verbs_exp.h"
//#include "/usr/src/mlnx-ofed-kernel-3.1/include/rdma/ib_verbs_exp.h"
//#include <rdma/ib_verbs_exp.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_user_verbs.h>
//#include <rdma/ib_user_verbs_exp.h>

//struct ibv_mr *send_msg_mr;
//struct ibv_mr *recv_msg_mr;

#define BUFFER_SIZE 1024*1024*10 // 10MB

u32 create_address(u8 *ip) {

   u32 addr = 0;
   int i;
   for (i = 0; i < 4; ++i) {
      addr += ip[i];
      if (i==3) {
         break;
      }
      addr <<= 8;
   }
   return addr;
}

// CHECK fails if condition (a) is not true
#define CHECK(a,b) {\
   if ((a) == 0) {\
    printk(KERN_INFO b); \
   }\
}
      

/*
 *  tcp_client_connect
 * ********************
 *	Creates a connection to a TCP server 
 */
/*
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

*/

static int
event_handler(struct rdma_cm_id* cma_id, struct rdma_cm_event* event) {
   switch (event->event) {
     case RDMA_CM_EVENT_CONNECT_REQUEST:
             printk("RDMA_CM_EVENT_CONNECT_REQUEST\n");
             break;
     case RDMA_CM_EVENT_ESTABLISHED:
             printk("RDMA_CM_EVENT_ESTABLISHED\n");
             break;
     case RDMA_CM_EVENT_ADDR_CHANGE:    /* FALLTHRU */
     case RDMA_CM_EVENT_DISCONNECTED:   /* FALLTHRU */
     case RDMA_CM_EVENT_DEVICE_REMOVAL: /* FALLTHRU */
     case RDMA_CM_EVENT_TIMEWAIT_EXIT:  /* FALLTHRU */
             printk("RDMA_CM_EVENT_TIMEWAIT_EXIT\n");
             break;
     case RDMA_CM_EVENT_REJECTED:       /* FALLTHRU */
     case RDMA_CM_EVENT_UNREACHABLE:    /* FALLTHRU */
     case RDMA_CM_EVENT_CONNECT_ERROR:
             printk("RDMA_CM_EVENT_CONNECT_ERROR\n");
             break;
     default:
             printk("DEFAULT\n");
             break;
   }
   return -1;
}

struct context {
   struct ibv_context *ctx;
   struct ib_pd* pd;
   struct ib_cq *cq;
   //struct ibv_comp_channel* comp_channel;
};

struct context s_ctx;

void comp_handler(struct ib_cq* cq, void* cq_context)
{

}

void cq_event_handler(struct ib_event* ib_e, void* v)
{

}

void build_qp_attr(struct ib_qp_init_attr* qp_attr)
{
   memset(qp_attr, 0, sizeof(struct ib_qp_init_attr));
   qp_attr->send_cq = s_ctx.cq;
   qp_attr->recv_cq = s_ctx.cq;
   qp_attr->qp_type = IB_QPT_RC;
   
   qp_attr->cap.max_send_wr = 10;
   qp_attr->cap.max_recv_wr = 10;
   qp_attr->cap.max_send_sge = 10;
   qp_attr->cap.max_recv_sge = 10;
}

struct connection {
   struct rdma_cm_id *id;
   struct ib_qp *qp;
   
   struct ib_mr *recv_mr;
   struct ib_mr *send_mr;
   
   char* recv_region;
   char* send_region;

   int num_completions;
};

struct connection* conn;

void register_memory(struct connection* conn)
{
    //conn->send_region = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    //conn->recv_region = kmalloc(BUFFER_SIZE, GFP_KERNEL);

    //conn->send_mr = ib_alloc_mr(s_ctx.pd, conn->send_region, BUFFER_SIZE, IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE);
    //conn->recv_mr = ib_reg_mr(s_ctx.pd, conn->recv_region, BUFFER_SIZE, IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE);
    
    //conn->send_mr = ib_alloc_mr(s_ctx.pd, IB_MR_TYPE_MEM_REG, 10);//??????
    //conn->recv_mr = ib_alloc_mr(s_ctx.pd, IB_MR_TYPE_MEM_REG, 10);//??????

    CHECK(conn->send_mr == 0, "send_mr wrong");
    CHECK(conn->recv_mr == 0, "recv_mr wrong");
}
void post_receives(struct connection* conn)
{
}

static void launch(void) 
{
    struct sockaddr_in sock_addr;
    struct ibv_device *ib_dev;
    struct ibv_device **ib_dev_list;
    u8 IP[] = {127,0,0,1};
    int retval;

    struct rdma_cm_event *event = NULL;
    struct rdma_event_channel *ec;
    struct rdma_conn_param cm_params;
    struct ib_qp_init_attr qp_attr;
    //struct ib_cq_init_attr cq_attr;
    void* cq_context;

    struct rdma_cm_id* rcma_id;
    rcma_id = rdma_create_id(event_handler, NULL, RDMA_PS_TCP, IB_QPT_RC);    

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(create_address(IP));
    sock_addr.sin_port = htons(1025);

    retval = rdma_resolve_addr(rcma_id, NULL, (struct sockaddr*)&sock_addr,500);

    CHECK(retval == 0, "rdma_resolve_addr return did not succeed");
    printk(KERN_WARNING "Resolved address\n");

    
    memset(&cm_params, 0, sizeof(cm_params)); 

    // ON addr resolved
    //s_ctx.ctx = rcma_id->verbs;     
    s_ctx.pd = ib_alloc_pd(rcma_id->device);
    //s_ctx.comp_channel = ib_create_comp_channel(rcma_id->device);
    s_ctx.cq = ib_create_cq(rcma_id->device, comp_handler, cq_event_handler, NULL, 10, 0xFFFF);

    retval = ib_req_notify_cq(s_ctx.cq, 0);
    CHECK(retval == 0, "ib_req_notify_cq did not succeed");

    build_qp_attr(&qp_attr);

    retval = rdma_create_qp(rcma_id, s_ctx.pd, &qp_attr);
    CHECK(retval == 0, "rdma_create_qp did not succeed");

    rcma_id->context = conn = (void*)kmalloc(sizeof(struct connection), GFP_KERNEL);
   
    conn->id = rcma_id;
    conn->qp = rcma_id->qp;
    conn->num_completions = 0;

    register_memory(conn);
    post_receives(conn); 

    retval = rdma_resolve_route(rcma_id, 500);
    CHECK(retval == 0, "rdma_resolve_route did not succeed");
    retval = rdma_connect(rcma_id, &cm_params);
    CHECK(retval == 0, "rdma_connect did not succeed");

    rdma_destroy_id(rcma_id);
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
MODULE_LICENSE("GPL");


