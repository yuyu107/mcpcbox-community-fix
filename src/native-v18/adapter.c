#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "adapter.h"

// ABI derived from the exact v4 MCPCBox.exe, NOT from private mcoption.dll offsets.
typedef struct { unsigned char bytes[0x1c]; } BoxString;
typedef struct { uint32_t type,id;BoxString name,url,target,md5; } BoxRequest;
typedef struct { uint32_t state,invoke; } BoxCallback;
typedef char assert_request_size[(sizeof(BoxRequest)==0x78)?1:-1];
typedef struct Task {
 struct Task *next;
 LONG id,nativeId,queued,result,cancelled,error,retired;
 void *manager;
 LONG groupId;
 HANDLE event;
 wchar_t target[512];
} Task;
static CRITICAL_SECTION registryLock,uiLock;
static Task *registry;
static volatile LONG nextId=0x61000000;
static uintptr_t imageBase;
static LONG currentGroup;
static void group_init(void);
static void group_pause_new(Task *t);
static void *volatile native_create_fn;
static HWND uiWindow;
static WNDPROC oldWndProc;
static int uiTimedOut;
static struct {int operation,ok;wchar_t path[4096];char error[512];} ui;
void *resource_resume,*resource_seh,*success_resume,*error_resume;
#define UI_MESSAGE (WM_APP+0x371)
#define UI_COOKIE 0x4f463731

static void *at(uint32_t rva){return (void*)(imageBase+rva);}
static void *iat(uint32_t rva){return *(void**)at(rva);}
static void box_ctor(BoxString *s){((void(__attribute__((thiscall))*)(void*))iat(0x309a98))(s);}
static void box_dtor(BoxString *s){((void(__attribute__((thiscall))*)(void*))iat(0x3099d8))(s);}
static void box_assign(BoxString *s,const wchar_t *text){((void(__attribute__((thiscall))*)(void*,const wchar_t*))iat(0x309c5c))(s,text);}
static const wchar_t *box_text(BoxString *s){uint32_t *w=(uint32_t*)s;return w[6]>=8?(const wchar_t*)(uintptr_t)w[1]:(const wchar_t*)&w[1];}
static void *option(void){return ((void*(__cdecl*)(void))iat(0x30a3b8))();}
static Task *find_task(LONG id,LONG nativeId,const wchar_t *target){for(Task *t=registry;t;t=t->next)if((id&&t->id==id)||(nativeId&&t->nativeId==nativeId)||(target&&!wcscmp(t->target,target)))return t;return NULL;}

static int post_marker(LONG logicalId);
static void history_dispatch(void *manager);
static void sweep_stale(void *manager){
 // Called only on the original task thread. Retire orphaned v7/v8 child
 // requests left paused in the launcher's persistent task list after exit.
 for(int pass=0;pass<128;pass++){
  uintptr_t begin=*(uintptr_t*)((char*)manager+0x28),end=*(uintptr_t*)((char*)manager+0x2c);LONG stale=0;
  if(end<begin||(end-begin)%0x88||(end-begin)/0x88>=200000)break;
  for(uintptr_t row=begin;row<end;row+=0x88){LONG id=*(LONG*)(row+4);unsigned prefix=(unsigned)id&0xff000000;
   if(*(DWORD*)row!=3||(prefix!=0x61000000&&prefix!=0x63000000)||wcsncmp(box_text((BoxString*)(row+8)),L"Minecraft ",10))continue;
   EnterCriticalSection(&registryLock);int owned=find_task(id,0,NULL)!=NULL;LeaveCriticalSection(&registryLock);
   if(!owned){stale=id;break;}
  }
  if(!stale)break;
  ((void(__attribute__((thiscall))*)(void*,LONG,int))at(0xb9640))(manager,stale,3);
  if(*(uintptr_t*)((char*)manager+0x28)==begin&&*(uintptr_t*)((char*)manager+0x2c)==end)break;
 }
}

static void sweep_completed(void *manager){
 // The original center clones its entire completed list on every completion.
 // Discard consumed child-row metadata using its own erase/refresh method.
 // This callback does not delete the downloaded file.
 for(int pass=0;pass<128;pass++){
  uintptr_t begin=*(uintptr_t*)((char*)manager+0x58),end=*(uintptr_t*)((char*)manager+0x5c);LONG id=0;
  if(end<begin||(end-begin)%0x7c||(end-begin)/0x7c>=200000)break;
  for(uintptr_t row=begin;row<end;row+=0x7c){
   if(*(DWORD*)row!=3)continue;
   EnterCriticalSection(&registryLock);Task*t=find_task(*(LONG*)(row+4),0,NULL);int consumed=t&&t->groupId&&t->result>0;LeaveCriticalSection(&registryLock);
   if(consumed){id=*(LONG*)(row+4);break;}
  }
  if(!id)break;
  ((void(__attribute__((thiscall))*)(void*,LONG,int,int))at(0xb9f00))(manager,id,3,1);
  if(*(uintptr_t*)((char*)manager+0x58)==begin&&*(uintptr_t*)((char*)manager+0x5c)==end)break;
 }
}

// We enter this method on the same MCPCBoxThread as ordinary native resource requests.
// Normal requests retain the original method, including its cache/MD5 behavior.
__attribute__((naked,noipa,used)) void create_native(void *manager,BoxRequest *request,BoxCallback *callback){
 __asm__ __volatile__("push %edi\n\tmov 8(%esp),%edi\n\tmov 12(%esp),%ecx\n\tmov 16(%esp),%eax\n\tpush %eax\n\tcall *_native_create_fn\n\tpop %edi\n\tret\n\t");
}

void __attribute__((fastcall,used)) AdapterResource(void *manager,void *unused,BoxRequest request,BoxCallback callback){
 (void)unused;history_dispatch(manager);sweep_stale(manager);sweep_completed(manager);
 EnterCriticalSection(&registryLock);Task *t=find_task((LONG)request.id,0,NULL);int cancelled=!t||t->cancelled;LeaveCriticalSection(&registryLock);
 if(!cancelled){
  create_native(manager,&request,&callback);
  // CDownloadManager owns this lock and the vector of 0x88-byte native records.
  ((void(__attribute__((thiscall))*)(void*))iat(0x30a808))((char*)manager+4);
  uintptr_t begin=*(uintptr_t*)((char*)manager+0x28),end=*(uintptr_t*)((char*)manager+0x2c);LONG nativeId=0;
  if(end>=begin&&(end-begin)%0x88==0&&(end-begin)/0x88<200000)
   for(uintptr_t row=begin;row<end;row+=0x88)if(*(uint32_t*)row==request.type&&*(uint32_t*)(row+4)==request.id){nativeId=*(LONG*)(row+0x78);break;}
  ((void(__attribute__((thiscall))*)(void*))iat(0x30a800))((char*)manager+4);
  EnterCriticalSection(&registryLock);if(t){t->manager=manager;if(nativeId)t->nativeId=nativeId;t->queued=1;if(!t->nativeId&&!t->result){t->result=-1;t->error=-1001;if(t->event)SetEvent(t->event);}}LeaveCriticalSection(&registryLock);
  if(t)group_pause_new(t);
 }else if(t){
  // This dispatch runs on the same original download thread as task creation.
  if(t->nativeId)((void(__attribute__((thiscall))*)(void*,LONG,int))at(0xb9640))(manager,t->id,3);
  EnterCriticalSection(&registryLock);t->queued=-1;t->result=-1;if(t->event)SetEvent(t->event);LeaveCriticalSection(&registryLock);
 }
 // These are the argument copies on the original method's stack; release them exactly once.
 ((void(__stdcall*)(BoxRequest*))at(0x4ac0))(&request);
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a868))(&callback);
}

void __attribute__((used)) AdapterObserve(LONG nativeId,const wchar_t *target,int result,int error){
 EnterCriticalSection(&registryLock);
 // Retries use the same target. A late callback for an old native ID must
 // never bind to the newest request just because its path matches first.
 Task *t=nativeId?find_task(0,nativeId,NULL):NULL;
 if(!t&&target)for(Task*p=registry;p;p=p->next)
  if(!p->nativeId&&!p->cancelled&&!p->result&&!wcscmp(p->target,target)){t=p;break;}
 if(t&&!t->cancelled&&!t->result){t->nativeId=nativeId;t->queued=1;t->result=result;t->error=error;if(t->event)SetEvent(t->event);}
 LeaveCriticalSection(&registryLock);
}

__attribute__((naked,used)) void AdapterResourceGate(void){__asm__ __volatile__(
 "cmpl $3,4(%esp)\n\tjne 1f\n\tmov 8(%esp),%eax\n\tand $0xf0000000,%eax\n\tcmp $0x60000000,%eax\n\tjne 1f\n\tjmp @AdapterResource@136\n\t"
 "1: push $-1\n\tpush _resource_seh\n\tjmp *_resource_resume\n\t");}

__attribute__((naked,used)) void AdapterSuccessGate(void){__asm__ __volatile__(
 "pushfl\n\tpushal\n\tmov 40(%esp),%eax\n\tmov 44(%esp),%edx\n\tpush $0\n\tpush $1\n\tpush %edx\n\tpush %eax\n\tcall _AdapterObserve\n\tadd $16,%esp\n\tpopal\n\tpopfl\n\tpush %ebp\n\tmov %esp,%ebp\n\tand $-8,%esp\n\tjmp *_success_resume\n\t");}

__attribute__((naked,used)) void AdapterErrorGate(void){__asm__ __volatile__(
 "pushfl\n\tpushal\n\tmov 40(%esp),%eax\n\tmov 44(%esp),%edx\n\tpush %edx\n\tpush $-1\n\tpush $0\n\tpush %eax\n\tcall _AdapterObserve\n\tadd $16,%esp\n\tpopal\n\tpopfl\n\tpush %ebp\n\tmov %esp,%ebp\n\tand $-8,%esp\n\tjmp *_error_resume\n\t");}

#include "option.inc"

static LRESULT CALLBACK AdapterWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==UI_MESSAGE+1&&wp==UI_COOKIE&&lp==0){
  ((void(__attribute__((thiscall))*)(void*))at(0xb9280))(at(0x3c8978));
  ((void(__attribute__((thiscall))*)(void*))at(0xb9320))(at(0x3c8978));return 1;
 }
 if(msg==UI_MESSAGE&&wp==UI_COOKIE&&lp==(LPARAM)&ui){
  ui.ok=0;ui.error[0]=0;void *opt=option();if(!opt){strcpy(ui.error,"mcoption interface unavailable");return 0;}
  void **vt=*(void***)opt;
  if(ui.operation==1){
   BoxString s;box_ctor(&s);((void(__attribute__((thiscall))*)(void*,BoxString*))vt[0x5c/4])(opt,&s);
   const wchar_t *root=box_text(&s);size_t n=wcslen(root);
   if(n>2&&n<350&&((root[1]==':'&&root[2]=='\\')||!wcsncmp(root,L"\\\\",2))){wcscpy(ui.path,root);ui.ok=1;}
   else strcpy(ui.error,"Box download directory is empty, relative, or too long; set it in the box settings first");
   box_dtor(&s);
   if(ui.ok&&(!(*(DWORD*)at(0x3c8974)&1)||!*(BYTE*)at(0x3c8a18))){ui.ok=0;strcpy(ui.error,"Native download manager is not ready; open Download Center once, then retry");}
  }else if(ui.operation==3){
   ((void(__attribute__((thiscall))*)(void*))at(0xb9280))(at(0x3c8978));
   ((void(__attribute__((thiscall))*)(void*))at(0xb9320))(at(0x3c8978));ui.ok=1;
  }else if(ui.operation==4){
   ui.ok=(*(DWORD*)at(0x3c8974)&1)&&*(BYTE*)at(0x3c8a18);
   if(!ui.ok)strcpy(ui.error,"Native download manager is not ready");
  }else if(ui.operation==2){
   ui.ok=register_official(opt,ui.path,ui.error);
  }else if(ui.operation==5){
   ui.ok=unregister_official(opt,ui.path,ui.error);
  }
  return ui.ok;
 }
 return CallWindowProcW(oldWndProc,hwnd,msg,wp,lp);
}

static int ui_call(int operation,wchar_t *path,int capacity,char *err){
 EnterCriticalSection(&uiLock);
 if(uiTimedOut){strcpy(err,"Box UI request previously timed out; restart the box before retrying");LeaveCriticalSection(&uiLock);return 0;}
 if(!uiWindow||!IsWindow(uiWindow)){
  uiWindow=FindWindowW(NULL,L"MCPC_WND_NAME-{EB20D18E-2FFA-4235-A259-958B395B71AB}");
  if(!uiWindow){strcpy(err,"Box main window not found");LeaveCriticalSection(&uiLock);return 0;}
  DWORD process=0;GetWindowThreadProcessId(uiWindow,&process);if(process!=GetCurrentProcessId()){strcpy(err,"Window belongs to another box process; close the old instance first");LeaveCriticalSection(&uiLock);return 0;}
  SetLastError(0);oldWndProc=(WNDPROC)SetWindowLongPtrW(uiWindow,GWLP_WNDPROC,(LONG_PTR)AdapterWndProc);
  if(!oldWndProc){strcpy(err,"Cannot attach native UI dispatcher");uiWindow=NULL;LeaveCriticalSection(&uiLock);return 0;}
 }
 ui.operation=operation;ui.ok=0;ui.error[0]=0;if(operation==2||operation==5){if(wcslen(path)>=4096){strcpy(err,"Version path too long");LeaveCriticalSection(&uiLock);return 0;}wcscpy(ui.path,path);}
 DWORD_PTR result=0;int delivered=SendMessageTimeoutW(uiWindow,UI_MESSAGE,UI_COOKIE,(LPARAM)&ui,SMTO_ABORTIFHUNG|SMTO_BLOCK,10000,&result)!=0;
 if(!delivered)uiTimedOut=1; // Do not reuse shared request storage while a delayed UI call may still run.
 int ok=delivered&&ui.ok;
 if(ok&&operation==1){if((int)wcslen(ui.path)>=capacity){ok=0;strcpy(err,"Box download path too long");}else wcscpy(path,ui.path);}
 if(!ok&&!*err)snprintf(err,512,"%s",delivered&&*ui.error?ui.error:"Box UI request failed or timed out");
 LeaveCriticalSection(&uiLock);return ok;
}

int adapter_root(wchar_t *out,int capacity,char *err){return ui_call(1,out,capacity,err);}
int adapter_register(const wchar_t *path,char *err){wchar_t copy[4096];if(wcslen(path)>=4096)return 0;wcscpy(copy,path);return ui_call(2,copy,4096,err);}
int adapter_unregister(const wchar_t *path,char *err){wchar_t copy[4096];if(wcslen(path)>=4096)return 0;wcscpy(copy,path);return ui_call(5,copy,4096,err);}

int adapter_submit(const wchar_t *name,const wchar_t *url,const wchar_t *target,void **task,char *err){
 if(!uiWindow||!IsWindow(uiWindow)){strcpy(err,"Native box window is unavailable");return 0;}
 if(wcslen(target)>=512||wcslen(url)>=2048){strcpy(err,"Path or URL exceeds the original downloader's capacity");return 0;}
 Task *t=calloc(1,sizeof(Task));if(!t){strcpy(err,"Cannot allocate native task record");return 0;}t->id=InterlockedIncrement(&nextId);if((t->id&0xf0000000)!=0x60000000){free(t);strcpy(err,"Native adapter task ID range exhausted");return 0;}wcscpy(t->target,target);
 t->event=CreateEventW(NULL,TRUE,FALSE,NULL);if(!t->event){free(t);strcpy(err,"Cannot create native completion event");return 0;}
 EnterCriticalSection(&registryLock);t->groupId=currentGroup;t->next=registry;registry=t;LeaveCriticalSection(&registryLock);
 BoxRequest *r=((void*(__cdecl*)(size_t))iat(0x309d1c))(sizeof(BoxRequest));
 BoxCallback *cb=((void*(__cdecl*)(size_t))iat(0x309d1c))(sizeof(BoxCallback));
 if(!r||!cb){if(r)((void(__cdecl*)(void*))at(0x2c4e58))(r);if(cb)((void(__cdecl*)(void*))at(0x2c4e58))(cb);t->result=-1;t->queued=-1;adapter_release(t);strcpy(err,"Box task allocation failed");return 0;}
 memset(r,0,sizeof(*r));box_ctor(&r->name);box_ctor(&r->url);box_ctor(&r->target);box_ctor(&r->md5);
 r->type=3;r->id=(uint32_t)t->id;box_assign(&r->name,name);box_assign(&r->url,url);box_assign(&r->target,target);
 ((void(__attribute__((thiscall))*)(void*,void*))iat(0x30a844))(cb,NULL);
 if(!PostMessageW(uiWindow,0x3ee,(WPARAM)r,(LPARAM)cb)){
  ((void(__stdcall*)(BoxRequest*))at(0x4ac0))(r);((void(__attribute__((thiscall))*)(void*))iat(0x30a868))(cb);
  ((void(__cdecl*)(void*))at(0x2c4e58))(r);((void(__cdecl*)(void*))at(0x2c4e58))(cb);
  t->result=-1;t->queued=-1;adapter_release(t);strcpy(err,"Cannot submit WM_DOWNLOAD_RESOURCE to box");return 0;
 }
 *task=t;return 1;
}
int adapter_status(void *task,unsigned long long *bytes,int *error){Task *t=task;LONG nativeId,queued,result,cancelled;void *manager;EnterCriticalSection(&registryLock);nativeId=t->nativeId;queued=t->queued;result=t->result;cancelled=t->cancelled;manager=t->manager;*error=t->error;LeaveCriticalSection(&registryLock);*bytes=0;if(nativeId)*bytes=((unsigned long long(__cdecl*)(LONG))iat(0x30a60c))(nativeId);if(cancelled)return -2;if(result)return result>0?2:-1;
 if(manager&&nativeId&&queued>0){int present=0;((void(__attribute__((thiscall))*)(void*))iat(0x30a808))((char*)manager+4);uintptr_t begin=*(uintptr_t*)((char*)manager+0x28),end=*(uintptr_t*)((char*)manager+0x2c);if(end>=begin&&(end-begin)%0x88==0&&(end-begin)/0x88<200000)for(uintptr_t row=begin;row<end;row+=0x88)if(*(LONG*)(row+0x78)==nativeId){present=1;break;}((void(__attribute__((thiscall))*)(void*))iat(0x30a800))((char*)manager+4);
  if(!present){EnterCriticalSection(&registryLock);result=t->result;LeaveCriticalSection(&registryLock);if(result)return result>0?2:-1;*error=-1002;return -1;}}
 return queued>0?1:queued<0?-1:0;}
int adapter_task_pending(void *task){
 Task*t=task;EnterCriticalSection(&registryLock);LONG id=t->nativeId,queued=t->queued;void*manager=t->manager;LeaveCriticalSection(&registryLock);
 if(queued<0)return 0;if(!manager||!id)return 1;
 int present=0;((void(__attribute__((thiscall))*)(void*))iat(0x30a808))((char*)manager+4);
 uintptr_t begin=*(uintptr_t*)((char*)manager+0x28),end=*(uintptr_t*)((char*)manager+0x2c);
 if(end<begin||(end-begin)%0x88||(end-begin)/0x88>=200000)present=1;
 else for(uintptr_t row=begin;row<end;row+=0x88)if(*(LONG*)(row+0x78)==id){present=1;break;}
 ((void(__attribute__((thiscall))*)(void*))iat(0x30a800))((char*)manager+4);return present;
}
int adapter_group_sync(void){
 DWORD_PTR result=0;
 // Pointer-free message: a delayed delivery is safe after timeout, unlike ui.
 return uiWindow&&SendMessageTimeoutW(uiWindow,UI_MESSAGE+1,UI_COOKIE,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,2000,&result)!=0;
}
static int post_marker(LONG logicalId){
 if(!uiWindow)return 0;
 BoxRequest*r=((void*(__cdecl*)(size_t))iat(0x309d1c))(sizeof(BoxRequest));
 BoxCallback*cb=((void*(__cdecl*)(size_t))iat(0x309d1c))(sizeof(BoxCallback));
 if(!r||!cb){if(r)((void(__cdecl*)(void*))at(0x2c4e58))(r);if(cb)((void(__cdecl*)(void*))at(0x2c4e58))(cb);return 0;}
 memset(r,0,sizeof(*r));r->type=3;r->id=logicalId;box_ctor(&r->name);box_ctor(&r->url);box_ctor(&r->target);box_ctor(&r->md5);
 ((void(__attribute__((thiscall))*)(void*,void*))iat(0x30a844))(cb,NULL);
 if(!PostMessageW(uiWindow,0x3ee,(WPARAM)r,(LPARAM)cb)){
  ((void(__stdcall*)(BoxRequest*))at(0x4ac0))(r);((void(__attribute__((thiscall))*)(void*))iat(0x30a868))(cb);
  ((void(__cdecl*)(void*))at(0x2c4e58))(r);((void(__cdecl*)(void*))at(0x2c4e58))(cb);
  return 0;
 }
 return 1;
}
void adapter_cancel(void *task){
 Task*t=task;EnterCriticalSection(&registryLock);t->cancelled=1;if(t->event)SetEvent(t->event);LONG id=t->nativeId;int retired=t->retired;t->retired=1;LeaveCriticalSection(&registryLock);
 if(id)((int(__cdecl*)(LONG))iat(0x30a628))(id);
 if(!retired)post_marker(t->id);
}
void adapter_wait(void *task,unsigned long timeout){Task*t=task;EnterCriticalSection(&registryLock);HANDLE event=t->event;LeaveCriticalSection(&registryLock);if(event)WaitForSingleObject(event,timeout);else Sleep(timeout);}
void adapter_release(void *task){Task*t=task;EnterCriticalSection(&registryLock);HANDLE event=t->event;t->event=NULL;LeaveCriticalSection(&registryLock);if(event)CloseHandle(event);}
void adapter_init(void){imageBase=(uintptr_t)GetModuleHandleW(NULL);InitializeCriticalSection(&registryLock);InitializeCriticalSection(&uiLock);native_create_fn=at(0xb71a0);resource_resume=at(0xb7587);resource_seh=at(0x2ff9bf);success_resume=at(0xb7c86);error_resume=at(0xb7d56);group_init();}

#include "group.inc"
#include "history.inc"
