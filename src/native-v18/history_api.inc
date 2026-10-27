#include <shellapi.h>
#include <objbase.h>
#include "history_keys.h"
typedef struct {char *data;size_t len,cap;int failed;} HistoryJson;
static void history_append(HistoryJson*b,const char*s){
 if(b->failed)return;size_t n=strlen(s);if(n>128*1024*1024||b->len>128*1024*1024-n){b->failed=1;return;}
 if(b->len+n+1>b->cap){size_t cap=b->cap?b->cap:4096;while(cap<b->len+n+1)cap*=2;char*p=realloc(b->data,cap);if(!p){b->failed=1;return;}b->data=p;b->cap=cap;}
 memcpy(b->data+b->len,s,n+1);b->len+=n;
}
static void history_quoted(HistoryJson*b,const char*s){char*q=json_quote(s);if(!q)b->failed=1;else{history_append(b,q);free(q);}}
static char *history_list_json(char *err){
 unsigned long revision=adapter_history_revision();
 HistoryRow *rows=NULL;int count=0;if(!adapter_history_list(&rows,&count,err))return NULL;
 HistoryJson b={0};char intro[128];snprintf(intro,sizeof(intro),"{\"revision\":%lu,\"items\":[",revision);history_append(&b,intro);
 for(int i=0;i<count&&!b.failed;i++){
  HistoryRow*r=rows+i;char meta[256];snprintf(meta,sizeof(meta),"%s{\"key\":\"%lu:%lu\",\"grouped\":%s,\"bytes\":%I64u,\"name\":",i?",":"",(unsigned long)r->type,(unsigned long)r->id,r->isGroup?"true":"false",r->bytes);history_append(&b,meta);
  char *name=utf8(r->name),*path=utf8(r->path);if(!name||!path)b.failed=1;else{history_quoted(&b,name);history_append(&b,",\"path\":");history_quoted(&b,path);history_append(&b,"}");}free(name);free(path);
 }
 free(rows);history_append(&b,"]}");if(b.failed){free(b.data);strcpy(err,"Cannot serialize completed history");return NULL;}return b.data;
}
#include "history_folder.h"
static int history_open(HistoryKey key,char *err){
 wchar_t stored[1024];if(!adapter_history_path(key.type,key.id,stored,err))return 0;
 return history_open_path(stored,err);
}
#include "history_delete.h"
static void history_remove_response(SOCKET s,HistoryKey*keys,int count,int files){
 HistoryMutation entries[100],eligible[100];int map[100],eligibleCount=0;
 memset(entries,0,sizeof(entries));char batchError[512]={0};
 for(int i=0;i<count;i++){entries[i].type=keys[i].type;entries[i].id=keys[i].id;}
 if(!files){
  if(!adapter_history_remove_batch(entries,count,batchError))for(int i=0;i<count;i++)snprintf(entries[i].error,512,"%s",batchError);
 }else{
  wchar_t root[4096],resources[4096];if(!adapter_root(root,4096,batchError)){response_error(s,500,batchError);return;}
  if(wcslen(root)+12>=4096){response_error(s,500,"Resource cache directory is too long");return;}swprintf(resources,4096,L"%ls\\resources",root);
  HistoryRow*rows=NULL;int total=0;if(!adapter_history_list(&rows,&total,batchError)){response_error(s,500,batchError);return;}
  HistoryDeleteItem*items=calloc(count,sizeof(*items));if(!items){free(rows);response_error(s,500,"Cannot allocate batch deletion");return;}
  for(int i=0;i<count;i++)for(int j=0;j<total;j++)if(keys[i].type==rows[j].type&&keys[i].id==rows[j].id){
   if(rows[j].isGroup)strcpy(entries[i].error,"版本任务只能清理记录；卸载已安装版本请使用游戏管理，共享缓存不按单个版本删除");
   else if(!cache_descendant(resources,rows[j].path))strcpy(entries[i].error,"该文件不在盒子当前的 resources 下载缓存中，已停止删除");
   else items[i].stored=rows[j].path;break;
  }
  history_delete_items(items,count);
  for(int i=0;i<count;i++){
   if(*entries[i].error)continue;
   if(!items[i].ok){snprintf(entries[i].error,512,"%s",items[i].error);continue;}
   batchError[0]=0;
   map[eligibleCount]=i;eligible[eligibleCount++]=entries[i];
  }
  if(eligibleCount){
   batchError[0]=0;
   if(adapter_history_remove_batch(eligible,eligibleCount,batchError))for(int k=0;k<eligibleCount;k++){int i=map[k];entries[i].ok=eligible[k].ok;if(!eligible[k].ok)snprintf(entries[i].error,512,"文件已删除，但移除记录失败：%.350s",eligible[k].error);}
   else for(int k=0;k<eligibleCount;k++)snprintf(entries[map[k]].error,512,"文件已删除，但移除记录失败：%.350s",batchError);
  }
  free(items);free(rows);
 }
 HistoryJson b={0};history_append(&b,"{\"results\":[");
 for(int i=0;i<count;i++){char meta[160];snprintf(meta,sizeof(meta),"%s{\"key\":\"%lu:%lu\",\"removed\":%s,\"error\":",i?",":"",(unsigned long)keys[i].type,(unsigned long)keys[i].id,entries[i].ok?"true":"false");history_append(&b,meta);history_quoted(&b,entries[i].error);history_append(&b,"}");}
 history_append(&b,"]}");if(b.failed)response_error(s,500,"Cannot serialize removal results");else response(s,200,"application/json; charset=utf-8",b.data);free(b.data);
}
