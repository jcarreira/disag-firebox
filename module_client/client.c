#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/un.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/socket.h>

#define SOCK_PATH   "127.0.0.1:1025"
#define MAX     100

struct socket *sock = NULL;


static int __init client_module_init( void ) {

    int retval;
    char str[MAX];

    struct sockaddr_un addr;
    struct msghdr msg;

//    struct iovec iov;
    mm_segment_t oldfs;

    printk(KERN_INFO "Start client module.\n");

    // create
    retval = sock_create(AF_UNIX, SOCK_STREAM, 0, &sock); 

    // connect
    memset(&addr, 0, sizeof(addr));  
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    retval = sock->ops->connect(sock, (struct sockaddr *)&addr, sizeof(addr), 0);

    return 0;
    // recvmsg

    memset(&msg, 0, sizeof(msg));
//    memset(&iov, 0, sizeof(iov));

    msg.msg_name = 0;
    msg.msg_namelen = 0;
//    msg.msg_iov = &iov;
//    msg.msg_iovlen = 1;
//    msg.msg_iov->iov_base= str;
//    msg.msg_iov->iov_len= strlen(str)+1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    //http://www.linuxjournal.com/node/8110/print
    oldfs = get_fs();
    set_fs(KERNEL_DS);

    retval = sock_recvmsg(sock, &msg, MAX, 0);

    set_fs(oldfs);

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
