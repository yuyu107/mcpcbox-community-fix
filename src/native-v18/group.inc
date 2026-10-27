// Native UI projection: keep real engine tasks intact, expose one version row.
typedef struct Group {
 struct Group *next;LONG id;int state;unsigned long long total,done;
 BoxRequest request;BoxString completedExtra;
} Group;
static Group *groups;
static LONG nextGroup=0x62000000;
static int groupReady;
void adapter_seed(unsigned long seed){nextId=0x63000000|(seed&0x007fffff);nextGroup=0x62000000|((seed>>1)&0x007fffff);}
static void *volatile originalMaxSize;
void *pause_resume,*resume_resume,*delete_resume,*delete_done_resume,*percent_resume;
void *pause_seh,*resume_seh,*delete_seh,*delete_done_seh,*percent_seh;
extern void NativeGroupCancel(void);
static Group *find_group(LONG id){for(Group*g=groups;g;g=g->next)if(g->id==id)return g;return NULL;}
static int child_row(uint32_t type,LONG id){uint32_t prefix=(uint32_t)id&0xff000000u;Task*t=type==3&&(prefix==0x63000000u||prefix==0x61000000u)?find_task(id,0,NULL):NULL;return t&&t->groupId;}
static void refresh_groups(void){if(uiWindow)PostMessageW(uiWindow,UI_MESSAGE+1,UI_COOKIE,0);}
int adapter_group_begin(const wchar_t *name,const wchar_t *directory,unsigned long long total,char *err){
 if(!groupReady){strcpy(err,"Cannot attach aggregate size provider to native center");return 0;}
 Group*g=calloc(1,sizeof(*g));if(!g){strcpy(err,"Cannot allocate native version group");return 0;}
 g->id=InterlockedIncrement(&nextGroup);g->state=1;g->total=total;
 box_ctor(&g->request.name);box_ctor(&g->request.url);box_ctor(&g->request.target);box_ctor(&g->request.md5);box_ctor(&g->completedExtra);
 g->request.type=3;g->request.id=g->id;box_assign(&g->request.name,name);box_assign(&g->request.target,directory);
 EnterCriticalSection(&registryLock);g->next=groups;groups=g;currentGroup=g->id;LeaveCriticalSection(&registryLock);
 post_marker(0x64000000);refresh_groups();return 1;
}
void adapter_group_progress(unsigned long long done){EnterCriticalSection(&registryLock);Group*g=find_group(currentGroup);if(g)g->done=done>g->total?g->total:done;LeaveCriticalSection(&registryLock);}
void adapter_group_finish(int state){EnterCriticalSection(&registryLock);Group*g=find_group(currentGroup);if(g&&g->state!=5){g->state=state;if(state==4)g->done=g->total;}LeaveCriticalSection(&registryLock);post_marker(0x64000000);refresh_groups();}
int adapter_group_paused(void){EnterCriticalSection(&registryLock);Group*g=find_group(currentGroup);int paused=g&&g->state==2;LeaveCriticalSection(&registryLock);return paused;}
static void group_pause_new(Task*t){EnterCriticalSection(&registryLock);Group*g=find_group(t->groupId);int pause=g&&(g->state==2||g->state==5);LONG id=t->nativeId;LeaveCriticalSection(&registryLock);if(pause&&id)((int(__cdecl*)(LONG))iat(0x30a628))(id);}
unsigned long long __cdecl AdapterMaxSize(LONG id){
 EnterCriticalSection(&registryLock);Group*g=find_group(id);unsigned long long total=g?g->total:0;LeaveCriticalSection(&registryLock);
 return g?total:((unsigned long long(__cdecl*)(LONG))originalMaxSize)(id);
}
// Construct views with the exact MSVC90 vector ABI; the original copy constructors
// create independent, correctly owned strings and callbacks for the caller.
void *__stdcall AdapterSnapshot(void *manager,void *out,int completed){
 unsigned stride=completed?0x7c:0x88,offset=completed?0x64:0x34;
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a808))((char*)manager+4);
 EnterCriticalSection(&registryLock);
 uintptr_t begin=*(uintptr_t*)((char*)manager+offset+12),end=*(uintptr_t*)((char*)manager+offset+16);
 size_t count=(end>=begin&&(end-begin)%stride==0&&(end-begin)/stride<200000)?(end-begin)/stride:0,extra=0;
 for(Group*g=groups;g;g=g->next)if(completed?g->state==4:(g->state>=1&&g->state<=2))extra++;
 // The native completed item box is hidden behind the embedded history UI.
 // Keep just one presence row so the host's empty/nonempty routing works,
 // instead of generating a second full invisible list on every notification.
 unsigned char *records=malloc((completed?1:count+extra+1)*stride);size_t used=0;
 if(records){
  if(completed)for(Group*g=groups;g;g=g->next)if(g->state==4){
   unsigned char*r=records;memset(r,0,stride);memcpy(r,&g->request,8);memcpy(r+8,&g->request.name,sizeof(BoxString));memcpy(r+0x24,&g->request.target,sizeof(BoxString));memcpy(r+0x40,&g->request.url,sizeof(BoxString));*(uint32_t*)(r+0x5c)=g->total>0xffffffffULL?0xffffffffU:(uint32_t)g->total;memcpy(r+0x60,&g->completedExtra,sizeof(BoxString));used=1;break;
  }
  for(size_t i=0;i<count&&(!completed||!used);i++){unsigned char*r=(unsigned char*)begin+i*stride;if(!child_row(*(uint32_t*)r,*(LONG*)(r+4))){memcpy(records+used*stride,r,stride);used++;}}
  for(Group*g=groups;g;g=g->next)if(!completed&&(g->state>=1&&g->state<=2)){
   unsigned char*r=records+used++*stride;memset(r,0,stride);
   if(completed){memcpy(r,&g->request,0x5c);*(uint32_t*)(r+0x5c)=g->total>0xffffffffULL?0xffffffffU:(uint32_t)g->total;memcpy(r+0x60,&g->completedExtra,sizeof(BoxString));}
   else{memcpy(r,&g->request,sizeof(BoxRequest));*(LONG*)(r+0x78)=g->id;*(uint32_t*)(r+0x84)=g->state;}
  }
 }
 uintptr_t view[6]={0},proxy=(uintptr_t)view;view[0]=(uintptr_t)&proxy;view[3]=(uintptr_t)records;view[4]=view[3]+used*stride;view[5]=view[4];
 ((void*(__attribute__((thiscall))*)(void*,void*))at(completed?0x2f7e0:0xba120))(view,out);
 free(records);LeaveCriticalSection(&registryLock);
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a800))((char*)manager+4);return out;
}
void *__stdcall AdapterDownloading(void *manager,void *out){return AdapterSnapshot(manager,out,0);}
void *__stdcall AdapterDownloaded(void *manager,void *out){return AdapterSnapshot(manager,out,1);}
int __stdcall AdapterPercent(void *manager,LONG id,int type){
 (void)manager;(void)type;EnterCriticalSection(&registryLock);Group*g=find_group(id);int percent=0;
 if(g&&g->total){percent=(int)(g->done*100/g->total);if(percent>=100&&g->state!=4)percent=99;}
 if(g&&g->state==4)percent=100;LeaveCriticalSection(&registryLock);return percent;
}
int __attribute__((fastcall)) AdapterCount(void *manager,void *unused){
 (void)unused;int count=0;
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a808))((char*)manager+4);EnterCriticalSection(&registryLock);
 uintptr_t begin=*(uintptr_t*)((char*)manager+0x40),end=*(uintptr_t*)((char*)manager+0x44);
 if(end>=begin&&(end-begin)%0x88==0&&(end-begin)/0x88<200000)for(uintptr_t row=begin;row<end;row+=0x88)if(!child_row(*(uint32_t*)row,*(LONG*)(row+4)))count++;
 for(Group*g=groups;g;g=g->next)if(g->state>=1&&g->state<=2)count++;
 LeaveCriticalSection(&registryLock);((void(__attribute__((thiscall))*)(void*))iat(0x30a800))((char*)manager+4);return count;
}
__attribute__((naked,used)) void AdapterCountGate(void){__asm__ __volatile__("mov %eax,%ecx\n\txor %edx,%edx\n\tjmp @AdapterCount@8\n\t");}
static void group_control(LONG id,int action){
 LONG nativeIds[16];int n=0,cancel=0;
 EnterCriticalSection(&registryLock);Group*g=find_group(id);
 if(g){
  if(action==1&&g->state==1)g->state=2;
  else if(action==2&&g->state==2)g->state=1;
  else if(action==3){g->state=5;cancel=id==currentGroup;}
  else if(action==4&&g->state==4)g->state=6;
  if(action!=4)for(Task*t=registry;t;t=t->next)if(t->groupId==id&&!t->result&&!t->cancelled&&t->nativeId&&n<16)nativeIds[n++]=t->nativeId;
 }
 LeaveCriticalSection(&registryLock);
 if(cancel)NativeGroupCancel();
 for(int i=0;i<n;i++)((int(__cdecl*)(LONG))iat(action==2?0x30a618:0x30a628))(nativeIds[i]);
 // Controls run on the original UI/download task threads; the native notify
 // methods use the same observer callbacks as ordinary center changes.
 if(g)refresh_groups();
}
void __attribute__((fastcall)) AdapterPause(void *manager,void *unused,LONG id,int type){(void)manager;(void)unused;(void)type;group_control(id,1);}
void __attribute__((fastcall)) AdapterResume(void *manager,void *unused,LONG id,int type){(void)manager;(void)unused;(void)type;group_control(id,2);}
void __attribute__((fastcall)) AdapterDelete(void *manager,void *unused,LONG id,int type){(void)manager;(void)unused;(void)type;group_control(id,3);}
void __attribute__((fastcall)) AdapterDeleteDone(void *manager,void *unused,LONG id,int type){(void)manager;(void)unused;(void)type;group_control(id,4);}

#define CONTROL_GATE(name,target,resume) \
 __attribute__((naked,used)) void name(void){__asm__ __volatile__( \
 "mov 4(%esp),%eax\n\tand $0xff000000,%eax\n\tcmp $0x62000000,%eax\n\tjne 1f\n\tjmp " target "\n\t" \
 "1: push %ebp\n\tmov %esp,%ebp\n\tand $-8,%esp\n\tjmp *_" #resume "\n\t");}
CONTROL_GATE(AdapterPauseGate,"@AdapterPause@16",pause_resume)
CONTROL_GATE(AdapterResumeGate,"@AdapterResume@16",resume_resume)
CONTROL_GATE(AdapterDeleteGate,"@AdapterDelete@16",delete_resume)
CONTROL_GATE(AdapterDeleteDoneGate,"@AdapterDeleteDone@16",delete_done_resume)
__attribute__((naked,used)) void AdapterPercentGate(void){__asm__ __volatile__(
 "mov 8(%esp),%eax\n\tand $0xff000000,%eax\n\tcmp $0x62000000,%eax\n\tjne 1f\n\tjmp _AdapterPercent@12\n\t"
 "1: push $-1\n\tpush _percent_seh\n\tjmp *_percent_resume\n\t");}
static void *volatile badge_get_manager;
extern void *badge_resume;
static void group_init(void){badge_get_manager=at(0x98f0);badge_resume=at(0x75637);
 originalMaxSize=iat(0x30a608);DWORD old;
 if(VirtualProtect(at(0x30a608),4,PAGE_READWRITE,&old)){*(void**)at(0x30a608)=AdapterMaxSize;groupReady=1;VirtualProtect(at(0x30a608),4,old,&old);}
 pause_resume=at(0xb9916);resume_resume=at(0xb9b06);delete_resume=at(0xb9646);delete_done_resume=at(0xb9d46);
 percent_resume=at(0xb82b7);percent_seh=at(0x2f11e8);
}


void *badge_resume;
__attribute__((naked,used)) void AdapterBadgeGate(void){__asm__ __volatile__(
 "call *_badge_get_manager\n\tpush %eax\n\tpush %ecx\n\tpush %edx\n\tmov %eax,%ecx\n\txor %edx,%edx\n\tcall @AdapterCount@8\n\tmov %eax,%ebx\n\tpop %edx\n\tpop %ecx\n\tpop %eax\n\tjmp *_badge_resume\n\t");}
