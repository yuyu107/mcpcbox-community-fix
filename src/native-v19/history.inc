// All reads/erasures of the original completed vector run on MCPCBoxThread3.
// File deletion is a separate, explicit server-side action; record-only remains metadata-only.
typedef struct HistoryCommand {
 struct HistoryCommand *next;LONG refs;HANDLE event;int operation,ok,count;
 uint32_t type,id;HistoryRow *rows;wchar_t path[1024];char error[512];
} HistoryCommand;
static HistoryCommand *historyHead,*historyTail;
static void *volatile history_copy_fn;
// Original completed-record copy loop: EAX=destination, ECX=first, EBX=last.
// It assigns every owned MSVC string via the original runtime, never memcpy.
__attribute__((naked,noipa,used)) void history_copy_records(void *dest,void *first,void *last){__asm__ __volatile__(
 "push %ebx\n\tmov 8(%esp),%eax\n\tmov 12(%esp),%ecx\n\tmov 16(%esp),%ebx\n\tcall *_history_copy_fn\n\tpop %ebx\n\tret\n\t");}
void HistoryBatchExecute(void *manager,HistoryMutation *entries,int count){
 if(count<1||count>100)return;
 for(int i=0;i<count;i++){entries[i].ok=0;strcpy(entries[i].error,"Completed record no longer exists");}
 uintptr_t begin=*(uintptr_t*)((char*)manager+0x58),end=*(uintptr_t*)((char*)manager+0x5c);
 if(count<1||count>100||end<begin||(end-begin)%0x7c||(end-begin)/0x7c>10000)return;
 size_t total=(end-begin)/0x7c;unsigned char*skip=calloc(total+1,1);
 if(!skip){for(int i=0;i<count;i++)strcpy(entries[i].error,"Cannot allocate batch history compaction");return;}
 int ordinary=0,changed=0;
 EnterCriticalSection(&registryLock);
 for(size_t row=0;row<total;row++){
  uintptr_t atRow=begin+row*0x7c;
  for(int i=0;i<count;i++)if(entries[i].type==*(DWORD*)atRow&&entries[i].id==*(DWORD*)(atRow+4)){
   if(child_row(entries[i].type,entries[i].id)){strcpy(entries[i].error,"Internal child record must be managed through its complete version");continue;}
   if(!skip[row]){skip[row]=1;ordinary++;}entries[i].ok=1;entries[i].error[0]=0;
  }
 }
 for(Group*g=groups;g;g=g->next)if(g->state==4)for(int i=0;i<count;i++)if(entries[i].type==3&&entries[i].id==(uint32_t)g->id){g->state=6;entries[i].ok=1;entries[i].error[0]=0;changed=1;}
 LeaveCriticalSection(&registryLock);
 if(ordinary){
  history_copy_fn=at(0x2cf80);uintptr_t write=begin;size_t read=0;
  while(read<total){if(skip[read]){read++;continue;}size_t first=read;while(read<total&&!skip[read])read++;
   uintptr_t source=begin+first*0x7c,last=begin+read*0x7c;
   if(write!=source)history_copy_records((void*)write,(void*)source,(void*)last);write+=last-source;
  }
  for(uintptr_t row=write;row<end;row+=0x7c)((void(__stdcall*)(void*))at(0x6a80))((void*)row);
  *(uintptr_t*)((char*)manager+0x5c)=write;
  // Match DeleteFileResult's publication, but only once for the whole batch.
  ((void(__attribute__((thiscall))*)(void*,int))at(0xb93c0))(manager,0x10);changed=1;
 }
 free(skip);if(changed){NativeHistoryChanged();refresh_groups();}
}
static void history_release(HistoryCommand*c){if(!InterlockedDecrement(&c->refs)){CloseHandle(c->event);free(c->rows);free(c);}}
static int history_copy(wchar_t *out,size_t cap,const wchar_t*in){size_t n=0;while(n<cap&&in[n])n++;if(n==cap)return 0;memcpy(out,in,(n+1)*sizeof(wchar_t));return 1;}
static int history_fill(HistoryRow *out,uint32_t type,uint32_t id,int grouped,unsigned long long bytes,BoxString*name,BoxString*path){
 memset(out,0,sizeof(*out));out->type=type;out->id=id;out->isGroup=grouped;out->bytes=bytes;
 return history_copy(out->name,512,box_text(name))&&history_copy(out->path,1024,box_text(path));
}
// Callable separately by machine-code tests; production invokes only in history_dispatch.
void HistoryExecute(void *manager,HistoryCommand*c){
 if(c->operation==4){HistoryBatchExecute(manager,(HistoryMutation*)c->rows,c->count);c->ok=1;return;}
 uintptr_t begin=*(uintptr_t*)((char*)manager+(c->operation==1?0x70:0x58)),end=*(uintptr_t*)((char*)manager+(c->operation==1?0x74:0x5c));
 if(end<begin||(end-begin)%0x7c||(end-begin)/0x7c>10000){strcpy(c->error,"Invalid or oversized completed history");return;}
 if(c->operation==1){
  EnterCriticalSection(&registryLock);size_t capacity=(end-begin)/0x7c;
  for(Group*g=groups;g;g=g->next)if(g->state==4)capacity++;
  if(capacity>10000){LeaveCriticalSection(&registryLock);strcpy(c->error,"Too many completed records");return;}
  c->rows=calloc(capacity+1,sizeof(HistoryRow));
  if(!c->rows){LeaveCriticalSection(&registryLock);strcpy(c->error,"Cannot allocate completed history");return;}
  for(uintptr_t row=begin;row<end;row+=0x7c)if(!child_row(*(DWORD*)row,*(LONG*)(row+4))){
   if(!history_fill(c->rows+c->count,*(DWORD*)row,*(DWORD*)(row+4),0,*(DWORD*)(row+0x5c),(BoxString*)(row+8),(BoxString*)(row+0x24)))goto long_record;
   c->count++;
  }
  for(Group*g=groups;g;g=g->next)if(g->state==4){
   if(!history_fill(c->rows+c->count,3,g->id,1,g->total,&g->request.name,&g->request.target))goto long_record;
   c->count++;
  }
  LeaveCriticalSection(&registryLock);c->ok=1;return;
long_record:
  LeaveCriticalSection(&registryLock);strcpy(c->error,"Completed record name or path exceeds capacity");return;
 }
 EnterCriticalSection(&registryLock);Group*g=c->type==3?find_group(c->id):NULL;
 if(g){
  if(g->state!=4){LeaveCriticalSection(&registryLock);strcpy(c->error,"Completed record no longer exists");return;}
  if(c->operation==2){g->state=6;c->ok=1;}
  else if(c->operation==3){c->count=1;c->ok=history_copy(c->path,1024,box_text(&g->request.target));}
  LeaveCriticalSection(&registryLock);if(c->ok){if(c->operation==2){NativeHistoryChanged();refresh_groups();}}else strcpy(c->error,"Cannot read completed directory");return;
 }
 LeaveCriticalSection(&registryLock);
 uintptr_t found=0;
 for(uintptr_t row=begin;row<end;row+=0x7c)if(*(DWORD*)row==c->type&&*(DWORD*)(row+4)==c->id){
  EnterCriticalSection(&registryLock);int child=child_row(c->type,c->id);LeaveCriticalSection(&registryLock);if(!child)found=row;break;
 }
 if(!found){strcpy(c->error,"Completed record no longer exists");return;}
 if(c->operation==3){c->ok=history_copy(c->path,1024,box_text((BoxString*)(found+0x24)));if(!c->ok)strcpy(c->error,"Cannot read completed path");return;}
 if(c->operation!=2){strcpy(c->error,"Invalid completed history operation");return;}
 // Original DeleteFileResult removes metadata and schedules its save/refresh.
 // Never call DeleteDownloadedFile (0xb9d40), DeleteFileW or a directory remover.
 ((void(__attribute__((thiscall))*)(void*,LONG,int,int))at(0xb9f00))(manager,c->id,c->type,1);
 begin=*(uintptr_t*)((char*)manager+0x58);end=*(uintptr_t*)((char*)manager+0x5c);
 c->ok=1;for(uintptr_t row=begin;row<end;row+=0x7c)if(*(DWORD*)row==c->type&&*(DWORD*)(row+4)==c->id){c->ok=0;strcpy(c->error,"Original manager did not remove the completed record");break;}
}
static void history_dispatch(void *manager){
 for(;;){EnterCriticalSection(&registryLock);HistoryCommand*c=historyHead;if(c){historyHead=c->next;if(!historyHead)historyTail=NULL;}LeaveCriticalSection(&registryLock);if(!c)break;
  HistoryExecute(manager,c);SetEvent(c->event);history_release(c);
 }
}
static HistoryCommand *history_call_data(int operation,uint32_t type,uint32_t id,const HistoryMutation*batch,int count,char *err){
 if(!ui_call(4,NULL,0,err))return NULL;
 HistoryCommand*c=calloc(1,sizeof(*c));if(!c){strcpy(err,"Cannot allocate history request");return NULL;}
 if(batch){if(count<1||count>100){free(c);strcpy(err,"Invalid history batch size");return NULL;}c->rows=(HistoryRow*)malloc(count*sizeof(HistoryMutation));if(!c->rows){free(c);strcpy(err,"Cannot copy history batch");return NULL;}memcpy(c->rows,batch,count*sizeof(HistoryMutation));c->count=count;}
 c->refs=2;c->operation=operation;c->type=type;c->id=id;c->event=CreateEventW(NULL,TRUE,FALSE,NULL);
 if(!c->event){free(c->rows);free(c);strcpy(err,"Cannot create history event");return NULL;}
 EnterCriticalSection(&registryLock);if(historyTail)historyTail->next=c;else historyHead=c;historyTail=c;LeaveCriticalSection(&registryLock);
 if(!post_marker(0x64000001)){
  EnterCriticalSection(&registryLock);HistoryCommand *prev=NULL,*p=historyHead;while(p&&p!=c){prev=p;p=p->next;}
  if(p){if(prev)prev->next=p->next;else historyHead=p->next;if(historyTail==p)historyTail=prev;}
  LeaveCriticalSection(&registryLock);if(p){history_release(c);history_release(c);strcpy(err,"Cannot submit history request");return NULL;}
 }
 DWORD waited=WaitForSingleObject(c->event,10000);
 if(waited!=WAIT_OBJECT_0||!c->ok){snprintf(err,512,"%s",waited!=WAIT_OBJECT_0?"History request timed out; refresh the list to check the result":*c->error?c->error:"History operation failed");history_release(c);return NULL;}
 return c;
}
static HistoryCommand *history_call(int operation,uint32_t type,uint32_t id,char *err){return history_call_data(operation,type,id,NULL,0,err);}
int adapter_history_remove_batch(HistoryMutation*entries,int count,char*err){HistoryCommand*c=history_call_data(4,0,0,entries,count,err);if(!c)return 0;memcpy(entries,c->rows,count*sizeof(HistoryMutation));history_release(c);return 1;}
int adapter_history_list(HistoryRow **rows,int *count,char *err){
 // The original manager publishes this immutable UI snapshot under manager+4.
 // Reads need no UI message, download-thread marker, completion event or sweep.
 if(!(*(DWORD*)at(0x3c8974)&1)||!*(BYTE*)at(0x3c8a18)){strcpy(err,"Native download manager is not ready");return 0;}
 void *manager=at(0x3c8978);HistoryCommand c;memset(&c,0,sizeof(c));c.operation=1;
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a808))((char*)manager+4);
 HistoryExecute(manager,&c);
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a800))((char*)manager+4);
 if(!c.ok){free(c.rows);snprintf(err,512,"%s",c.error);return 0;}*rows=c.rows;*count=c.count;return 1;
}
unsigned long adapter_history_revision(void){
 if(!(*(DWORD*)at(0x3c8974)&1)||!*(BYTE*)at(0x3c8a18))return 0;
 void *manager=at(0x3c8978);uint32_t hash=2166136261u;
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a808))((char*)manager+4);
 uintptr_t begin=*(uintptr_t*)((char*)manager+0x70),end=*(uintptr_t*)((char*)manager+0x74);
 if(end>=begin&&(end-begin)%0x7c==0&&(end-begin)/0x7c<=10000)for(uintptr_t row=begin;row<end;row+=0x7c){hash=(hash^*(DWORD*)row)*16777619u;hash=(hash^*(DWORD*)(row+4))*16777619u;}
 EnterCriticalSection(&registryLock);for(Group*g=groups;g;g=g->next)if(g->state==4)hash=(hash^(uint32_t)g->id)*16777619u;LeaveCriticalSection(&registryLock);
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a800))((char*)manager+4);return hash?hash:1;
}
int adapter_history_remove(uint32_t type,uint32_t id,char *err){HistoryCommand*c=history_call(2,type,id,err);if(!c)return 0;history_release(c);return 1;}
int adapter_history_target(uint32_t type,uint32_t id,wchar_t *out,int *grouped,char *err){HistoryCommand*c=history_call(3,type,id,err);if(!c)return 0;wcscpy(out,c->path);if(grouped)*grouped=c->count;history_release(c);return 1;}
int adapter_history_path(uint32_t type,uint32_t id,wchar_t *out,char *err){return adapter_history_target(type,id,out,NULL,err);}

int adapter_history_available(void){return (*(DWORD*)at(0x3c8974)&1)&&*(BYTE*)at(0x3c8a18);}
int adapter_history_attach(char*err){return ui_call(4,NULL,0,err);}
int adapter_group_export(HistoryRow**out,int*count,char*err){
 *out=NULL;*count=0;EnterCriticalSection(&registryLock);int n=0;
 for(Group*g=groups;g;g=g->next)if(g->state==4)n++;
 if(n>10000){LeaveCriticalSection(&registryLock);strcpy(err,"Completed group limit exceeded");return 0;}
 HistoryRow*rows=calloc(n+1,sizeof(*rows));if(!rows){LeaveCriticalSection(&registryLock);strcpy(err,"Cannot snapshot completed groups");return 0;}
 for(Group*g=groups;g;g=g->next)if(g->state==4){
  if(!history_fill(rows+*count,3,g->id,1,g->total,&g->request.name,&g->request.target)){free(rows);LeaveCriticalSection(&registryLock);strcpy(err,"Stored group name or path too long");return 0;}(*count)++;
 }
 LeaveCriticalSection(&registryLock);*out=rows;return 1;
}
int adapter_group_restore(const HistoryRow*rows,int count,char*err){
 if(count<0||count>10000){strcpy(err,"Invalid stored group count");return 0;}
 for(int i=0;i<count;i++){
  if(rows[i].type!=3||!rows[i].isGroup||(rows[i].id&0xff000000)!=0x62000000||!rows[i].name[0]||rows[i].name[511]||rows[i].path[1023]){strcpy(err,"Invalid stored group");return 0;}
  for(int j=0;j<i;j++)if(rows[j].id==rows[i].id){strcpy(err,"Duplicate stored group ID");return 0;}
 }
 Group*head=NULL,*tail=NULL;
 for(int i=0;i<count;i++){
  Group*g=calloc(1,sizeof(*g));if(!g){while(head){Group*next=head->next;box_dtor(&head->request.name);box_dtor(&head->request.url);box_dtor(&head->request.target);box_dtor(&head->request.md5);box_dtor(&head->completedExtra);free(head);head=next;}strcpy(err,"Cannot restore completed groups");return 0;}
  g->id=rows[i].id;g->state=4;g->total=g->done=rows[i].bytes;g->request.type=3;g->request.id=g->id;
  box_ctor(&g->request.name);box_ctor(&g->request.url);box_ctor(&g->request.target);box_ctor(&g->request.md5);box_ctor(&g->completedExtra);
  box_assign(&g->request.name,rows[i].name);box_assign(&g->request.target,rows[i].path);
  if(tail)tail->next=g;else head=g;tail=g;
 }
 EnterCriticalSection(&registryLock);Group*accepted=NULL,*last=NULL;
 while(head){Group*g=head;head=head->next;g->next=NULL;
  if(find_group(g->id)){box_dtor(&g->request.name);box_dtor(&g->request.url);box_dtor(&g->request.target);box_dtor(&g->request.md5);box_dtor(&g->completedExtra);free(g);continue;}
  if(last)last->next=g;else accepted=g;last=g;
 }
 if(last){last->next=groups;groups=accepted;}LeaveCriticalSection(&registryLock);refresh_groups();return 1;
}
