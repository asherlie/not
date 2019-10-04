#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

/*#include "not.h"*/
#include "shared.h"

pthread_mutex_t uid_lock;
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
      pthread_mutex_lock(&uid_lock);
      int ret = UID_ASN++;
      pthread_mutex_unlock(&uid_lock);
      return ret;
}

void send_uid_alert(int sock){
     int uid = assign_uid();
     send_msg(sock, UID_ALERT, &uid, sizeof(int));
}

/* *addr is optionally set to the addrss of the initial sender */
struct msg read_msg(int sock){
      struct msg ret;

      if(read(sock, &ret.type, sizeof(msgtype_t)) == -1 ||
         read(sock, &ret.buf_sz, sizeof(int)) == -1){
      
            ret.type = MSG_BROKEN;
            return ret;
      }

      /* free buf */
      ret.buf = calloc(ret.buf_sz, sizeof(char));
      read(sock, ret.buf, ret.buf_sz);

      return ret;
}

_Bool handle_msg(struct msg m, struct read_th_arg* rta){
      switch(m.type){
            case MSG_BROKEN:
                  return 0;
            case UID_REQ:
                  if(!rta->master_node)return 1;
                  send_uid_alert(rta->sock);
                  break;
            case UID_ALERT:
                  memcpy(&m.me->uid, m.buf, sizeof(int));
            /* TODO: make sure that we're the master node
             * if not, do not attempt to assign
             */
            case REQ:;
      }
      return 1;
}

#if 0
when should this be spawned?
each time we accept() im p sure
#endif

void* read_th(void* rta_v){
      struct read_th_arg* rta = (struct read_th_arg*)rta_v;
      struct msg m; 
      m.me = rta->me;
      while(((m = read_msg(rta->sock)).type != MSG_BROKEN) && handle_msg(m, rta));
      /*
       * while(1){
       *       m = read_msg(rta->sock);
       *       handle_msg(m, rta);
       * }
      */
      return NULL;
}

/* host */
void* accept_th(void* arg_v){
      struct accept_th_arg* arg = (struct accept_th_arg*)arg_v;
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(struct sockaddr_in));

      /*if(arg->master_node && listen(arg->local_sock, 0) == -1)perror("listen");*/
      if(listen(arg->local_sock, 0) == -1)perror("listen");

      printf("listening on sock %i\n", arg->local_sock);

      /* not sure that this assignment is necessary */
      socklen_t slen = sizeof(struct sockaddr_in);

      int peer_sock;
/*
 *       there are 3 types of acceptances
 *             1: i'm the master assigning uid
 *                this connection will be closed after assignment  
 * 
 *             2: i'm a node that's being conenected to permanently
*/

      while(1){
            if((peer_sock = accept(arg->local_sock, (struct sockaddr*)&addr, &slen)) != -1){
                  pthread_t read_pth;
                  /* putting this on the heap so it lasts between iteratios */
                  /* TODO: free */
                  struct read_th_arg* rta = malloc(sizeof(struct read_th_arg));
                  rta->sock = peer_sock;
                  rta->me = arg->me;

                  pthread_create(&read_pth, NULL, read_th, &rta);

                  /*if we're the master and */
                  /*
                   * if(arg->master_node && arg->pot_peers[UID_ASN])
                  */
                  /*this should be done by read_th*/
                  /*send_uid_alert(peer_sock);*/
            }
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
/*
 * void* connect_th(char* ip){
 *       int sock = socket(AF_INET, SOCK_STREAM, 0);
 *       (void)sock;
 *       (void)ip;
 *       return NULL;
 * }
*/
/* end client */


/* node/net operations */
struct node* create_node(int uid, struct in_addr addr, int sock){
      struct node* ret = malloc(sizeof(struct node));
      ret->uid = uid;
      ret->addr = addr;
      ret->sock = sock;
      return ret;
}

void init_sub_net(struct sub_net* sn){
      sn->n_direct = 0;
      sn->direct_cap = 20;
      sn->direct_peers = malloc(sizeof(struct node)*sn->direct_cap);
}

/*
 * void connect_sock(int sock, struct inet_addr addr, struct thread_cont){
 *       pthread_create
 * }
*/

/* returns the socket of the peer - this is also stored in rta->sock */
int connect_sock(struct node* me, struct in_addr inet_addr){
      pthread_t read_pth;
      struct read_th_arg* rta = malloc(sizeof(struct read_th_arg));
      rta->sock = me->sock;
      rta->me = me;

      struct sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = PORT;
      addr.sin_addr = inet_addr;

      rta->sock = connect(me->sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));

      pthread_create(&read_pth, NULL, read_th, &rta);

      return rta->sock;
}

/* this is called by a client to join the network
 * addr should be to the master node
 */
void join_network(struct node** me, char* master_addr, int local_sock){
       struct in_addr dummy;
       memset(&dummy, 0, sizeof(struct sockaddr_in));
       *me = create_node(-1, dummy, local_sock);

      struct in_addr addr;
      inet_aton(master_addr, &addr);
      /*
       * big TODO:
       * keep track of all read threads, close them when necessary
       * or just detach them and have them stop running once reading a -1
      */
      /* this connects and starts a read thread */
      int master_sock = connect_sock(*me, addr);

      send_msg(master_sock, UID_REQ, NULL, 0);
      /*
       * send a uid_request
       * send_msg();
       * wait below for uid assignment
       * then send log2(uid) connection polls
      */

      /*int master_sock = */

      /*
       * read_th should be called in another thread now
       * not sure how to make that happen
      */

      /* we now wait to receive a uid assignment */
      int timeout = 0;
      while((*me)->uid == -1 && !usleep(10000))
            if(++timeout == 1e4){
                  puts("fatal error - timed out waiting for uid assignment");
                  exit(EXIT_FAILURE);
            }
      printf("we've been assigned uid: %i\n", (*me)->uid);
}

int main(int a, char** b){
      if(a != 3)return EXIT_FAILURE;
      /* TODO: destroy this */
      pthread_mutex_init(&uid_lock, NULL);
      int local_sock = socket(AF_INET, SOCK_STREAM, 0);

      struct sub_net sn;
      init_sub_net(&sn);

      struct accept_th_arg ata;
      ata.local_sock = local_sock;
      /* expecting ./not -m <ip> */
      /*ata.master_node = a == 3;*/
      ata.master_node = *b[1] == '-';

      struct sockaddr_in s_addr;
      s_addr.sin_port = PORT;
      s_addr.sin_family = AF_INET;

      /* this is for accept thread */
      inet_aton((ata.master_node) ? b[2] : b[1], &s_addr.sin_addr);
      /*
       * this is a bad solution - we need to be able to bind to an address
       * that others will know
       * s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      */

      if(bind(local_sock, (struct sockaddr*)&s_addr, sizeof(struct sockaddr_in)) == -1)perror("bind");

      if(ata.master_node){
            ata.pot_cap = 100;
            ata.pot_peers = calloc(ata.pot_cap, sizeof(int));
      }
      ata.me = sn.me;
      /*ata.local_sock = */

      pthread_t accept_pth;
      pthread_create(&accept_pth, NULL, accept_th, &ata);

      /* we're taking on master role */
      if(ata.master_node){
            sn.me = create_node(assign_uid(), s_addr.sin_addr, local_sock);
            while(1)usleep(1000);
      }
      else{
            /*join_network(&sn.me, b[2], local_sock);*/
            /* should this connector sock have the ip i want ppl to connect
             * to when they hit me up?
             * after i request a formal connection
             * yes, right
             * can two socks have same addr
             */
            join_network(&sn.me, b[2], socket(AF_INET, SOCK_STREAM, 0));
      }
      assert(sizeof(char) == 1);
}
