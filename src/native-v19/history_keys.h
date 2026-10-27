#ifndef MCPC_HISTORY_KEYS_H
#define MCPC_HISTORY_KEYS_H
#include <stdint.h>
typedef struct {uint32_t type,id;} HistoryKey;
static int history_number(const char **p,uint32_t*out){
 uint64_t n=0;const char*s=*p;if(*s<'0'||*s>'9')return 0;
 do{n=n*10+(unsigned)(*s++-'0');if(n>0xffffffffULL)return 0;}while(*s>='0'&&*s<='9');
 *p=s;*out=(uint32_t)n;return 1;
}
static int history_keys(const char *query,HistoryKey keys[100]){
 if(strncmp(query,"keys=",5))return 0;query+=5;int n=0;
 for(;;){HistoryKey k;if(n==100||!history_number(&query,&k.type)||*query++!=':'||!history_number(&query,&k.id)||!k.id)return 0;
  int duplicate=0;for(int i=0;i<n;i++)if(keys[i].type==k.type&&keys[i].id==k.id)duplicate=1;
  if(!duplicate)keys[n++]=k;
  if(!*query)return n;if(*query++!=','||!*query)return 0;
 }
}
#endif
