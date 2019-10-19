#include <arpa/inet.h>

#ifndef _NODE_H
#define _NODE_H
struct node{
      int uid, sock;
      struct in_addr addr;
};

_Bool node_connected(struct node* n);
struct node* create_node(int uid, struct in_addr addr, int sock);
#endif
