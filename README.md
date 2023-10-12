# In-kernel RDMA library

This repo contains client/server code that uses RDMA to communicate. The server runs in userspace, while the client runs as a kernel module.

This code has been tested with the hardware/software configuration at the bottom.

Note: 
Modified to work with newer kernel versions. 

##FAQ

1. Why are there TCP sockets in the code?

   To setup an RDMA QP we need to have some information about the remote node. In the beginning a TCP connection is established to communicate this.

2. This doesn't work for me.

   You can shoot an e-mail or open an issue. I will try to help.

##Hardware and Software configuration:

CPU: Intel IvyBridge E5-1680V2, 8C/16T 3.0GHz 25mb

RAM: 128GB ( 8 x 16gb ECC)

NIC: 56Gbps InfiniBand Mellanox ConnectX-3 VPI MCX354A-FCBT FDR QSFP Dual Port PCI-E x8 8.0GT/s

Switch: Mellanox MSX6036F-1SFS

OS: Ubuntu 14.04.4 Linux 3.19.0-42

OFED: Mellanox OFED 3.2
