#include <stdlib.h>

#include "node.h"
#include "shared.h"

_Bool node_connected(struct node* n){
      if(n->sock == -1)return 0;
      return send_msg(n->sock, CONN_CHECK, NULL, 0);
}

struct node* create_node(int uid, struct in_addr addr, int sock){
      struct node* ret = malloc(sizeof(struct node));
      ret->uid = uid;
      ret->addr = addr;
      ret->sock = sock;
      return ret;
}
