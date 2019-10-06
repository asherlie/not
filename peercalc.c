
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int* gen_two(int n){
      int* ret = malloc(sizeof(int)*n);
      for(int i = 0; i < n; ++i)
            ret[i] = (int)pow((double)2, (double)i);
      return ret;
}

/* returns a malloc'd array of size log2(uid)+1 */
int* gen_peers(int uid, int* n_peers){
      if(!uid)return NULL;

      *n_peers = ((int)log2(uid))+1;
      int table[uid], * ret = malloc(sizeof(int)*(*n_peers));
      memset(table, 0, uid*sizeof(int));
      table[uid-1] = 1;

      int two_len, * tmp_twos;
      for(int i = uid-1; i >= 0; --i){
            /*two_len = ((int)log2(uid))+2;*/
            /* +1 for some reason */
            two_len = *n_peers+1;
            tmp_twos = gen_two(two_len);
            for(int j = 0; j < two_len; ++j){
                  if(tmp_twos[j] + i == uid)
                        ++table[i];
            }
            free(tmp_twos);
      }

      int rs = 0;
      for(int i = 0; i < uid; ++i)
            if(table[i])ret[rs++] = i;
      
      return ret;
}
