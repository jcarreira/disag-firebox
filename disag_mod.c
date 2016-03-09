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
#include <linux/kthread.h>  // for threads
#include <linux/delay.h>

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
//#include <rdma/ib_user_verbs.h>
//#include <rdma/ib_user_verbs_exp.h>

//struct ibv_mr *send_msg_mr;
//struct ibv_mr *recv_msg_mr;

#define BUFFER_SIZE 1024*1024*10 // 10MB

static void launch2(void);
struct sockaddr_in sock_addr;
struct ibv_device *ib_dev;
struct ibv_device **ib_dev_list;
u8 IP[] = {10,10,49,83}; //10.10.49.84
int retval;
struct rdma_cm_event *event = NULL;
struct rdma_event_channel *ec;
struct rdma_conn_param cm_params;
struct ib_qp_init_attr qp_attr;
void* cq_context;
struct rdma_cm_id* rcma_id;
static struct task_struct *thread1;
char our_thread[8]="thread1";

void on_route_resolved(struct rdma_cm_id *id);

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
   printk("event\n");
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
     case RDMA_CM_EVENT_ADDR_RESOLVED:
             printk("RDMA_CM_EVENT_ADDR_RESOLVED\n");
             launch2();
             break;
     case RDMA_CM_EVENT_ADDR_ERROR:
             printk("RDMA_CM_EVENT_ADDR_ERROR\n");
             break;
     case RDMA_CM_EVENT_ROUTE_RESOLVED:
             printk("RDMA_CM_EVENT_ROUTE_RESOLVED\n");
             on_route_resolved(cma_id);
             break;
     case RDMA_CM_EVENT_ROUTE_ERROR:
             printk("RDMA_CM_EVENT_ROUTE_ERROR\n");
             break;
     case RDMA_CM_EVENT_CONNECT_RESPONSE:
             printk("RDMA_CM_EVENT_CONNECT_RESPONSE\n");
             break;
     case RDMA_CM_EVENT_MULTICAST_JOIN:
             printk("RDMA_CM_EVENT_MULTICAST_JOIN\n");
             break;
     case RDMA_CM_EVENT_MULTICAST_ERROR:
             printk("RDMA_CM_EVENT_MULTICAST_ERROR\n");
             break;
     default:
             printk("DEFAULT\n");
             break;
   }
   return 0;
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
    printk(KERN_WARNING "COMP HANDLER\n");
}

void cq_event_handler(struct ib_event* ib_e, void* v)
{
    printk(KERN_WARNING "CQ HANDLER\n");
}

void build_qp_attr(struct ib_qp_init_attr* qp_attr)
{
   printk(KERN_WARNING "build_qp_attr\n");
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
   
   //struct ib_mr *recv_mr;
   //struct ib_mr *send_mr;
   struct ib_mr *mr;
   u64 recv_mr;
   u64 send_mr;
   
   char* recv_region;
   char* send_region;

   int num_completions;
};

void on_route_resolved(struct rdma_cm_id *id)
{
    memset(&cm_params, 0, sizeof(cm_params)); 
    printk("connecting...\n");
    retval = rdma_connect(id, &cm_params);
    CHECK(retval == 0, "rdma_connect did not succeed");
    
    printk("connect successful\n");

    rdma_destroy_id(rcma_id);
}

struct connection* conn;

void register_memory(struct connection* conn)
{
    conn->mr = ib_get_dma_mr(s_ctx.pd, IB_ACCESS_LOCAL_WRITE | 
                            IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ);
    if (conn->mr == 0) {
	    printk(KERN_WARNING "error ib_get_dma_mr\n");
	    return;
    }

    printk(KERN_WARNING "conn->mr->lkey: %d\n", conn->mr->lkey);
    printk(KERN_WARNING "kmalloc1\n");
    conn->send_region = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    printk(KERN_WARNING "kmalloc2\n");
    conn->recv_region = kmalloc(BUFFER_SIZE, GFP_KERNEL);

    //conn->send_mr = ib_alloc_mr(s_ctx.pd, conn->send_region, BUFFER_SIZE, IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE);
    //conn->recv_mr = ib_reg_mr(s_ctx.pd, conn->recv_region, BUFFER_SIZE, IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE);
    
    //conn->send_mr = ib_alloc_mr(s_ctx.pd, IB_MR_TYPE_MEM_REG, 10);//??????
    //conn->recv_mr = ib_alloc_mr(s_ctx.pd, IB_MR_TYPE_MEM_REG, 10);//??????

    printk(KERN_WARNING "dma_map_single\n");
    conn->send_mr = ib_dma_map_single(rcma_id->device, conn->send_region, BUFFER_SIZE, DMA_BIDIRECTIONAL);
    conn->recv_mr = ib_dma_map_single(rcma_id->device, conn->recv_region, BUFFER_SIZE, DMA_BIDIRECTIONAL);

    //printk(KERN_WARNING "ret1: %d\n", ib_dma_mapping_error(rcma_id->device, conn->send_mr));
    //printk(KERN_WARNING "ret1: %d\n", ib_dma_mapping_error(rcma_id->device, conn->recv_mr));

    CHECK(conn->send_mr != 0, "send_mr wrong");
    CHECK(conn->recv_mr != 0, "recv_mr wrong");
}
void post_receives(struct connection* conn)
{
    struct ib_recv_wr wr, *bad_wr;
    struct ib_sge sg;
    int ret;
    
    printk(KERN_WARNING "post_receives\n");

    memset(&sg, 0, sizeof(sg));
    sg.addr = (uintptr_t)conn->recv_region;
    sg.length = BUFFER_SIZE;
    sg.lkey = conn->mr->lkey;
    
    memset(&wr, 0, sizeof(wr));
    wr.next = NULL;
    wr.wr_id = (uintptr_t)conn;
    wr.sg_list = &sg;
    wr.num_sge = 1;

    printk(KERN_WARNING "going for ib_post_recv\n");
    ret = ib_post_recv(conn->qp, &wr, &bad_wr);

    if (ret) {
       printk(KERN_WARNING "ib_post_recv failed\n");
    }
    printk(KERN_WARNING "ib_post_recv success\n");
    return;
}

static void launch3(void)
{
}

static void on_completion(struct ib_wc* wc)
{
  struct connection *conn = (struct connection *)(uintptr_t)wc->wr_id;

  if (wc->status != IB_WC_SUCCESS) {
    printk(KERN_WARNING "on_completion: status is not IBV_WC_SUCCESS.\n");
    return;
  }

  if (wc->opcode & IB_WC_RECV)
    printk(KERN_WARNING "received message: %s\n", conn->recv_region);
  else if (wc->opcode == IB_WC_SEND)
    printk(KERN_WARNING "send completed successfully.\n");
  else
    printk(KERN_WARNING "on_completion: completion isn't a send or a receive.");

//  if (++conn->num_completions == 2)
//    rdma_disconnect(conn->id);
}

int poll_cq_thread_fn(void *ctx)
{
  struct ib_wc wc;
  int ret;
  printk(KERN_WARNING "poll_cq_thread_fn pd\n");
  //struct ibv_cq *cq;
  //struct ibv_wc wc;

  while (1) {
	  //ib_get_cq_event(s_ctx->comp_channel, &cq, &ctx);
	  //ib_ack_cq_events(cq, 1);
          //printk(KERN_WARNING "doing ib_req_notify_cq\n");
	  retval = ib_req_notify_cq(s_ctx.cq, 0);
          //printk(KERN_WARNING "ib_req_notify_cq done\n");
	  CHECK(retval == 0, "ib_req_notify_cq_2 did not succeed");

          //printk(KERN_WARNING "calling ib_poll_cq\n");
	  //while (ib_poll_cq(s_ctx.cq, 1, &wc))
	  ret = ib_poll_cq(s_ctx.cq, 1, &wc);
          //{
          //printk(KERN_WARNING "calling on_completion ib_poll_cq ret: %d\n", ret);
	  on_completion(&wc);
          msleep(1000);
	  //}
	  //printk(KERN_WARNING "restarting loop\n");
  }
  return 0;
}

static void launch2(void)
{
    // ON addr resolved
    printk(KERN_WARNING "allocating pd\n");
    s_ctx.pd = ib_alloc_pd(rcma_id->device);
    CHECK(s_ctx.pd != 0, "ib_alloc_pd did not succeed");
    printk(KERN_WARNING "allocated pd\n");

    s_ctx.cq = ib_create_cq(rcma_id->device, comp_handler, cq_event_handler, NULL, 10, 0);
    printk(KERN_WARNING "created cq\n");
    CHECK(s_ctx.cq != 0, "ib_create_cq did not succeed");

    retval = ib_req_notify_cq(s_ctx.cq, 0);
    CHECK(retval == 0, "ib_req_notify_cq did not succeed");
    printk(KERN_WARNING "req notify success\n");

    thread1 = kthread_create(poll_cq_thread_fn,NULL,our_thread);
    if(thread1)
    {
        printk(KERN_WARNING "creating thread\n");
        wake_up_process(thread1);
    } else {
        printk(KERN_WARNING "error in thread\n");
        return;
    }

    build_qp_attr(&qp_attr);
    printk(KERN_WARNING "build_qp_attr success\n");
    
    rcma_id->context = conn = (void*)kmalloc(sizeof(struct connection), GFP_KERNEL);

    retval = rdma_create_qp(rcma_id, s_ctx.pd, &qp_attr);
    //conn->qp = ib_create_qp(s_ctx.pd, &qp_attr);
    CHECK(retval == 0, "rdma_create_qp did not succeed");
    //CHECK(conn->qp == 0, "rdma_create_qp did not succeed");
    printk(KERN_WARNING "rdma_create_qp success\n");

    printk(KERN_WARNING "kmalloc success\n");
   
    conn->id = rcma_id;
    conn->qp = rcma_id->qp;
    conn->num_completions = 0;

    printk(KERN_WARNING "returning\n");

    register_memory(conn);
    printk(KERN_WARNING "register_memory success\n");
    post_receives(conn); 

    printk(KERN_WARNING "going for rdma_resolve_route\n");
    retval = rdma_resolve_route(rcma_id, 500);
    CHECK(retval == 0, "rdma_resolve_route did not succeed\n");
    
    printk(KERN_WARNING "rdma_resolve_route success\n");
}

static void launch(void) 
{
    rcma_id = rdma_create_id(event_handler, NULL, RDMA_PS_TCP, IB_QPT_RC);    

    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(create_address(IP));
    sock_addr.sin_port = htons(1025);

    retval = rdma_resolve_addr(rcma_id, NULL, (struct sockaddr*)&sock_addr,500);

    CHECK(retval == 0, "rdma_resolve_addr return did not succeed");
    printk(KERN_WARNING "Resolved address\n");
    
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


