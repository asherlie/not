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

/*#include "not.h"*/
#include "peercalc.h"
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

/* end host */

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
      sn->direct_peers = malloc(sizeof(struct node*)*sn->direct_cap);
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

/* TODO: we need a mutex lock on sn */
struct node* shortest_sub_net_dist(struct sub_net* sn, int dest_uid){
       /*
        * check if sock == -1
        * socks will be set to -1 by handler if MSG_BROKEN
       */
       /* TODO: use a less naive approach */
       if(!sn->n_direct)return NULL;
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

void send_txt_msg(struct sub_net* sn, int uid, char* msg){
      /* TODO: don't use strlen */
      send_prop_msg(sn, TEXT_COM, sn->me->uid, uid, msg, strlen(msg));
}

/* forward declaration */
int connect_sock(struct node* me, struct in_addr inet_addr, int uid, struct sub_net* sn);

/* pp_opt is used only for recursive calls once prop msg reaches dest */
_Bool handle_msg(struct msg m, struct read_th_arg* rta, struct prop_pkg* pp_opt){
      switch(m.type){
            case MSG_BROKEN:
                  rta->sock = -1;
                  return 0;
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
            case TEXT_COM:
                  printf("got message from %i: %s\n", pp_opt->sender_uid, (char*)m.buf);
      }
      return 1;
}

void* read_th(void* rta_v){
      struct read_th_arg* rta = (struct read_th_arg*)rta_v;
      struct msg m; 

      while(((m = read_msg(rta->sock)).type != MSG_BROKEN) && handle_msg(m, rta, NULL));
      return NULL;
}

/* returns the socket of the peer - this is also stored in rta->sock */
/* new peer information is stored in sn, if(sn) */
int connect_sock(struct node* me, struct in_addr inet_addr, int uid, struct sub_net* sn){
      pthread_t read_pth;
      struct read_th_arg* rta = malloc(sizeof(struct read_th_arg));
      rta->sock = me->sock;

      // uhh clean this up
      rta->sock = socket(AF_INET, SOCK_STREAM, 0);

      rta->me = me;

      struct sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = PORT;
      addr.sin_addr = inet_addr;

      /*connect(me->sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));*/
      connect(rta->sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));

      if(sn){
            if(sn->n_direct == sn->direct_cap){
                  sn->direct_cap *= 2;
                  struct node** tmp_n = malloc(sizeof(struct node*)*sn->direct_cap);
                  memcpy(tmp_n, sn->direct_peers, sizeof(struct node*)*sn->n_direct);
                  free(sn->direct_peers);
                  sn->direct_peers = tmp_n;
            }
            sn->direct_peers[sn->n_direct++] = create_node(uid, inet_addr, rta->sock);
      }

      /* rta->sock is redundant */
      /*nvm don't do this - new socket should be returned*/
      /*rta->sock = me->sock;*/

      pthread_create(&read_pth, NULL, read_th, rta);
      pthread_detach(read_pth);

      return rta->sock;
}


/* host */
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

                  pthread_create(&read_pth, NULL, read_th, rta);
                  pthread_detach(read_pth);
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
      int master_sock = connect_sock(me, addr, -1, NULL);

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
      shutdown(master_sock, 2);
}

/* TODO: add a queue structure for sending messages */
/* wait - possibly only need a queue to receive them
 * sending messages occurs within a repl and is limited
 * by the user 
 * we need a queue for receiving many messages concurrently
 */
void queue_msg(struct sub_net* sn, int uid, char* ln){
      struct node* closest = shortest_sub_net_dist(sn, uid);
      (void)closest;
      (void)ln;
      /*send_msg();*/
}

int main(int a, char** b){
      if(a != 2)return EXIT_FAILURE;
      /* TODO: destroy this */
      pthread_mutex_init(&uid_lock, NULL);
      int local_sock = socket(AF_INET, SOCK_STREAM, 0);

      struct sub_net sn;
      init_sub_net(&sn);

      /* does this need to be on the heap? */
      struct accept_th_arg* ata = malloc(sizeof(struct accept_th_arg));
      /*struct accept_th_arg ata;*/
      ata->local_sock = local_sock;
      ata->sn = &sn;
      /* expecting ./not -m <ip> */
      /*ata.master_node = a == 3;*/
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
            shutdown(local_sock, 2);
            perror("bind");
            exit(EXIT_FAILURE);
      }

      /*
       * if(ata->master_node){
       *       ata->pot_cap = 100;
       *       ata->pot_peers = calloc(ata->pot_cap, sizeof(int));
       * }
      */
      ata->me = sn.me = create_node(
                                    (ata->master_node) ? assign_uid() : -1,
                                    s_addr.sin_addr,
                                    (ata->master_node) ? local_sock : socket(AF_INET, SOCK_STREAM, 0)
                                   );
      printf("addr: %i\n", (int)s_addr.sin_addr.s_addr);

      pthread_t accept_pth;
      pthread_create(&accept_pth, NULL, accept_th, ata);
      pthread_detach(accept_pth);

      /*join_network(&sn.me, b[2], local_sock);*/
      /* should this connector sock have the ip i want ppl to connect
       * to when they hit me up?
       * after i request a formal connection
       * yes, right
       * can two socks have same addr
       */
      if(!ata->master_node)
            join_network(sn.me, b[1]);

      /*
       * while(1){
       *       getchar()
       * }
      */

      char* ln = NULL;
      size_t sz;
      puts("RE");
      char* i;
      while(getline(&ln, &sz, stdin) != EOF){
            /*queue_msg(ln);*/
            for(i = ln; *i && *i != ' '; ++i);
            if(!*i)continue;

            int uid = atoi(ln);
            char* msg = i+1;

            send_txt_msg(&sn, uid, msg);

            /*queue_msg(&sn, uid, ln);*/
            /*printf("%i direct peers\n", sn.n_direct);*/
      }
}
