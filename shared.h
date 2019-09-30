#define PORT 2390

typedef enum msgtype {
      REQ = 0, UID_REQ, UID_ALERT
      }msgtype_t;

struct node{
      int uid, sock;
      struct in_addr addr;
};

struct sub_net{
      int n_direct, direct_cap;
      struct node* direct_peers;
      struct node* me;
};

struct msg{
      msgtype_t type;
      void* buf;
      int buf_sz;

      /* useful for setting uid */
      struct node* me;
};

/* buffer contains a request_package when a REQ message is sent */
struct request_package{
      int dest_uid;
      char addr_str[50];
};

/* arguments */

/* these two structs can be consolidated */
struct accept_th_arg{
      _Bool master_node;
      int* pot_peers, pot_cap;

      int local_sock;
      struct node* me;
};

struct read_th_arg{
      _Bool master_node;
      int sock;
      struct node* me;
};

struct thrad_cont{
      void* x;
};
