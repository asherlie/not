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
