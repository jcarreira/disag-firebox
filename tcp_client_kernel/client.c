
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/un.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/module.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#define SIZE 500
#define MAX  100

struct socket *sock = NULL;

#define CHECK_MSG(arg, msg) {\
   if ((arg) == 0){\
      printk(KERN_INFO msg);\
      return(-1);\
   }\
}

#define CHECK(arg) {\
   if ((arg) == 0){\
      printk(KERN_INFO "Error.\n");\
      return(-1);\
   }\
}

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

struct ib_device* mlnx_device;
struct ib_device_attr mlnx_device_attr;
struct ib_event_handler ieh;
struct ib_port_attr port_attr;

struct context {
   struct ib_device* device;
   struct ib_cq* send_cq, *recv_cq;
   struct ib_pd* pd;
   struct ib_qp* qp;
   struct ib_qp_init_attr qp_attr;
   unsigned long long int rem_vaddr;
   unsigned int rem_rkey;
   struct ib_mr *mr;
} s_ctx;

void print_device_attr(struct ib_device_attr dev_attr)
{
   printk(KERN_WARNING "fw_ver: %llu\n"
           "sys_image_guid: %llu\n"
           "max_mr_size: %llu\n"
           "page_size_cap: %llu\n"
           "vendor_id: %d\n"
           "vendor_part_id: %d\n"
           "hw_ver: %d\n"
           "max_qp: %d\n"
           "max_qp_wr: %d\n"
           "device_cap_flags: %llu\n"
           "max_sge: %d\n"
           "max_sge_rd: %d\n"
           "max_cq: %d\n"
           "max_cqe: %d\n"
           "max_mr: %d\n"
           "max_pd: %d\n"
           "max_qp_rd_atom: %d\n"
           "max_ee_rd_atom: %d\n"
           "max_res_rd_atom: %d\n",
	   dev_attr.fw_ver, dev_attr.sys_image_guid, dev_attr. max_mr_size,
           dev_attr.page_size_cap, dev_attr.vendor_part_id, dev_attr.vendor_part_id,
           dev_attr.hw_ver, dev_attr.max_qp, dev_attr.max_qp_wr, dev_attr.device_cap_flags,
           dev_attr.max_sge, dev_attr.max_sge_rd, dev_attr.max_cq, dev_attr.max_mr, dev_attr.max_mr,
           dev_attr.max_pd, dev_attr.max_qp_rd_atom, dev_attr.max_ee_rd_atom, dev_attr.max_res_rd_atom);
            //%lu %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d
            //%d %d

}


void async_event_handler(struct ib_event_handler* ieh, struct ib_event *ie)
{
    printk(KERN_WARNING "async_event_handler\n");
}

void print_port_info(struct ib_port_attr port_attr)
{
   
   printk(KERN_WARNING "Port 1 info\npkey_tbl_len: %d\n"
           "lid: %d\n"
           "sm_lid: %d\n"
           "active_speed: %d\n"
           "active_width: %d\n",
           port_attr.pkey_tbl_len, port_attr.lid, port_attr.sm_lid, 
           port_attr.active_speed, port_attr.active_width);
}

void get_port_info(struct ib_device *dev)
{
    ib_query_port(dev, 1, &port_attr);
    print_port_info(port_attr);
}

void comp_handler_send(struct ib_cq* cq, void* cq_context)
{
    struct ib_wc wc;
    printk(KERN_WARNING "COMP HANDLER\n");
    do {
	    while (ib_poll_cq(cq, 1, &wc)> 0) {
		    if (wc.status == IB_WC_SUCCESS) {
			    printk(KERN_WARNING "IB_WC_SUCCESS\n");
		    } else {
			    printk(KERN_WARNING "FAILURE\n");
		    }
	    }
    } while (ib_req_notify_cq(cq, IB_CQ_NEXT_COMP |
			    IB_CQ_REPORT_MISSED_EVENTS) > 0);
}

void comp_handler_recv(struct ib_cq* cq, void* cq_context)
{
	printk(KERN_WARNING "COMP HANDLER\n");
}

void cq_event_handler_send(struct ib_event* ib_e, void* v)
{
    printk(KERN_WARNING "CQ HANDLER\n");
}
void cq_event_handler_recv(struct ib_event* ib_e, void* v)
{
    printk(KERN_WARNING "CQ HANDLER\n");
}

struct sockaddr_in servaddr;
static int connect(void)
{
    //int PORT = 15000;
    int PORT = 18515;
    int retval;
    //u8 IP[] = {10,10,40,83};//10.10.49.83
    u8 IP[] = {127,0,0,1};
    //u8 IP[] = {10,10,49,91}; // 10.10.49.91 f9

    // create
    retval = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock); 
    CHECK_MSG(retval == 0, "Error creating socket");

    // connect
    //int PORT = 1025;
    memset(&servaddr, 0, sizeof(servaddr));  
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = htonl(create_address(IP));

    CHECK(retval == 0);
    CHECK(sock);
    CHECK(sock->ops->connect);

    printk(KERN_INFO "connecting to 127.0.0.1\n");
    retval = sock->ops->connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr), 0);
    printk(KERN_INFO "connected retval: %d\n", retval);
    CHECK(retval == 0);

    return 0;
}

/*void exchange_data(char* data, int len)
{
    struct msghdr msg;
    struct iovec iov;
    char my_data[SIZE];
    int retval;
    mm_segment_t oldfs;
    
    printk(KERN_INFO "Exchanging data\n");

    msg.msg_name     = 0;
    msg.msg_namelen  = 0;
    msg.msg_iov      = &iov;
    msg.msg_iovlen   = 1;
    msg.msg_control  = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags    = 0;
    msg.msg_iov->iov_len = len;
    msg.msg_iov->iov_base = data;

    printk(KERN_INFO "Sending data..\n");
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = sock_sendmsg(sock, &msg, len);

    set_fs(oldfs);

    msg.msg_name = 0;
    msg.msg_namelen = 0;
    //msg.msg_name = &servaddr;
    //msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    msg.msg_iov->iov_base= my_data;
    msg.msg_iov->iov_len = SIZE;
    
    printk(KERN_INFO "Receving data..\n");
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = sock_recvmsg(sock, &msg, SIZE, 0);

    set_fs(oldfs);
    
    printk(KERN_INFO "Exchange done..\n");
}
*/

static void send_data(char* data, int size) {
    struct msghdr msg;
    struct iovec iov;
    int retval;
    mm_segment_t oldfs;
    
    printk(KERN_INFO "Exchanging data\n");

    msg.msg_name     = 0;
    msg.msg_namelen  = 0;
    msg.msg_iov      = &iov;
    msg.msg_iovlen   = 1;
    msg.msg_control  = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags    = 0;
    msg.msg_iov->iov_len = size;
    msg.msg_iov->iov_base = data;

    printk(KERN_INFO "Sending data..\n");
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = sock_sendmsg(sock, &msg, size);

    set_fs(oldfs);
}

static void receive_data(char* data, int size) {
    struct msghdr msg;
    struct iovec iov;
    int retval;
    mm_segment_t oldfs;
    
    printk(KERN_INFO "receive_data\n");
    
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    //msg.msg_name = &servaddr;
    //msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    msg.msg_iov->iov_base= data;
    msg.msg_iov->iov_len = size;
    
    printk(KERN_INFO "Receving data..\n");

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = sock_recvmsg(sock, &msg, size, 0);

    set_fs(oldfs);
}

/*int setup_rdma()
{  
   
   s_ctx.pd = ibv_alloc_pd(s_ctx.context);
   CHECK_MSG(s_ctx.pd != 0, "Error gettign pd");
   
   char* buf = (char*)malloc(SIZE);
   CHECK_MSG(buf != 0, "Error getting buf");
   s_ctx.mr = ibv_reg_mr(s_ctx.pd, buf, SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
   CHECK_MSG(s_ctx.mr != NULL, "Error getting mr");
   
   s_ctx.send_cq = ibv_create_cq(s_ctx.context, 100, NULL, 0, 0);
   CHECK_MSG(s_ctx.send_cq != 0, "Error getting cq");
   s_ctx.recv_cq = ibv_create_cq(s_ctx.context, 100, NULL, 0, 0);
   CHECK_MSG(s_ctx.recv_cq != 0, "Error getting recv cq");
   
   struct ibv_qp_init_attr qp_attr;
   memset(&qp_attr, 0, sizeof(struct ibv_qp_init_attr));
   qp_attr.send_cq = s_ctx.send_cq;
   qp_attr.recv_cq = s_ctx.recv_cq;
   qp_attr.cap.max_send_wr  = 1;
   qp_attr.cap.max_recv_wr  = 1;
   qp_attr.cap.max_send_sge = 1;
   qp_attr.cap.max_recv_sge = 1;
   qp_attr.cap.max_inline_data = 0;
   qp_attr.qp_type = IBV_QPT_RC;
   
   s_ctx.qp = ibv_create_qp(s_ctx.pd, &qp_attr);
   CHECK_MSG(s_ctx.qp != NULL, "Error getting qp");
   
   puts("Moving QP to init");
*/
static int
do_some_rdma_kungfu(char*data)
{
    struct ib_sge sg;
    struct ib_send_wr wr;
    struct ib_send_wr *bad_wr;
    u64 dma_addr;

    char* rdma_recv_buffer = kmalloc(500, GFP_KERNEL);
    CHECK(rdma_recv_buffer != 0);


    s_ctx.mr = ib_get_dma_mr(s_ctx.pd, IB_ACCESS_REMOTE_READ | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE);
    CHECK(s_ctx.mr != 0);

    dma_addr = ib_dma_map_single(s_ctx.device, rdma_recv_buffer, 500, DMA_BIDIRECTIONAL);
    CHECK(ib_dma_mapping_error(s_ctx.device, dma_addr) == 0);
    //s_ctx.mr = ib_reg_mr(s_ctx.pd, rdma_recv_buffer, 500, 
    //           IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ);

    sscanf(data, "%016Lx:%08x", &s_ctx.rem_vaddr, &s_ctx.rem_rkey);
    printk(KERN_INFO "rem_vaddr: %llu rem_rkey:%u\n", s_ctx.rem_vaddr, s_ctx.rem_rkey);

    memset(&sg, 0, sizeof(sg));
    
    printk(KERN_INFO "Setting sg..\n");
    sg.addr     = (uintptr_t)rdma_recv_buffer;
    sg.length     = 500;
    sg.lkey     = s_ctx.mr->lkey;

    printk(KERN_INFO "Working on wr..\n");
    memset(&wr, 0, sizeof(wr));
    wr.wr_id      = 0;
    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = IB_WR_RDMA_READ;
    wr.send_flags = 0;
    wr.wr.rdma.remote_addr = s_ctx.rem_vaddr;
    wr.wr.rdma.rkey        = s_ctx.rem_rkey;

    printk(KERN_INFO "Posting send..\n");
    if (ib_post_send(s_ctx.qp, &wr, &bad_wr)) {
	    printk(KERN_INFO "Error posting send..\n");
	    return -1;
    }
    printk(KERN_INFO "Send posted..\n");
    return 0;
}

static int devices_seen = 0;
void add_device(struct ib_device* dev) {
    char recv_data[100];

    printk(KERN_WARNING "We got a new device! %d\n ", devices_seen);

    if (++devices_seen > 1) {
        printk(KERN_WARNING "Enough devices. Returning\n");
        return;
    }

    s_ctx.device = mlnx_device = dev;

    ib_query_device(dev, &mlnx_device_attr); 

    print_device_attr(mlnx_device_attr);
    
    INIT_IB_EVENT_HANDLER(&ieh, dev, async_event_handler);
    ib_register_event_handler(&ieh);

    s_ctx.pd = ib_alloc_pd(dev);
    CHECK_MSG(s_ctx.pd != 0, "Error creating pd");

    s_ctx.send_cq = ib_create_cq(dev, comp_handler_send, cq_event_handler_send, NULL, 10, 0);
    s_ctx.recv_cq = ib_create_cq(dev, comp_handler_recv, cq_event_handler_recv, NULL, 10, 0);
    CHECK_MSG(s_ctx.send_cq != 0, "Error creating CQ");
    CHECK_MSG(s_ctx.recv_cq != 0, "Error creating CQ");

    
    CHECK(ib_req_notify_cq(s_ctx.recv_cq, IB_CQ_NEXT_COMP) == 0);
    CHECK(ib_req_notify_cq(s_ctx.send_cq, IB_CQ_NEXT_COMP) == 0)

    // initialize qp_attr
    memset(&s_ctx.qp_attr, 0, sizeof(struct ib_qp_init_attr));
    s_ctx.qp_attr.send_cq = s_ctx.send_cq;
    s_ctx.qp_attr.recv_cq = s_ctx.recv_cq;
    s_ctx.qp_attr.cap.max_send_wr  = 1;
    s_ctx.qp_attr.cap.max_recv_wr  = 1;
    s_ctx.qp_attr.cap.max_send_sge = 1;
    s_ctx.qp_attr.cap.max_recv_sge = 1;
    s_ctx.qp_attr.cap.max_inline_data = 0;
    s_ctx.qp_attr.qp_type = IB_QPT_RC;
    s_ctx.qp_attr.sq_sig_type = IB_SIGNAL_ALL_WR;

    s_ctx.qp = ib_create_qp(s_ctx.pd, &s_ctx.qp_attr);

    printk(KERN_INFO "Start client module.\n");

    CHECK_MSG(connect() == 0, "Error connecting");
  
    printk(KERN_WARNING "receiving vaddr:rkey\n");
    receive_data(recv_data, 100);
    printk(KERN_WARNING "data received: %s\n", recv_data);
    printk(KERN_WARNING "Sending OK\n");
    send_data("OK", 2);
    printk(KERN_WARNING "Sent OK\n");

    do_some_rdma_kungfu(recv_data);

    //while (1) {
    //   if(ib_peek_cq(s_ctx.recv_cq, 1) > 0) 
    //           printk(KERN_WARNING "YES\n");
    //   if(ib_peek_cq(s_ctx.send_cq, 1) > 0)
    //           printk(KERN_WARNING "YES\n");
    //}
}

void remove_device(struct ib_device* dev) {
    printk(KERN_WARNING "remove\n ");
}

struct ib_client my_client;
static int __init client_module_init( void ) {

    my_client.name = "MY NAME";
    my_client.add = add_device;
    my_client.remove = remove_device;

    ib_register_client(&my_client);
    // this is hopefully going to add_device
    return 0;
}

static void __exit client_module_exit( void )
{
    sock_release(sock);
    ib_unregister_event_handler(&ieh);
    ib_unregister_client(&my_client);
    printk(KERN_INFO "Exit client module.\n");
}

module_init( client_module_init );
module_exit( client_module_exit );
MODULE_LICENSE("GPL");

