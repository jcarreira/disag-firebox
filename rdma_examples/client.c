
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

static int comp_handler_success = 0;
void comp_handler_send(struct ib_cq* cq, void* cq_context)
{
    struct ib_wc wc;
    printk(KERN_WARNING "COMP HANDLER\n");
    do {
	    while (ib_poll_cq(cq, 1, &wc)> 0) {
		    if (wc.status == IB_WC_SUCCESS) {
			    printk(KERN_WARNING "IB_WC_SUCCESS\n");
			    printk(KERN_WARNING "%s\n", s_ctx.rdma_recv_buffer);

                            comp_handler_success++;
                            
		    } else {
			    printk(KERN_WARNING "FAILURE %d\n", wc.status);
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

int modify_qp(void)
{
    int retval;

    struct ib_qp_attr attr;
    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IB_QPS_INIT;
    attr.pkey_index = 0;
    attr.port_num = 1;
    attr.qp_access_flags = IB_ACCESS_REMOTE_WRITE  | IB_ACCESS_REMOTE_READ| IB_ACCESS_REMOTE_ATOMIC;//0;

    printk(KERN_INFO "Going to INIT..\n");
    retval = ib_modify_qp(s_ctx.qp, &attr, IB_QP_STATE | IB_QP_PKEY_INDEX | IB_QP_PORT | IB_QP_ACCESS_FLAGS);
    CHECK(retval == 0);

    printk(KERN_INFO "Preparing for RTR. mtu: %d rem_qpn: %d rem_psn: %d rem_lid: %d\n",
           s_ctx.active_mtu, s_ctx.rem_qpn, s_ctx.rem_psn, s_ctx.rem_lid);
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IB_QPS_RTR;
    attr.path_mtu = s_ctx.active_mtu;
    attr.dest_qp_num = s_ctx.rem_qpn;
    attr.rq_psn = s_ctx.rem_psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 12;
    //attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = s_ctx.rem_lid;
    attr.ah_attr.sl = 0; // service level
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = 1;
   
    printk(KERN_INFO "Going to RTR..\n");
    retval = ib_modify_qp(s_ctx.qp, &attr,
		    IB_QP_STATE | 
                    IB_QP_AV | 
                    IB_QP_PATH_MTU | 
                    IB_QP_DEST_QPN | 
                    IB_QP_RQ_PSN | 
                    IB_QP_MAX_DEST_RD_ATOMIC |
                    IB_QP_MIN_RNR_TIMER);
    if(retval) {
       printk(KERN_INFO "RTR failed ret: %d..\n", retval);
    }
    CHECK(retval == 0);

    attr.qp_state = IB_QPS_RTS;
    attr.timeout = 14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 6;
    attr.sq_psn = s_ctx.psn;
    attr.max_rd_atomic = 1;
    
    printk(KERN_INFO "Going to RTS..\n");
    retval = ib_modify_qp(s_ctx.qp, &attr, IB_QP_STATE | IB_QP_TIMEOUT | 
                          IB_QP_RETRY_CNT | IB_QP_RNR_RETRY | IB_QP_SQ_PSN | IB_QP_MAX_QP_RD_ATOMIC);

    CHECK(retval == 0);
    return 0;
}

static int prepare_qp(void)
{
    s_ctx.mr = ib_get_dma_mr(s_ctx.pd, IB_ACCESS_REMOTE_READ | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE);
    CHECK(s_ctx.mr != 0);

    s_ctx.rkey = s_ctx.mr->rkey;
    
    return modify_qp();
}

static int
send_rdma_write(char* data_to_send, int len)
{
    struct ib_sge sg;
    struct ib_send_wr wr;
    struct ib_send_wr *bad_wr;
    u64 dma_addr;

    char *data = kmalloc(len, GFP_KERNEL);
    CHECK(data != 0);
    strcpy(data, data_to_send);

    dma_addr = ib_dma_map_single(s_ctx.device, data, len, DMA_BIDIRECTIONAL);
    CHECK(ib_dma_mapping_error(s_ctx.device, dma_addr) == 0);

    printk(KERN_INFO "Setting sg..\n");
    memset(&sg, 0, sizeof(sg));
    sg.addr  = (uintptr_t)dma_addr;
    sg.length = len;
    sg.lkey  = s_ctx.mr->lkey;

    printk(KERN_INFO "Working on IB_WR_RDMA_WRITE wr..\n");
    memset(&wr, 0, sizeof(wr));
    wr.wr_id      = (uintptr_t)&s_ctx;
    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = IB_WR_RDMA_WRITE;
    wr.send_flags = IB_SEND_SIGNALED;
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

static int
do_some_rdma_kungfu(void)
{
    struct ib_sge sg;
    struct ib_send_wr wr;
    struct ib_send_wr *bad_wr;
    u64 dma_addr;

    s_ctx.rdma_recv_buffer = kmalloc(500, GFP_KERNEL);
    CHECK(s_ctx.rdma_recv_buffer != 0);
    strcpy(s_ctx.rdma_recv_buffer, "HELLO WORLD");

    s_ctx.mr = ib_get_dma_mr(s_ctx.pd, IB_ACCESS_REMOTE_READ | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE);
    CHECK(s_ctx.mr != 0);

    s_ctx.rkey = s_ctx.mr->rkey;

    dma_addr = ib_dma_map_single(s_ctx.device, s_ctx.rdma_recv_buffer, 500, DMA_BIDIRECTIONAL);
    CHECK(ib_dma_mapping_error(s_ctx.device, dma_addr) == 0);
    //s_ctx.mr = ib_reg_mr(s_ctx.pd, rdma_recv_buffer, 500, 
    //           IB_ACCESS_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE | IB_ACCESS_REMOTE_READ);

    modify_qp();

    printk(KERN_INFO "Setting sg..\n");
#define DO_RDMA_WRITE
//#define DO_RDMA_READ
#ifdef DO_RDMA_READ
    memset(&sg, 0, sizeof(sg));
    sg.addr     = (uintptr_t)dma_addr;//rdma_recv_buffer;
    sg.length   = 500;
    sg.lkey     = s_ctx.mr->lkey;

    printk(KERN_INFO "Working on IB_WR_RDMA_READ wr..\n");
    memset(&wr, 0, sizeof(wr));
    wr.wr_id      = (uintptr_t)&s_ctx;//0;
    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = IB_WR_RDMA_READ;
    wr.send_flags = IB_SEND_SIGNALED; //0
    wr.wr.rdma.remote_addr = s_ctx.rem_vaddr;
    wr.wr.rdma.rkey        = s_ctx.rem_rkey;
#elif defined(DO_RDMA_WRITE)
    memset(&sg, 0, sizeof(sg));
    sg.addr  = (uintptr_t)dma_addr;
    sg.length = 500;
    sg.lkey  = s_ctx.mr->lkey;

    printk(KERN_INFO "Working on IB_WR_RDMA_WRITE wr..\n");
    memset(&wr, 0, sizeof(wr));
    wr.wr_id      = (uintptr_t)&s_ctx;
    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = IB_WR_RDMA_WRITE;
    wr.send_flags = IB_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = s_ctx.rem_vaddr;
    wr.wr.rdma.rkey        = s_ctx.rem_rkey;
#else
    memset(&sg, 0, sizeof(sg));
    sg.addr  = (uintptr_t)dma_addr;//rdma_recv_buffer;
    sg.length = 500;
    sg.lkey  = s_ctx.mr->lkey;

    printk(KERN_INFO "Working on IB_WR_SEND wr..\n");
    memset(&wr, 0, sizeof(wr));
    wr.wr_id      = (uintptr_t)&s_ctx;
    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = IB_WR_SEND;
    wr.send_flags = IB_SEND_SIGNALED;
#endif

    printk(KERN_INFO "Posting send..\n");
    if (ib_post_send(s_ctx.qp, &wr, &bad_wr)) {
	    printk(KERN_INFO "Error posting send..\n");
	    return -1;
    }
    printk(KERN_INFO "Send posted..\n");
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
    
    printk(KERN_WARNING "Get port data\n");
    retval = ib_query_port(s_ctx.device,1,&attr);
    CHECK2(retval == 0);
    CHECK_MSG2(attr.active_mtu == 5, "!!!!!!!!Wrong device!!!!!!");

    s_ctx.lid = attr.lid;
    s_ctx.qpn = s_ctx.qp->qp_num;

    get_random_bytes(&s_ctx.psn, sizeof(s_ctx.psn));
    s_ctx.psn &= 0xffffff;
    s_ctx.active_mtu = attr.active_mtu;

    ib_query_gid(s_ctx.device, 1, 0, &s_ctx.gid, &gid_attr);
} 

void add_device2(struct ib_device* dev) {
    printk(KERN_WARNING "We got a new device V2!\n");
}

static int devices_seen = 0;
void add_device(struct ib_device* dev) {
    char str[] = "HELLO WORLD";

    devices_seen++;
    printk(KERN_WARNING "We got a new device! %d\n ", devices_seen);

    if (devices_seen < 2) {
        printk(KERN_WARNING "Enough devices. Returning\n");
        return;
    }

    // We care abou the second device
    // The first one is ethernet
    printk(KERN_WARNING "Installing device\n");

    s_ctx.device = mlnx_device = dev;

    // get device attrs
    ib_query_device(dev, &mlnx_device_attr); 

    print_device_attr(mlnx_device_attr);
    
    INIT_IB_EVENT_HANDLER(&ieh, dev, async_event_handler);
    ib_register_event_handler(&ieh);

    s_ctx.pd = ib_alloc_pd(dev);
    CHECK_MSG2(s_ctx.pd != 0, "Error creating pd");

    s_ctx.send_cq = ib_create_cq(dev, comp_handler_send, cq_event_handler_send, NULL, 10, 0);
    s_ctx.recv_cq = ib_create_cq(dev, comp_handler_recv, cq_event_handler_recv, NULL, 10, 0);
    CHECK_MSG2(s_ctx.send_cq != 0, "Error creating CQ");
    CHECK_MSG2(s_ctx.recv_cq != 0, "Error creating CQ");

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

    printk(KERN_INFO "Start client module.\n");

    CHECK_MSG2(connect() == 0, "Error connecting");

    // Get some useful data from port
    get_port_data(); 
    handshake(); 

    prepare_qp();

    send_rdma_write(str, strlen(str));
    //do_some_rdma_kungfu();

    while (comp_handler_success == 0)
        ;
}

void remove_device(struct ib_device* dev) {
    printk(KERN_WARNING "remove_device\n ");
}

struct ib_client my_client;
static int __init client_module_init( void ) {

    my_client.name = "DISAG_MEM";
    my_client.add = add_device;
    my_client.remove = remove_device;

    ib_register_client(&my_client);
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

