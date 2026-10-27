#ifndef MCPC_HISTORY_TEXT_H
#define MCPC_HISTORY_TEXT_H
typedef struct {char *data;size_t len,cap;int failed;} HistoryJson;
static void history_append(HistoryJson*b,const char*s){
 if(b->failed)return;size_t n=strlen(s);if(n>128*1024*1024||b->len>128*1024*1024-n){b->failed=1;return;}
 if(b->len+n+1>b->cap){size_t cap=b->cap?b->cap:4096;while(cap<b->len+n+1)cap*=2;char*p=realloc(b->data,cap);if(!p){b->failed=1;return;}b->data=p;b->cap=cap;}
 memcpy(b->data+b->len,s,n+1);b->len+=n;
}
static void history_quoted(HistoryJson*b,const char*s){char*q=json_quote(s);if(!q)b->failed=1;else{history_append(b,q);free(q);}}
#endif
