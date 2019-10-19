#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#include "sub_net.h"
#include "node.h"

void init_sub_net(struct sub_net* sn){
      sn->n_direct = 0;
      sn->direct_cap = 20;
      sn->direct_peers = malloc(sizeof(struct node*)*sn->direct_cap);
}

void sn_insert_direct_peer(struct sub_net* sn, int uid, struct in_addr inet_addr, int peer_sock){
      /* TODO: pthread_mutex_lock() */
      if(sn->n_direct == sn->direct_cap){
            sn->direct_cap *= 2;
            struct node** tmp_n = malloc(sizeof(struct node*)*sn->direct_cap);
            memcpy(tmp_n, sn->direct_peers, sizeof(struct node*)*sn->n_direct);
            free(sn->direct_peers);
            sn->direct_peers = tmp_n;
      }
      sn->direct_peers[sn->n_direct++] = create_node(uid, inet_addr, peer_sock);
}

void sn_remove(struct sub_net* sn, int ind){
      memmove(sn->direct_peers+ind, sn->direct_peers+ind+1, sizeof(struct node*)*sn->n_direct-ind-1);
      --sn->n_direct;
}

int sn_purge(struct sub_net* sn){
      for(int i = 0; i < sn->n_direct; ++i){
            if(!node_connected(sn->direct_peers[i]))
                  sn_remove(sn, i--);
      }
      return 0;
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
