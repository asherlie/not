#include "sub_net.h"
#include <arpa/inet.h>

#ifndef _NODE_H
#define _NODE_H
struct node{
      int uid, sock;
      struct in_addr addr;
};

_Bool node_connected(struct node* n);
struct node* shortest_sub_net_dist(struct sub_net* sn, int dest_uid);
struct node* create_node(int uid, struct in_addr addr, int sock);
#endif
