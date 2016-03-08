
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/un.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/module.h>

//#define IP   "127.0.0.1"
#define MAX  100
#define PORT 1025

struct socket *sock = NULL;

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

static int __init client_module_init( void ) {

    int retval;
    char str[MAX];

    struct sockaddr_in servaddr;

    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    u8 IP[] = {127,0,0,1};

    printk(KERN_INFO "Start client module.\n");

    // create
    retval = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock); 

    // connect
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

    // recvmsg

    printk(KERN_INFO "about to do memset1");
    memset(&msg, 0, sizeof(msg));
//    memset(&iov, 0, sizeof(iov));
    printk(KERN_INFO "memset1\n");

    msg.msg_name = &servaddr;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_iov->iov_base= str;
    msg.msg_iov->iov_len = MAX;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;
    
    printk(KERN_INFO "stage2\n");

    //http://www.linuxjournal.com/node/8110/print
    oldfs = get_fs();
    //printk(KERN_INFO "stage2.1\n");
    set_fs(KERNEL_DS);

    retval = sock_recvmsg(sock, &msg, MAX, 0);

    set_fs(oldfs);
    
    printk(KERN_INFO "recvmsg retval: %d str: %s\n", retval, str);

    // print str
    printk(KERN_INFO "client module: %s.\n",str);

    // release socket
    sock_release(sock);

    return 0;
}

static void __exit client_module_exit( void )
{
    printk(KERN_INFO "Exit client module.\n");
}

module_init( client_module_init );
module_exit( client_module_exit );
MODULE_LICENSE("GPL");

