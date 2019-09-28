#define PORT 2390

struct node{
      _Bool routing_node;
};

/* only the RN is guaranteed to have a full pictue of the net */
struct net{
      /* uid assigned will be sz++ */
      int sz;
};
