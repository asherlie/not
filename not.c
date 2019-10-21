/* TODO: possible host/net byte order issues */
/* big TODO: make a diagram of threads */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "node.h"
#include "sub_net.h"
#include "peercalc.h"
#include "shared.h"

pthread_mutex_t uid_lock;
int UID_ASN = 0;


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

      if(read(sock, &ret.type, sizeof(msgtype_t)) <= 0 ||
        (read(sock, &ret.buf_sz, sizeof(int)) <= 0)){
             ret.type = MSG_BROKEN;
             return ret;
      }

      /* free buf */
      if(ret.buf_sz){
            ret.buf = calloc(ret.buf_sz, sizeof(char));
            if(read(sock, ret.buf, ret.buf_sz) <= 0)
                  ret.type = MSG_BROKEN;
      }

      return ret;
}

/* sock is socket of master node, uid is uid of target peer
 * target peer will join addr
 */
void request_connection(int sock, int uid, int initiator_uid, struct in_addr addr){
      struct request_package rp;
      rp.dest_uid = uid;
      /* this is my address */
      rp.loop_addr = addr;
      rp.loop_uid = initiator_uid;
      send_msg(sock, CON_REQ, &rp, sizeof(struct request_package));
}

_Bool send_prop_msg(struct sub_net* sn, msgtype_t msgtype, int sender_uid, 
                   int dest_uid, void* buf, int buf_sz){
      struct prop_pkg pp;
      pp.dest_uid = dest_uid;
      pp.sender_uid = sender_uid;
      memcpy(pp.dest_buf, buf, buf_sz);
      pp.dest_bufsz = buf_sz;
      pp.msgtype = msgtype;

      /*
       * won't work because buf is apointer to our virt mem
       * need to eithe rset aside like 200 bytes in each struct prop msg 
       * to store buf
       * or have another mesage send soon after
       * pp.buf = buf;
      */
      
      struct node* closest = shortest_sub_net_dist(sn, dest_uid);
      if(!closest)return 0;

      send_msg(closest->sock, PROP_MSG, &pp, sizeof(struct prop_pkg));
      return 1;
}

_Bool send_txt_msg(struct sub_net* sn, int uid, char* msg){
      /* TODO: don't use strlen */
/*
 *       concern:
 *       why does sn have 2 direct peers in master node when master sends msg to new node
 *       should only be new node peer
 *       instead, however, we have new node and a node with -1 as uid 
 * 
 *       our first peer is -1 uid
 *       this causes issues in shortest_sub_net_dist
 *       -1 is the shortest
 *       when correcting this in gdb, message was sent to myself
 * 
 *       omg this is obvious, entries are never removed from sn 
 * 
*/

      return send_prop_msg(sn, TEXT_COM, sn->me->uid, uid, msg, strlen(msg));
}

/* msg queue */
void msg_queue_init(struct msg_queue* mq){
      pthread_mutex_init(&mq->lock, NULL);
      mq->n_msgs = 0;
      mq->msg_cap = 100;
      mq->msgs = malloc(mq->msg_cap*sizeof(struct mq_entry));
}

void msg_queue_insert(struct msg_queue* mq, char* msg, int sender_uid){
      pthread_mutex_lock(&mq->lock);
      if(mq->n_msgs == mq->msg_cap){
            mq->msg_cap *= 2;
            /*char** tmp_msg = malloc(mq->msg_cap*sizeof(char*));*/
            struct mq_entry* tmp_msg = malloc(mq->msg_cap*sizeof(struct mq_entry));
            memcpy(tmp_msg, mq->msgs, mq->n_msgs*sizeof(struct mq_entry));
            free(mq->msgs);
            mq->msgs = tmp_msg;
      }
      mq->msgs[mq->n_msgs].msg = msg;
      mq->msgs[mq->n_msgs++].sender = sender_uid;
      pthread_mutex_unlock(&mq->lock);
}

struct mq_entry* msg_queue_pop(struct msg_queue* mq){
      pthread_mutex_lock(&mq->lock);
      struct mq_entry* ret = (mq->n_msgs) ? &mq->msgs[--mq->n_msgs] : NULL;
      pthread_mutex_unlock(&mq->lock);
      return ret;
}
/* /msg queue */

/* forward declaration */
int connect_sock(struct node* me, struct in_addr inet_addr, int uid, struct sub_net* sn);

/* pp_opt is used only for recursive calls once prop msg reaches dest */
_Bool handle_msg(struct msg m, struct read_th_arg* rta, struct prop_pkg* pp_opt){
      switch(m.type){
            case NEW_PEER_UID:{
                  if(m.buf_sz != sizeof(int))return 1;
                  int uid;
                  memcpy(&uid, m.buf, sizeof(int));
                  struct in_addr dummy_addr; 
                  dummy_addr.s_addr = 0;
                  sn_insert_direct_peer(rta->sn, uid, dummy_addr, rta->sock);
                  break;
            }
            case MSG_BROKEN:{
                  /* this can occur during handshake */
                  if(!rta->sn)break;
                  for(int i = 0; i < rta->sn->n_direct; ++i){
                        /* why is sn NULL? */
                        if(rta->sn->direct_peers[i]->sock == rta->sock){
                              rta->sn->direct_peers[i]->sock = -1;
                              break;
                        }
                  }
                  /*mn can't send itself a message*/
                  return 0;
            }
            case ADDR_REQ:
                  if(!rta->master_node)return 1;
                  send_msg(rta->sock, ADDR_ALERT, &rta->peer_addr, sizeof(struct in_addr));
                  break;
            case ADDR_ALERT:
                  memcpy(&rta->me->addr, m.buf, sizeof(struct in_addr));
                  break;
            case UID_REQ:
                  if(!rta->master_node)return 1;
                  send_uid_alert(rta->sock);
                  break;
            case UID_ALERT:
                  memcpy(&rta->me->uid, m.buf, sizeof(int));
                  break;
            case CON_REQ:{
                  /* should we only continue if master node? */
                  if(!rta->master_node || m.buf_sz != sizeof(struct request_package))return 1;

                  struct request_package rp;
                  memcpy(&rp, m.buf, sizeof(struct request_package));
                  if(rta->me->uid == rp.dest_uid){
                        /*
                         * connect sock should use a new socket each time
                         * that it creates and listen()s
                        */

/*
 *                          all users should accept connections from strangers
 *                          upon receiving a uid assignment request, connectee will
 *                          instead senda  MN_REROUTE message directing NN to
 *                          the designated MN
 *                          upon receiving an MN_REROUTE, an NN will re-request a uid
 *                          assignmnent
 * 
*/
                        connect_sock(rta->me, rp.loop_addr, rp.loop_uid, rta->sn);
                  }

                  if(rta->me->uid != rp.dest_uid){

                        /*sock should be a socket from out subnet*/
                        /*
                         * define a functoin to find closest uid to request
                         * to the destination
                        */
                        struct node* closest = shortest_sub_net_dist(rta->sn, rp.loop_uid);
                        if(!closest)return 1;
                        request_connection(closest->sock, rp.dest_uid, rp.loop_uid, rp.loop_addr);
                        /*request_connection(rta->sock, rp.dest_uid, rp.loop_uid, rp.loop_addr);*/
                  }

                  break;
            }
            case PROP_MSG:{
                  if(m.buf_sz != sizeof(struct prop_pkg))return 1;
                  struct prop_pkg pp;
                  memcpy(&pp, m.buf, sizeof(struct prop_pkg));
                  if(rta->me->uid == pp.dest_uid){

                        struct msg tmp_m;
                        tmp_m.type = pp.msgtype;
                        tmp_m.buf = pp.dest_buf;
                        tmp_m.buf_sz = pp.dest_bufsz;

                        handle_msg(tmp_m, rta, &pp);
                  }
                  else{
                        struct node* closest = shortest_sub_net_dist(rta->sn, pp.dest_uid);
                        if(!closest)return 1;
                        /* TODO: there's no reason to rebuild a prop pkg
                         * when we already have one
                         */
                        send_prop_msg(rta->sn, pp.msgtype, pp.sender_uid, pp.dest_uid, pp.dest_buf, pp.dest_bufsz);
                  }
                  break;
            }
            case CONN_CHECK:
                  break;
            case TEXT_COM:{
                  /*
                   * add this to mutex locked message queue
                   * another thread will pop them off and pretty print them
                   * no io from this thread
                  */
                  char* mstr = malloc(m.buf_sz*sizeof(char));
                  memcpy(mstr, m.buf, m.buf_sz*sizeof(char));
                  msg_queue_insert(rta->sn->mq, mstr, pp_opt->sender_uid);
                  /*printf("got message from %i: %s\n", pp_opt->sender_uid, (char*)m.buf);*/
            }
      }
      return 1;
}

void* read_th(void* rta_v){
      struct read_th_arg* rta = (struct read_th_arg*)rta_v;

      while(handle_msg(read_msg(rta->sock), rta, NULL));

      /* this implicitly removes broken connection by removing all peers with sock -1 */

      /* rta->sn can be NULL if thread was added ny NN connecting to MN */
      /* this happens in join_network() */
      if(rta->sn)sn_purge(rta->sn);
      return NULL;
}

/* returns the socket of the peer - this is also stored in rta->sock */
/* new peer information is stored in sn, if(sn) */
int connect_sock(struct node* me, struct in_addr inet_addr, int uid, struct sub_net* sn){
      pthread_t read_pth;
      struct read_th_arg* rta = malloc(sizeof(struct read_th_arg));
      rta->sock = me->sock;

      rta->sock = socket(AF_INET, SOCK_STREAM, 0);
      rta->sn = sn;

      rta->me = me;

      struct sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(PORT);
      addr.sin_addr = inet_addr;

      connect(rta->sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));

      if(sn)sn_insert_direct_peer(sn, uid, inet_addr, rta->sock);

      /* rta->sock is redundant */
      /*nvm don't do this - new socket should be returned*/
      /*rta->sock = me->sock;*/

      pthread_create(&read_pth, NULL, read_th, rta);
      pthread_detach(read_pth);

      /* send our uid to our new direct peer so that we can be added to their sub_net */
      send_msg(rta->sock, NEW_PEER_UID, &me->uid, sizeof(int));

      return rta->sock;
}


void* accept_th(void* arg_v){
      struct accept_th_arg* arg = (struct accept_th_arg*)arg_v;
      struct sockaddr_in addr;
      memset(&addr, 0, sizeof(struct sockaddr_in));

      /*if(arg->master_node && listen(arg->local_sock, 0) == -1)perror("listen");*/
      if(listen(arg->local_sock, 0) == -1)perror("listen");

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
                  rta->sn = arg->sn;
                  rta->sock = peer_sock;
                  rta->me = arg->me;
                  rta->master_node = arg->master_node;
                  rta->peer_addr = addr.sin_addr;

                  /* TODO URGENT: we need to know new peer's uid */
                  /*sn_insert_direct_peer(rta->sn, 0);*/

                  pthread_create(&read_pth, NULL, read_th, rta);
                  pthread_detach(read_pth);

                  /*
                   * wait for new peer's uid to be set with timeout technique
                   * handle message will ideally receive a MY_UID_ALERT
                   * and set it
                   * we can also have handle_msg await a MY_UID_ALERT as a sign
                   * that a user has joined us from a request
                   * handle_msg will then add this new user to our struct sub_net 
                   * this minimizes complexity, as we only need to add one protocol
                  */
            }
      }
      return NULL;
}


/* this is called by a client to join the network
 * addr should be to the master node
 */
void join_network(struct node* me, char* master_addr){
      struct in_addr addr;
      inet_aton(master_addr, &addr);
      /*
       * big TODO:
       * keep track of all read threads, close them when necessary
       * or just detach them and have them stop running once reading a -1
      */
      /* this connects and starts a read thread */
      /*subnet is NULL in this call*/
      int master_sock = connect_sock(me, addr, -1, NULL);
      /*int master_sock = connect_sock(me, addr, -1, sn);*/

      send_msg(master_sock, UID_REQ, NULL, 0);
      /*
       * send a uid_request
       * send_msg();
       * wait below for uid assignment
       * then send log2(uid) connection polls
      */

      /* we now wait to receive a uid assignment */

      /* 100 second timeout */
      int timeout = 0;
      /*make this a macro and use it for my address request*/
      while(me->uid == -1 && !usleep(10000))
            if(++timeout == 1e4){
                  puts("fatal error - timed out waiting for uid assignment");
                  exit(EXIT_FAILURE);
            }
      printf("we've been assigned uid: %i\n", me->uid);

      /* requesting my address */
      send_msg(master_sock, ADDR_REQ, NULL, 0);

      timeout = 0;

      /* this is valid because INADDR_ANY == 0 */
      while(!me->addr.s_addr && !usleep(10000))
            if(++timeout == 1e4){
                  puts("fatal error - timed out waiting for addr notification");
                  exit(EXIT_FAILURE);
            }

      int n_peers;
      int* to_conn = gen_peers(me->uid, &n_peers);

      for(int i = 0; i < n_peers; ++i){
            /*
             * we need to wait after each conn request to confirm we've been reached out to
             * we can timeout and ignore it if no conn is made assume death
             * once we've been connected to, CLOSE SOCKE WITH MASTER NODE
            */
            request_connection(master_sock, to_conn[i], me->uid, me->addr);
      }
      close(master_sock);
}

/* this thread can be killed by setting mq->msgs to NULL */
void* msg_print_th(void* mqv){
      struct msg_queue* mq = (struct msg_queue*)mqv;
      struct mq_entry* msg;
      while(mq->msgs){
            msg = msg_queue_pop(mq);
            if(msg){
                  printf("%i: %s%s%s\n", msg->sender, ANSI_BLU, msg->msg, ANSI_NON);
                  free(msg->msg);
            }
            usleep(1e4);
      }
      return NULL;
}

void print_error(char* str){
      printf("%s%s%s\n", ANSI_RED, str, ANSI_NON);
}

int main(int a, char** b){
      if(a != 2)return EXIT_FAILURE;
      /* TODO: destroy this */
      pthread_mutex_init(&uid_lock, NULL);
      int local_sock = socket(AF_INET, SOCK_STREAM, 0);

      struct sub_net sn;
      init_sub_net(&sn);

      sn.mq = malloc(sizeof(struct msg_queue));
      msg_queue_init(sn.mq);

      /* does this need to be on the heap? */
      struct accept_th_arg* ata = malloc(sizeof(struct accept_th_arg));

      ata->local_sock = local_sock;
      ata->sn = &sn;

      ata->master_node = *b[1] == '-';

      struct sockaddr_in s_addr;
      memset(&s_addr, 0, sizeof(struct sockaddr_in));

      s_addr.sin_port = PORT;
      s_addr.sin_family = AF_INET;

      /* this is for accept thread */
      /*inet_aton((ata.master_node) ? b[2] : b[1], &s_addr.sin_addr);*/
      /*
       * this is a bad solution - we need to be able to bind to an address
       * that others will know
       * s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      */
      s_addr.sin_addr.s_addr = INADDR_ANY;

      if(bind(local_sock, (struct sockaddr*)&s_addr, sizeof(struct sockaddr_in)) == -1){
            close(local_sock);
            perror("bind");
            exit(EXIT_FAILURE);
      }

      ata->me = sn.me = create_node(
                                    (ata->master_node) ? assign_uid() : -1,
                                    s_addr.sin_addr,
                                    /*(ata->master_node) ? local_sock : socket(AF_INET, SOCK_STREAM, 0)*/
                                    local_sock
                                   );
      printf("addr: %i\n", (int)s_addr.sin_addr.s_addr);

      pthread_t accept_pth;
      pthread_create(&accept_pth, NULL, accept_th, ata);
      pthread_detach(accept_pth);

      /* TODO: set mq->msgs to NULL and pthread_join() */
      pthread_t msg_print_pth;
      pthread_create(&msg_print_pth, NULL, msg_print_th, (void*)sn.mq);

      if(!ata->master_node)
            join_network(sn.me, b[1]);

      char* ln = NULL;
      size_t sz;
      char* i;
      int len;
      while((len = getline(&ln, &sz, stdin)) != EOF){
            ln[--len] = 0;
            switch(*ln){
                  /* [i]nfo */
                  case 'i':
                        /* this information can be incorrect if sn_purge 
                         * hasn't been called yet
                         */
                        printf("%i direct peers\n", sn.n_direct);
                        break;
                  case '0': case '1': case '2':
                  case '3': case '4': case '5':
                  case '6': case '7': case '8': case '9':{
                        for(i = ln; *i && *i != ' '; ++i);
                        if(!*i)continue;

                        int uid = atoi(ln);
                        char* msg = i+1;

                        if(!send_txt_msg(&sn, uid, msg))print_error("failed to find a route to recipient");
                  }
            }
      }
      return 0;
}
