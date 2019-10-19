#ifndef _SHARED_H
#define _SHARED_H

#include "node.h"

#define PORT 2390

#define ANSI_RED   "\x1B[31m"
#define ANSI_NON   "\x1b[0m"
#define ANSI_GRE   "\x1b[32m"
#define ANSI_BLU   "\x1b[34m"
#define ANSI_MGNTA "\x1b[35m"

typedef enum msgtype {
      CON_REQ = 0, ADDR_REQ, ADDR_ALERT, UID_REQ, UID_ALERT, MSG_BROKEN, PROP_MSG, TEXT_COM, NEW_PEER_UID, CONN_CHECK
      }msgtype_t;

/*
 * i connect to master node, request uid from master node
 * based on my uid, i can assume the size of the network
 * 
 * using this information i will request the master node to find the
 * node i need to connect to
 * REQ(node 1);
 * REQ(node 2);
 * REQ(node 4);
 * 
 * master node will pass along my request, as well as my address and uid, and most importantly
 * the recipient's uid
 * 
 * once the message gets to the recipient, recipient will connect to my address
 * recipient adds my information to their struct sub_net in the form of a node with my socket and uid
*/

struct msg{
      msgtype_t type;
      void* buf;
      int buf_sz;
};

/* buffer contains a request_package when a REQ message is sent */
struct request_package{
      int dest_uid, loop_uid;
      struct in_addr loop_addr;
      //char addr_str[50];
};

struct prop_pkg{
      /* dest_buf{sz} are used once the message reaches dest_uid */
      int dest_uid, sender_uid, dest_bufsz;
      msgtype_t msgtype;
      char dest_buf[200];
};

struct mq_entry{
      char* msg;
      int sender;
};

struct msg_queue{
      pthread_mutex_t lock;
      int n_msgs, msg_cap; 
      struct mq_entry* msgs;
};

/* arguments */

/* these two structs can be consolidated */
struct accept_th_arg{
      _Bool master_node;
      //int* pot_peers, pot_cap;

      int local_sock;
      struct node* me;

      struct sub_net* sn;
};

struct read_th_arg{
      _Bool master_node;
      int sock;
      struct node* me;

      struct in_addr peer_addr;

      struct sub_net* sn;
};

struct thrad_cont{
      void* x;
};

_Bool send_msg(int sock, msgtype_t msgtype, void* buf, int buf_sz);
#endif
