#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "shared.h"

/* host */
void* accept_th(void* v_sock){
      int local_sock = *((int*)v_sock);
      struct sockaddr_in addr = {0};
      /* not sure that this assignment is necessary */
      socklen_t slen = sizeof(struct sockaddr_in);

      int peer_sock;
      while(1){
            if((peer_sock = accept(local_sock, (struct sockaddr*)&addr, &slen)) != -1){};
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

int main(){
}
