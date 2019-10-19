#include "shared.h"

/* spec:
 *    for REQ
 *          buf contains a struct request_package
 *          buf contains ip of final destination k
 */
_Bool send_msg(int sock, msgtype_t msgtype, void* buf, int buf_sz){
      return
      send(sock, &msgtype, sizeof(msgtype_t), 0) != -1 &&
      send(sock, &buf_sz, sizeof(int), 0) != -1 &&
      send(sock, buf, buf_sz, 0) != -1;
}

