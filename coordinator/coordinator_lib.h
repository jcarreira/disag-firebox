#ifndef _COORD_LIB_H_
#define _COORD_LIB_H_

typedef struct coordinator_handle c_handle_t;
typedef struct coordinator_data c_data_t;

c_handle_t connect_coordinator(char* ip_addr, int port);
c_data_t get_data(c_handle_t);
int exit_coordinator(c_handle_t);

#endif
