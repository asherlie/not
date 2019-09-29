#define PORT 2390
typedef enum msgtype {REQ = 0}msgtype_t;

struct node{
      int uid;
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
};
