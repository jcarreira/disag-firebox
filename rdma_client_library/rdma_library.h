/*
 * RDMA Client Library (Kernel Space)
 * Not concurrent
 */

#ifndef _RDMA_LIB_H_
#define _RDMA_LIB_H_

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

typedef struct rdma_ctx* rdma_ctx_t;
typedef struct rdma_request* rdma_req_t;

typedef enum {RDMA_READ,RDMA_WRITE} RDMA_OP;

typedef struct rdma_request
{
    RDMA_OP rw;
    void *local;
    uint32_t remote_offset;
    int length;
} rdma_request;

int rdma_library_ready(void);

int rdma_library_init(void);
int rdma_library_exit(void);

rdma_ctx_t rdma_init(int npages, char* ip_addr, int port);
int rdma_exit(rdma_ctx_t);

int rdma_op(rdma_ctx_t ctx, rdma_req_t req, int n_requests);
#endif // _RDMA_LIB_H_


