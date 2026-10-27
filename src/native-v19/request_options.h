#ifndef MCPC_REQUEST_OPTIONS_H
#define MCPC_REQUEST_OPTIONS_H
#include <string.h>
static int download_parallel(const char *query){
 const char *p=query;
 while(p&&*p){
  const char *end=strchr(p,'&');size_t n=end?(size_t)(end-p):strlen(p);
  if(n==10&&!memcmp(p,"parallel=4",10))return 4;
  if(n==10&&!memcmp(p,"parallel=8",10))return 8;
  if(n==11&&!memcmp(p,"parallel=16",11))return 16;
  p=end?end+1:NULL;
 }
 return 8;
}
#endif

