#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include <sys/wait.h>
#include <signal.h>
#include <infiniband/verbs.h>

#define CHECK(cond) {\
   if((cond)==0) {\
      puts("Error");\
      exit(-1);\
   }\
}

#define CHECK_MSG(cond, msg) {\
   if((cond)==0) {\
      puts(msg);\
      exit(-1);\
   }\
}

#define MYPORT 18515

time_t timer;
int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
struct sockaddr_in my_addr; // my address information
struct sockaddr_in their_addr; // connector.s address information
int sin_size;
int yes=1;

struct context {
   struct ibv_context* context;
   struct ibv_pd* pd;
   struct ibv_mr* mr;
   struct ibv_qp* qp;
   struct ibv_cq* send_cq;
   struct ibv_cq* recv_cq;
   struct ibv_comp_channel *event_channel;

   int active_mtu;   
   uint32_t local_rkey;  // rkey to be sent to client
   union ibv_gid gid;
   int qpn;
   int psn;
   int lid;

   int rem_qpn;
   int rem_psn;
   int rem_lid;
   unsigned long long int rem_vaddr;
   uint32_t rem_rkey;

   char* rdma_buffer;
  
   unsigned long rdma_mem_size; 
} s_ctx;

static 
void handshake_get_memsize(void)
{
    char recv_buffer[1000];

    int retval = recv(new_fd, recv_buffer, 1000, 0);
    CHECK(retval > 0);
    
    sscanf(recv_buffer, "%lu", &s_ctx.rdma_mem_size);
    CHECK_MSG(s_ctx.rdma_mem_size > 0, "Error: received wrong mem size");
}

static
void exchange_bootstrap_data(void* virtual_address, uint32_t rkey, int qpn, int psn, int lid)
{
    puts("exchange_bootstrap_data starting");
    // The server needs to send the rkey
    // and the virtual address
    char data_to_send[1000];
    memset(data_to_send, 0, 1000);

    sprintf(data_to_send, "%016Lx:%u:%x:%x:%x", (unsigned long long int)virtual_address, rkey, qpn, psn, lid);
    printf("sending. vaddr: %lld rkey: %u qpn: %d psn:%d lid:%d\n",
           (unsigned long long int)virtual_address, rkey, qpn, psn, lid);

    int retval = send(new_fd, data_to_send, strlen(data_to_send), 0);
    
    printf("sent data: %s\n", data_to_send);

    char recv_buffer[1000];
    memset(recv_buffer, 0, 1000);

    retval = recv(new_fd, recv_buffer, 1000, 0);
    CHECK(retval > 0);
    
    printf("received: %s\n", recv_buffer);

    sscanf(recv_buffer, "%016Lx:%u:%x:%x:%x", &s_ctx.rem_vaddr, 
           &s_ctx.rem_rkey, &s_ctx.rem_qpn, &s_ctx.rem_psn, &s_ctx.rem_lid);
}

int get_port_data()
{
    struct ibv_port_attr attr;
    int retval = ibv_query_port(s_ctx.context,1,&attr);
    CHECK(retval == 0);
    CHECK_MSG(attr.active_mtu == 5, "!!!!!!!!Wrong device!!!!!!");

    s_ctx.lid = attr.lid;
    s_ctx.qpn = s_ctx.qp->qp_num;

    long int rand_value = rand();//lrand48();
    s_ctx.psn = rand_value & 0xffffff;
    printf("Random value: %ld psn: %d\n", rand_value, s_ctx.psn);

    s_ctx.local_rkey = s_ctx.mr->rkey;
    s_ctx.active_mtu = attr.active_mtu;

    ibv_query_gid(s_ctx.context, 1, 0, &s_ctx.gid);

    return 0;
}

void handshake()
{
    exchange_bootstrap_data(s_ctx.rdma_buffer, s_ctx.local_rkey, s_ctx.qpn, s_ctx.psn, s_ctx.lid);
}

int setup_rdma_2()
{
   puts("Moving to RTR");
   printf("mtu: %d qpn: %d psn: %d lid: %d\n",
          s_ctx.active_mtu, s_ctx.rem_qpn, s_ctx.rem_psn, s_ctx.rem_lid);

   struct ibv_qp_attr attr;
   memset(&attr, 0, sizeof(attr));
   attr.qp_state = IBV_QPS_RTR;
   attr.path_mtu = s_ctx.active_mtu;
   attr.dest_qp_num = s_ctx.rem_qpn;
   attr.rq_psn	    = s_ctx.rem_psn;
   attr.max_dest_rd_atomic = 1;
   attr.min_rnr_timer = 12;
   attr.ah_attr.is_global = 0;
   attr.ah_attr.dlid = s_ctx.rem_lid;
   attr.ah_attr.sl = 0;
   attr.ah_attr.src_path_bits = 0;
   attr.ah_attr.port_num = 1;

   CHECK(ibv_modify_qp(s_ctx.qp, &attr,
			   IBV_QP_STATE              |
			   IBV_QP_AV                 |
			   IBV_QP_PATH_MTU           |
			   IBV_QP_DEST_QPN           |
			   IBV_QP_RQ_PSN             |
			   IBV_QP_MAX_DEST_RD_ATOMIC |
			   IBV_QP_MIN_RNR_TIMER) == 0);

   puts("Moving to RTS");
   memset(&attr, 0, sizeof(attr));

   attr.qp_state = IBV_QPS_RTS;
   attr.sq_psn	 = s_ctx.psn;
   attr.timeout	 = 14;
   attr.retry_cnt = 7;
   attr.rnr_retry = 7; /* infinite */
   attr.max_rd_atomic  = 1;

   CHECK (ibv_modify_qp(s_ctx.qp, &attr,
			   IBV_QP_STATE              |
			   IBV_QP_TIMEOUT            |
			   IBV_QP_RETRY_CNT          |
			   IBV_QP_RNR_RETRY          |
			   IBV_QP_SQ_PSN             |
			   IBV_QP_MAX_QP_RD_ATOMIC) == 0);

   puts("Moved to RTS");

#define DO_RECEIVE_WR
#ifdef DO_RECEIVE_WR
   puts("Building receive WR");

   struct ibv_sge sg;
   struct ibv_recv_wr wr;
   struct ibv_recv_wr *bad_wr;

   memset(&sg, 0, sizeof(sg));
   sg.addr	  = (uintptr_t)s_ctx.rdma_buffer;
   sg.length      = s_ctx.rdma_mem_size;
   sg.lkey	  = s_ctx.mr->lkey;

   memset(&wr, 0, sizeof(wr));
   wr.wr_id      = 0;
   wr.sg_list    = &sg;
   wr.num_sge    = 1;

   puts("Ibv_post_recv");
   if (ibv_post_recv(s_ctx.qp, &wr, &bad_wr)) {
	   fprintf(stderr, "Error, ibv_post_recv() failed\n");
	   return -1;
   }
#endif
}

int setup_rdma_1()
{
   struct ibv_device **dev_list;
   struct ibv_device *ibv_dev;

   int num_devices;
   dev_list = ibv_get_device_list(&num_devices);

   ibv_dev = dev_list[0];
   CHECK_MSG(ibv_dev != NULL, "Error getting device");

   s_ctx.context = ibv_open_device(ibv_dev);
   CHECK_MSG(s_ctx.context != NULL, "Error getting cntext");

   s_ctx.pd = ibv_alloc_pd(s_ctx.context);
   CHECK_MSG(s_ctx.pd != 0, "Error gettign pd");

   s_ctx.rdma_buffer = (char*)malloc(s_ctx.rdma_mem_size);
   strcpy(s_ctx.rdma_buffer, "AHOY!");
   CHECK_MSG(s_ctx.rdma_buffer != 0, "Error getting buf");
   s_ctx.mr = ibv_reg_mr(s_ctx.pd, s_ctx.rdma_buffer, s_ctx.rdma_mem_size, 
                            IBV_ACCESS_LOCAL_WRITE | 
                            IBV_ACCESS_REMOTE_WRITE | 
                            IBV_ACCESS_REMOTE_READ);
   CHECK_MSG(s_ctx.mr != NULL, "Error getting mr");

   printf("Created my. rkey: %u\n", s_ctx.mr->rkey);
   
   // create channel
   s_ctx.event_channel = ibv_create_comp_channel(s_ctx.context);
   
   s_ctx.send_cq = ibv_create_cq(s_ctx.context, 100, s_ctx.event_channel, 0, 0); 
   CHECK_MSG(s_ctx.send_cq != 0, "Error getting cq");
   s_ctx.recv_cq = ibv_create_cq(s_ctx.context, 100, s_ctx.event_channel, 0, 0); 
   CHECK_MSG(s_ctx.recv_cq != 0, "Error getting recv cq");

   struct ibv_qp_init_attr qp_attr;
   memset(&qp_attr, 0, sizeof(struct ibv_qp_init_attr));
   qp_attr.send_cq = s_ctx.send_cq;
   qp_attr.recv_cq = s_ctx.recv_cq;
   qp_attr.cap.max_send_wr  = 10;
   qp_attr.cap.max_recv_wr  = 10;
   qp_attr.cap.max_send_sge = 1;
   qp_attr.cap.max_recv_sge = 1;
   qp_attr.cap.max_inline_data = 0;
   qp_attr.qp_type = IBV_QPT_RC;

   s_ctx.qp = ibv_create_qp(s_ctx.pd, &qp_attr);
   CHECK_MSG(s_ctx.qp != NULL, "Error getting qp");

   puts("Moving QP to init");

   struct ibv_qp_attr attr;
   memset(&attr, 0, sizeof(attr));
    
   attr.qp_state        = IBV_QPS_INIT;
   attr.pkey_index      = 0;
   attr.port_num        = 1;
   attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE  | IBV_ACCESS_REMOTE_READ|IBV_ACCESS_REMOTE_ATOMIC ;//0;

   int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
   int retval = ibv_modify_qp(s_ctx.qp, &attr, flags);
   CHECK_MSG(retval == 0, "Error modifying qp");
}

void wait_for_tcp_connection()
{
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
        exit(1);
    }
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }
    my_addr.sin_family = AF_INET; // host byte order
    my_addr.sin_port = htons(MYPORT); // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(my_addr.sin_zero), 0, 8); // zero the rest of the struct

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        exit(1);
    }
    if (listen(sockfd, 10) == -1)
    {
        perror("listen");
        exit(1);
    }
    
    sin_size = sizeof(struct sockaddr_in);
    if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr,&sin_size)) == -1)
    {
	    //perror("accept");
	    exit(-1);
    }
    printf("Received request from Client: %s:%d\n",
		    inet_ntoa(their_addr.sin_addr),MYPORT);
}

int main(void)
{
    timer = time(NULL);

    wait_for_tcp_connection();
    
    puts("Handshaking. Getting mem_size");
    handshake_get_memsize();

    setup_rdma_1();
    get_port_data();
    
    puts("Handshaking");
    handshake();

    setup_rdma_2();

    CHECK(ibv_req_notify_cq(s_ctx.send_cq, 0) == 0);
    CHECK(ibv_req_notify_cq(s_ctx.recv_cq, 0) == 0);

    puts("ibv_get_cq_event");
    void *ctx;

    int num_comp, num_comp2;
    struct ibv_wc wc;
    do {
	    num_comp = ibv_poll_cq(s_ctx.recv_cq, 1, &wc);
	    num_comp2 = ibv_poll_cq(s_ctx.send_cq, 1, &wc);
    } while (num_comp == 0 && num_comp2==0);

    puts("YEAY");
    printf("rdma_buffer: %s\n", s_ctx.rdma_buffer);    

    CHECK(ibv_get_cq_event(s_ctx.event_channel, &s_ctx.recv_cq, &ctx) == 0);

    puts("Got event");

    while(1);

    close(new_fd);
    exit(0);
}

