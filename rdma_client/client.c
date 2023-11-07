
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/un.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#include "log.h"

#define SIZE 500
#define MAX  100

struct socket *sock = NULL;

#define CHECK_MSG2(arg, msg) {\
   if ((arg) == 0){\
      printk(KERN_INFO msg);\
      return;\
   }\
}

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

#define CHECK2(arg) {\
   if ((arg) == 0){\
      printk(KERN_INFO "Error.\n");\
      return;\
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
   struct ib_mr *mr;
   int active_mtu; 
   int lid;
   int qpn;
   int psn;
   int qp_psn;
   uint32_t rkey;
   union ib_gid gid;
   
   unsigned long long int rem_vaddr;
   uint32_t rem_rkey;
   
   int rem_qpn;
   int rem_psn;
   int rem_lid;

   char* rdma_recv_buffer;
   u64 dma_addr;
} s_ctx;

void print_device_attr(struct ib_device_attr dev_attr)
{
   LOG_KERN(LOG_INFO, ("fw_ver: %llu\n"
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
           dev_attr.max_pd, dev_attr.max_qp_rd_atom, dev_attr.max_ee_rd_atom, dev_attr.max_res_rd_atom));
}

void async_event_handler(struct ib_event_handler* ieh, struct ib_event *ie)
{
    LOG_KERN(LOG_INFO, ("async_event_handler\n"));
}

void print_port_info(struct ib_port_attr port_attr)
{
   
   LOG_KERN(LOG_INFO, ("Port 1 info\npkey_tbl_len: %d\n"
           "lid: %d\n"
           "sm_lid: %d\n"
           "active_speed: %d\n"
           "active_width: %d\n",
           port_attr.pkey_tbl_len, port_attr.lid, port_attr.sm_lid, 
           port_attr.active_speed, port_attr.active_width));
}

void get_port_info(struct ib_device *dev)
{
    ib_query_port(dev, 1, &port_attr);
    print_port_info(port_attr);
}

void comp_handler_send(struct ib_cq* cq, void* cq_context)
{
    struct ib_wc wc;
    LOG_KERN(LOG_INFO, ("COMP HANDLER\n"));
    do {
	    while (ib_poll_cq(cq, 1, &wc)> 0) {
		    if (wc.status == IB_WC_SUCCESS) {
			    LOG_KERN(LOG_INFO, ("IB_WC_SUCCESS\n"));
			    LOG_KERN(LOG_INFO, ("%s\n", s_ctx.rdma_recv_buffer));
                            
		    } else {
			    LOG_KERN(LOG_INFO, ("FAILURE %d\n", wc.status));
		    }
	    }
    } while (ib_req_notify_cq(cq, IB_CQ_NEXT_COMP |
			    IB_CQ_REPORT_MISSED_EVENTS) > 0);
}

void comp_handler_recv(struct ib_cq* cq, void* cq_context)
{
	LOG_KERN(LOG_INFO, ("COMP HANDLER\n"));
}

void cq_event_handler_send(struct ib_event* ib_e, void* v)
{
    LOG_KERN(LOG_INFO, ("CQ HANDLER\n"));
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
    iov.iov_base = data;
    iov.iov_len = size;
    iov_iter_init(&msg.msg_iter, READ, &iov, 1, size);
    msg.msg_control  = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags    = 0;

    printk(KERN_INFO "Sending data..\n");
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = sock_sendmsg(sock, &msg);

    set_fs(oldfs);
}

static void receive_data(char* data, int size) {
    struct msghdr msg;
    struct iovec iov;
    int retval;
    mm_segment_t oldfs;
    
    printk(KERN_INFO "receive_data\n");
    
    iov.iov_base = data;
    iov.iov_len = size;
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    //msg.msg_name = &servaddr;
    //msg.msg_namelen = sizeof(struct sockaddr_in);
    iov_iter_init(&msg.msg_iter, READ, &iov, 1, size);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    
    printk(KERN_INFO "Receving data..\n");

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = sock_recvmsg(sock, &msg, 0);

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

int modify_qp(void)
{
    int retval;

    struct ib_qp_attr attr;
    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IB_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = 1;
    attr.qp_access_flags = IB_ACCESS_REMOTE_WRITE  | IB_ACCESS_REMOTE_READ |
                           IB_ACCESS_REMOTE_ATOMIC;

    LOG_KERN(LOG_INFO, ("Going to INIT..\n"));
    retval = ib_modify_qp(s_ctx.qp, &attr, IB_QP_STATE | IB_QP_PKEY_INDEX | 
                                           IB_QP_PORT | IB_QP_ACCESS_FLAGS);
    CHECK(retval == 0);

    LOG_KERN(LOG_INFO, ("Preparing for RTR. mtu: %d rem_qpn: %d rem_psn: %d rem_lid: %d\n",
           s_ctx.active_mtu, s_ctx.rem_qpn, s_ctx.rem_psn, s_ctx.rem_lid));

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IB_QPS_RTR;
    attr.path_mtu = s_ctx.active_mtu;
    attr.dest_qp_num = s_ctx.rem_qpn;
    attr.rq_psn = s_ctx.rem_psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12;
    attr.ah_attr.dlid = s_ctx.rem_lid;
    attr.ah_attr.sl = 0; // service level
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = 1;
   
    LOG_KERN(LOG_INFO, ("Going to RTR..\n"));
    retval = ib_modify_qp(s_ctx.qp, &attr,
		    IB_QP_STATE | 
                    IB_QP_AV | 
                    IB_QP_PATH_MTU | 
                    IB_QP_DEST_QPN | 
                    IB_QP_RQ_PSN | 
                    IB_QP_MAX_DEST_RD_ATOMIC |
                    IB_QP_MIN_RNR_TIMER);
    if(retval) {
       LOG_KERN(LOG_INFO, ("RTR failed ret: %d..\n", retval));
    }
    CHECK(retval == 0);

    attr.qp_state = IB_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 6;
    attr.sq_psn = s_ctx.psn;
    attr.max_rd_atomic = 1;
    
    LOG_KERN(LOG_INFO, ("Going to RTS..\n"));
    retval = ib_modify_qp(s_ctx.qp, &attr, IB_QP_STATE | IB_QP_TIMEOUT | 
                          IB_QP_RETRY_CNT | IB_QP_RNR_RETRY | IB_QP_SQ_PSN | IB_QP_MAX_QP_RD_ATOMIC);

    CHECK(retval == 0);
    return 0;
}

static int
rdma_setup(void)
{

    // create receive buffer
    s_ctx.rdma_recv_buffer = kmalloc(500, GFP_KERNEL);
    CHECK(s_ctx.rdma_recv_buffer != 0);

    // create memory region
    s_ctx.mr = ib_get_dma_mr(s_ctx.pd, IB_ACCESS_REMOTE_READ | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE);
    CHECK(s_ctx.mr != 0);

    s_ctx.rkey = s_ctx.mr->rkey;

    // get dma_addr
    s_ctx.dma_addr = ib_dma_map_single(s_ctx.device, s_ctx.rdma_recv_buffer, 500, DMA_BIDIRECTIONAL);
    CHECK(ib_dma_mapping_error(s_ctx.device, s_ctx.dma_addr) == 0);

    // modify QP until RTS
    modify_qp();
    return 0;
}

void handshake(void)
{
    char data[500];
    unsigned long long int vaddr = 0;

    receive_data(data, 500);
    printk(KERN_WARNING "data received: %s\n", data);
    
    sscanf(data, "%016Lx:%u:%x:%x:%x", &s_ctx.rem_vaddr, &s_ctx.rem_rkey, 
           &s_ctx.rem_qpn, &s_ctx.rem_psn, &s_ctx.rem_lid);
    printk(KERN_INFO "rem_vaddr: %llu rem_rkey:%u rem_qpn:%d rem_psn:%d rem_lid:%d\n", 
           s_ctx.rem_vaddr, s_ctx.rem_rkey, s_ctx.rem_qpn, s_ctx.rem_psn, s_ctx.rem_lid);

    sprintf(data, "%016Lx:%u:%x:%x:%x", 
            vaddr, s_ctx.rkey, s_ctx.qpn, s_ctx.psn, s_ctx.lid);
    send_data(data, strlen(data));
}

void get_port_data(void)
{
    struct ib_port_attr attr;
    struct ib_gid_attr gid_attr;
    int retval;
    
    LOG_KERN(LOG_INFO, ("Get port data\n"));
    retval = ib_query_port(s_ctx.device,1,&attr);
    CHECK2(retval == 0);
    CHECK_MSG2(attr.active_mtu == 5, "Error: Wrong device");

    s_ctx.lid = attr.lid;
    s_ctx.qpn = s_ctx.qp->qp_num;

    get_random_bytes(&s_ctx.psn, sizeof(s_ctx.psn));
    s_ctx.psn &= 0xffffff;
    s_ctx.active_mtu = attr.active_mtu;

    ib_query_gid(s_ctx.device, 1, 0, &s_ctx.gid, &gid_attr);
} 

void add_device2(struct ib_device* dev)
{
    printk(KERN_WARNING "We got a new device V2!\n");
}

static int devices_seen = 0;

int is_second_device(void)
{
    devices_seen++;
    if (devices_seen == 2) {
        return 1;
    } else {
        return 0;
    }
}

int post_send_wr(void)
{
    struct ib_sge sg;
    struct ib_rdma_wr rdma_wr;
    struct ib_send_wr *bad_wr;
    LOG_KERN(LOG_INFO, ("Setting sg..\n"));
#define DO_RDMA_READ
#ifdef DO_RDMA_READ
    memset(&sg, 0, sizeof(sg));
    sg.addr     = (uintptr_t)s_ctx.dma_addr;//rdma_recv_buffer;
    sg.length   = 500;
    sg.lkey     = s_ctx.mr->lkey;

    LOG_KERN(LOG_INFO, ("Working on IB_WR_RDMA_READ wr..\n"));
    memset(&rdma_wr, 0, sizeof(rdma_wr));
    rdma_wr.wr.wr_id      = (uintptr_t)&s_ctx;//0;
    rdma_wr.wr.sg_list    = &sg;
    rdma_wr.wr.num_sge    = 1;
    rdma_wr.wr.opcode     = IB_WR_RDMA_READ;
    rdma_wr.wr.send_flags = IB_SEND_SIGNALED; //0
    rdma_wr.remote_addr = s_ctx.rem_vaddr;
    rdma_wr.rkey        = s_ctx.rem_rkey;
#else
    memset(&sg, 0, sizeof(sg));
    sg.addr  = (uintptr_t)s_ctx.dma_addr;//rdma_recv_buffer;
    sg.length = 500;
    sg.lkey  = s_ctx.mr->lkey;

    LOG_KERN(LOG_INFO, "Working on IB_WR_SEND wr..\n");
    memset(&rdma_wr, 0, sizeof(rdma_wr));
    rdma_wr.wr.wr_id      = (uintptr_t)&s_ctx;
    rdma_wr.wr.sg_list    = &sg;
    rdma_wr.wr.num_sge    = 1;
    rdma_wr.wr.opcode     = IB_WR_SEND;
    rdma_wr.wr.send_flags = IB_SEND_SIGNALED;
#endif

    LOG_KERN(LOG_INFO, ("Posting send..\n"));
    if (ib_post_send(s_ctx.qp, &rdma_wr.wr, &bad_wr)) {
	    printk(KERN_INFO "Error posting send..\n");
	    return -1;
    }
    LOG_KERN(LOG_INFO, ("Send posted..\n"));

    return 0;
}

void add_device(struct ib_device* dev)
{
    LOG_KERN(LOG_INFO, ("We got a new device! %d\n ", devices_seen));

    if (!is_second_device())
        return;

    // We care abou the second device
    // The first one is ethernet
    LOG_KERN(LOG_INFO, ("Installing device\n"));

    s_ctx.device = mlnx_device = dev;

    // get device attrs
    ib_query_device(dev, &mlnx_device_attr); 

    print_device_attr(mlnx_device_attr);
   
    // register handler 
    INIT_IB_EVENT_HANDLER(&ieh, dev, async_event_handler);
    ib_register_event_handler(&ieh);

    s_ctx.pd = __ib_alloc_pd(dev, 0, KBUILD_MODNAME);
    CHECK_MSG2(s_ctx.pd != 0, "Error creating pd");

    // create a attribute structure and pass the same to create cq
    struct ib_cq_init_attr cq_attr = {};
    cq_attr.cqe = 10;
    
    s_ctx.send_cq = __ib_create_cq(dev, comp_handler_send, cq_event_handler_send, NULL, &cq_attr, KBUILD_MODNAME);
    s_ctx.recv_cq = __ib_create_cq(dev, comp_handler_recv, cq_event_handler_recv, NULL, &cq_attr, KBUILD_MODNAME);
    CHECK_MSG2(s_ctx.send_cq != 0, "Error creating CQ");
    CHECK_MSG2(s_ctx.recv_cq != 0, "Error creating CQ");

    // request notifications
    CHECK2(ib_req_notify_cq(s_ctx.recv_cq, IB_CQ_NEXT_COMP) == 0);
    CHECK2(ib_req_notify_cq(s_ctx.send_cq, IB_CQ_NEXT_COMP) == 0);

    // initialize qp_attr
    memset(&s_ctx.qp_attr, 0, sizeof(struct ib_qp_init_attr));
    s_ctx.qp_attr.send_cq = s_ctx.send_cq;
    s_ctx.qp_attr.recv_cq = s_ctx.recv_cq;
    s_ctx.qp_attr.cap.max_send_wr  = 10;
    s_ctx.qp_attr.cap.max_recv_wr  = 10;
    s_ctx.qp_attr.cap.max_send_sge = 1;
    s_ctx.qp_attr.cap.max_recv_sge = 1;
    s_ctx.qp_attr.cap.max_inline_data = 0;
    s_ctx.qp_attr.qp_type = IB_QPT_RC;
    s_ctx.qp_attr.sq_sig_type = IB_SIGNAL_ALL_WR;

    s_ctx.qp = ib_create_qp(s_ctx.pd, &s_ctx.qp_attr);
    CHECK_MSG2(s_ctx.qp != 0, "Error creating QP");

    LOG_KERN(LOG_INFO, ("Start client module.\n"));

    CHECK_MSG2(connect() == 0, "Error connecting");

    // Get some useful data from port
    get_port_data(); 

    // exchange data to bootstrap RDMA
    handshake(); 

    // create memory region
    // modify QP to RTS
    rdma_setup();

    // post a send request
    post_send_wr();
}

void remove_device(struct ib_device* dev)
{
    LOG_KERN(LOG_INFO, ("remove_device\n "));
}

struct ib_client my_client;
static int __init client_module_init(void)
{
    my_client.name = "DISAG_MEM";
    my_client.add = add_device;
    my_client.remove = remove_device;

    ib_register_client(&my_client);

    while(1);
    return 0;
}

static void __exit client_module_exit( void )
{
    sock_release(sock);
    ib_unregister_event_handler(&ieh);
    ib_unregister_client(&my_client);
    LOG_KERN(LOG_INFO, ("Exit client module.\n"));
}

module_init( client_module_init );
module_exit( client_module_exit );
MODULE_LICENSE("GPL");

