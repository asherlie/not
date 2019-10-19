#define PORT 2390

typedef enum msgtype {
      CON_REQ = 0, ADDR_REQ, ADDR_ALERT, UID_REQ, UID_ALERT, MSG_BROKEN, PROP_MSG, TEXT_COM, NEW_PEER_UID, CONN_CHECK
      }msgtype_t;

struct node{
      int uid, sock;
      struct in_addr addr;
};

/*
 * i connect to master node, request uid from master node
 * based on my uid, i can assume the size of the network
 * 
 * using this information i will request the master node to find the
 * node i need to connect to
 * REQ(node 1);
 * REQ(node 2);
 * REQ(node 4);
 * 
 * master node will pass along my request, as well as my address and uid, and most importantly
 * the recipient's uid
 * 
 * once the message gets to the recipient, recipient will connect to my address
 * recipient adds my information to their struct sub_net in the form of a node with my socket and uid
*/


struct sub_net{
      //pthread_mutex_t 
      int n_direct, direct_cap;
      struct node** direct_peers;
      struct node* me;
};

struct msg{
      msgtype_t type;
      void* buf;
      int buf_sz;
};

/* buffer contains a request_package when a REQ message is sent */
struct request_package{
      int dest_uid, loop_uid;
      struct in_addr loop_addr;
      //char addr_str[50];
};

struct prop_pkg{
      /* dest_buf{sz} are used once the message reaches dest_uid */
      int dest_uid, sender_uid, dest_bufsz;
      msgtype_t msgtype;
      char dest_buf[200];
};

/* arguments */

/* these two structs can be consolidated */
struct accept_th_arg{
      _Bool master_node;
      //int* pot_peers, pot_cap;

      int local_sock;
      struct node* me;

      struct sub_net* sn;
};

struct read_th_arg{
      _Bool master_node;
      int sock;
      struct node* me;

      struct in_addr peer_addr;

      struct sub_net* sn;
};

struct thrad_cont{
      void* x;
};
