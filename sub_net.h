#ifndef _SUB_NET_H
#define _SUB_NET_H

#include "shared.h"
#include <netinet/in.h>

struct sub_net{
      //pthread_mutex_t 
      int n_direct, direct_cap;
      struct node** direct_peers;
      struct node* me;

      struct msg_queue* mq;
};

void init_sub_net(struct sub_net* sn);
void sn_insert_direct_peer(struct sub_net* sn, int uid, struct in_addr inet_addr, int peer_sock);
void sn_remove(struct sub_net* sn, int ind);
int sn_purge(struct sub_net* sn);

struct node* shortest_sub_net_dist(struct sub_net* sn, int dest_uid);
#endif
