#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "not.h"
#include "shared.h"

int uid = 0;

/* TODO: what happens when an internal node disconnects? */
int assign_uid(){
      return uid++;
}

/* host */
void* accept_th(void* arg_v){
      struct accept_th_arg* arg = (struct accept_th_arg*)arg_v;
      struct sockaddr_in addr = {0};

      /* not sure that this assignment is necessary */
      socklen_t slen = sizeof(struct sockaddr_in);

      int peer_sock;
      while(1){
            if((peer_sock = accept(arg->local_sock, (struct sockaddr*)&addr, &slen)) != -1){};
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

void send_msg(int sock, msgtype_t msgtype, void* buf, int buf_sz){
      send(sock, &msgtype, sizeof(msgtype_t), 0);
      send(sock, &buf_sz, sizeof(int), 0);
      send(sock, buf, buf_sz, 0);
}

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

void handle_msg(struct msg m){
      switch(m.type){
            /* TODO: make sure that we're the master node
             * if not, do not attempt to assign
             */
            case REQ:;
      }
}

int main(int a, char** b){
      (void)b;
      /* we're taking on master role */
      if(a == 1){
      }
      assert(sizeof(char) == 1);
}
