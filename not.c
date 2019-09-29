#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "not.h"
#include "shared.h"

int UID_ASN = 0;

/* spec:
 *    for REQ
 *          buf contains a struct request_package
 *          buf contains ip of final destination k
 */
void send_msg(int sock, msgtype_t msgtype, void* buf, int buf_sz){
      send(sock, &msgtype, sizeof(msgtype_t), 0);
      send(sock, &buf_sz, sizeof(int), 0);
      send(sock, buf, buf_sz, 0);
}


/* TODO: what happens when an internal node disconnects? */
int assign_uid(){
      return UID_ASN++;
}

void send_uid_alert(int sock){
     int uid = assign_uid();
     send_msg(sock, UID_ALERT, &uid, sizeof(int));
}

/* host */
void* accept_th(void* arg_v){
      struct accept_th_arg* arg = (struct accept_th_arg*)arg_v;
      struct sockaddr_in addr = {0};

      /* not sure that this assignment is necessary */
      socklen_t slen = sizeof(struct sockaddr_in);

      int peer_sock;
      while(1){
            if((peer_sock = accept(arg->local_sock, (struct sockaddr*)&addr, &slen)) != -1)
                  send_uid_alert(peer_sock);
      }
}
/* end host */

/* client */
/* client joins and immediately gets a uid - with this information client knows to wait for connections from uids that 
 * are appropriate distances from her
 * uid == 1 joins and requests information about uid == 0
 *
 * uid == 2 joins by connecting to uid == 0 and knows to read the amount of bytes necessary to get information about
 * nodes uid == 0, uid == 1j
 *
 * wait no we should expect clients to request information so it can be passed through system
 * send a request to node 0 for each node it computes that it needs to connect to
 * 0 requests highest node possible without getting to correct one
 * this follows until message containing destination node uid and original requester ip address gets to destination node
 * destination node will connect to requester node - this avoids he need to backtrack the entire connection
 *
 */
void* connect_th(char* ip){
      int sock = socket(AF_INET, SOCK_STREAM, 0);
      (void)sock;
      (void)ip;
      return NULL;
}
/* end client */

/* *addr is optionally set to the addrss of the initial sender */
struct msg read_msg(int sock){
      struct msg ret;

      read(sock, &ret.type, sizeof(msgtype_t));
      read(sock, &ret.buf_sz, sizeof(int));

      /* free buf */
      ret.buf = calloc(ret.buf_sz, sizeof(char));
      read(sock, ret.buf, ret.buf_sz);

      return ret;
}

/* node/net operations */
struct node* create_node(int uid, struct in_addr addr){
      struct node* ret = malloc(sizeof(struct node));
      ret->uid = uid;
      ret->addr = addr;
      return ret;
}

void init_sub_net(struct sub_net* sn){
      sn->n_direct = 0;
      sn->direct_cap = 20;
      sn->direct_peers = malloc(sizeof(struct node)*sn->direct_cap);
}

void join_network(struct node** me, char* master_addr){
       struct in_addr dummy;
       *me = create_node(-1, dummy);
       (*me)->sock = socket(AF_INET, SOCK_STREAM, 0);

      struct sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = PORT;

      inet_aton(master_addr, &addr.sin_addr);

      int master_sock = connect((*me)->sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));

      /* we now wait to receive a uid assignment */
      int timeout = 0;
      while((*me)->uid == -1 && usleep(10000))
            if(++timeout == 1e4){
                  puts("fatal error - timed out waiting for uid assignment");
                  exit(EXIT_FAILURE);
            }
}

void handle_msg(struct msg m){
      switch(m.type){
            case UID_ALERT:
                  memcpy(&m.me->uid, m.buf, sizeof(int));
            /* TODO: make sure that we're the master node
             * if not, do not attempt to assign
             */
            case REQ:;
      }
}

int main(int a, char** b){
      struct sub_net sn;
      init_sub_net(&sn);

      (void)b;
      /* we're taking on master role */
      if(a == 1){
            struct in_addr dummy;
            sn.me = create_node(assign_uid(), dummy);
      }
      else{
            join_network(&sn.me, b[1]);
      }
      assert(sizeof(char) == 1);
}
