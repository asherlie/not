#include <stdlib.h>

#include "node.h"
#include "shared.h"

_Bool node_connected(struct node* n){
      if(n->sock == -1)return 0;
      return send_msg(n->sock, CONN_CHECK, NULL, 0);
}

/* TODO: we need a mutex lock on sn */
struct node* shortest_sub_net_dist(struct sub_net* sn, int dest_uid){
       /*
        * check if sock == -1
        * socks will be set to -1 by handler if MSG_BROKEN
       */
       /* TODO: use a less naive approach */
       if(!sn->n_direct)return NULL;
       /* TODO: URGENT:
        * we need a mutex lock on sn
        */
       /* TODO: should we get rid of sn_purge() and just use
        * sock_connected() in this loop?
        */
       sn_purge(sn);
       int dist, min = abs(dest_uid-sn->direct_peers[0]->uid);
       struct node* min_n = sn->direct_peers[0];
       for(int i = 1; i < sn->n_direct; ++i){
            dist = abs(dest_uid-sn->direct_peers[i]->uid);
            if(sn->direct_peers[i]->sock != -1 && dist < min){
                  min = dist;
                  min_n = sn->direct_peers[i];
            }
       }
       return min_n;
}

struct node* create_node(int uid, struct in_addr addr, int sock){
      struct node* ret = malloc(sizeof(struct node));
      ret->uid = uid;
      ret->addr = addr;
      ret->sock = sock;
      return ret;
}
