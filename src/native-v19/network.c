#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <wininet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef HINTERNET (WINAPI *ConnectFn)(HINTERNET,LPCWSTR,INTERNET_PORT,LPCWSTR,LPCWSTR,DWORD,DWORD,DWORD_PTR);
typedef HINTERNET (WINAPI *RequestFn)(HINTERNET,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR*,DWORD,DWORD_PTR);
typedef BOOL (WINAPI *CloseFn)(HINTERNET);
static ConnectFn originalConnect;
static RequestFn originalRequest;
static CloseFn originalClose;
typedef struct {
 wchar_t agent[256],proxy[512],bypass[1024];DWORD access,timeout[3];HINTERNET root;
} Session;
typedef struct Conn {
 struct Conn *next;HINTERNET handle,root;wchar_t host[128];
 INTERNET_PORT port;DWORD flags;DWORD_PTR context;int cached,inUse,requests,poisoned;unsigned generation;
} Conn;
typedef struct Request {struct Request *next;HINTERNET handle,connection;} Request;
static CRITICAL_SECTION lock;
static Session sessions[8];static unsigned sessionCount;
static Conn *connections;static Request *requests;static unsigned cachedConnections,connectionReused,nextGeneration;
static volatile LONG installed;
static DWORD oldHttp,oldHttp10,newHttp,newHttp10;
static unsigned reused,created,official,keepAlive,fallback;

static int official_host(LPCWSTR host){
 const wchar_t *names[]={L"piston-data.mojang.com",L"piston-meta.mojang.com",L"libraries.minecraft.net",
 L"resources.download.minecraft.net",L"launcher.mojang.com",L"launchermeta.mojang.com",NULL};
 if(!host)return 0;for(int i=0;names[i];i++)if(!_wcsicmp(host,names[i]))return 1;return 0;
}
static int copy_optional(wchar_t *dest,size_t cap,LPCWSTR src){
 if(!src){*dest=0;return 1;}size_t n=wcslen(src);if(n>=cap)return 0;memcpy(dest,src,(n+1)*sizeof(wchar_t));return 1;
}
static HINTERNET shared_root(HINTERNET original){
 Session key;memset(&key,0,sizeof(key));
 DWORD n=sizeof(key.agent);if(!InternetQueryOptionW(original,INTERNET_OPTION_USER_AGENT,key.agent,&n))return NULL;
 key.agent[255]=0;
 union {void *align;BYTE bytes[8192];} info;n=sizeof(info.bytes);
 if(!InternetQueryOptionW(original,INTERNET_OPTION_PROXY,info.bytes,&n))return NULL;
 struct ProxyW {DWORD dwAccessType;LPCWSTR lpszProxy,lpszProxyBypass;};
 struct ProxyW *proxy=(struct ProxyW*)info.bytes;key.access=proxy->dwAccessType;
 if(!copy_optional(key.proxy,512,proxy->lpszProxy)||!copy_optional(key.bypass,1024,proxy->lpszProxyBypass))return NULL;
 const DWORD opts[]={INTERNET_OPTION_CONNECT_TIMEOUT,INTERNET_OPTION_SEND_TIMEOUT,INTERNET_OPTION_RECEIVE_TIMEOUT};
 for(int i=0;i<3;i++){n=4;if(!InternetQueryOptionW(original,opts[i],&key.timeout[i],&n))return NULL;}
 EnterCriticalSection(&lock);HINTERNET result=NULL;
 for(unsigned i=0;i<sessionCount;i++){
  Session *s=&sessions[i];
  if(s->access==key.access&&!wcscmp(s->agent,key.agent)&&!wcscmp(s->proxy,key.proxy)&&!wcscmp(s->bypass,key.bypass)&&!memcmp(s->timeout,key.timeout,sizeof(key.timeout))){result=s->root;reused++;break;}
 }
 if(!result&&sessionCount<8){
  result=InternetOpenW(key.agent,key.access,*key.proxy?key.proxy:NULL,*key.bypass?key.bypass:NULL,0);
  if(result){
   for(int i=0;i<3;i++)InternetSetOptionW(result,opts[i],&key.timeout[i],4);
   key.root=result;sessions[sessionCount++]=key;created++;
  }
 }
 LeaveCriticalSection(&lock);return result;
}
HINTERNET WINAPI NativeInternetConnect(HINTERNET root,LPCWSTR host,INTERNET_PORT port,LPCWSTR user,LPCWSTR password,DWORD service,DWORD flags,DWORD_PTR context){
 // Pool only public official HTTP endpoints; credentials and non-HTTP callers
 // keep the original root/session behavior.
 if(!installed)return originalConnect(root,host,port,user,password,service,flags,context);
 int eligible=service==INTERNET_SERVICE_HTTP&&flags==0&&context==0&&official_host(host)&&(!user||!*user)&&(!password||!*password);
 HINTERNET pooled=eligible?shared_root(root):NULL;
 if(eligible){EnterCriticalSection(&lock);official++;if(!pooled)fallback++;LeaveCriticalSection(&lock);}
 if(!pooled)return originalConnect(root,host,port,user,password,service,flags,context);
 EnterCriticalSection(&lock);
 int cacheable=flags==0&&context==0;
 if(cacheable)for(Conn *p=connections;p;p=p->next)
  if(p->cached&&!p->inUse&&!p->poisoned&&p->root==pooled&&p->port==port&&!_wcsicmp(p->host,host)){
   p->inUse=1;connectionReused++;HINTERNET result=p->handle;LeaveCriticalSection(&lock);return result;
  }
 // InternetConnect does not open an HTTP socket itself. Holding the lock here
 // prevents two tasks from claiming the same idle connection handle.
 HINTERNET connection=originalConnect(pooled,host,port,user,password,service,flags,context);
 if(connection){
  Conn *node=calloc(1,sizeof(*node));
  if(node){node->handle=connection;node->generation=++nextGeneration;node->root=pooled;node->port=port;node->flags=flags;node->context=context;
   node->cached=cacheable&&cachedConnections<64;node->inUse=1;copy_optional(node->host,128,host);
   if(node->cached)cachedConnections++;node->next=connections;connections=node;
  }
 }
 LeaveCriticalSection(&lock);
 return connection;
}
HINTERNET WINAPI NativeHttpOpenRequest(HINTERNET connection,LPCWSTR verb,LPCWSTR object,LPCWSTR version,LPCWSTR referrer,LPCWSTR *accept,DWORD flags,DWORD_PTR context){
 if(!installed)return originalRequest(connection,verb,object,version,referrer,accept,flags,context);
 int match=0,tracked=0;unsigned generation=0;EnterCriticalSection(&lock);
 for(Conn *p=connections;p;p=p->next)if(p->handle==connection){match=1;if(p->cached){p->requests++;tracked=1;generation=p->generation;}break;}
 if(match&&(!verb||!_wcsicmp(verb,L"GET"))){flags|=INTERNET_FLAG_KEEP_CONNECTION;keepAlive++;}
 LeaveCriticalSection(&lock);
 HINTERNET request=originalRequest(connection,verb,object,version,referrer,accept,flags,context);
 if(tracked){EnterCriticalSection(&lock);
  for(Conn *p=connections;p;p=p->next)if(p->handle==connection&&p->generation==generation){
   if(request){Request *r=malloc(sizeof(*r));if(r){r->handle=request;r->connection=connection;r->next=requests;requests=r;}else p->poisoned=1;}else if(p->requests>0)p->requests--;
   break;
  }
  LeaveCriticalSection(&lock);
 }
 return request;
}
BOOL WINAPI NativeInternetClose(HINTERNET handle){
 if(!installed)return originalClose(handle);
 EnterCriticalSection(&lock);
 Request **r=&requests;
 while(*r){if((*r)->handle==handle){Request *old=*r;for(Conn *c=connections;c;c=c->next)if(c->handle==old->connection){if(c->requests>0)c->requests--;break;}*r=old->next;free(old);break;}r=&(*r)->next;}
 Conn **p=&connections;
 while(*p){if((*p)->handle==handle){
  Conn *old=*p;
  if(old->cached&&!old->requests&&!old->poisoned){old->inUse=0;LeaveCriticalSection(&lock);return TRUE;}
  // Closing a parent while a request is still active must retain the original
  // cancellation semantics, rather than returning it to the idle pool.
  *p=old->next;if(old->cached)cachedConnections--;
  r=&requests;while(*r){if((*r)->connection==handle){Request *drop=*r;*r=drop->next;free(drop);}else r=&(*r)->next;}
  free(old);break;
 }p=&(*p)->next;}
 LeaveCriticalSection(&lock);return originalClose(handle);
}
static void **import_slot(HMODULE module,const char *name){
 BYTE *base=(BYTE*)module;IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)base;
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return NULL;
 IMAGE_NT_HEADERS32 *nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC)return NULL;
 IMAGE_IMPORT_DESCRIPTOR *d=(IMAGE_IMPORT_DESCRIPTOR*)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
 for(;d->Name;d++)if(!_stricmp((char*)base+d->Name,"WININET.dll")&&d->OriginalFirstThunk){
  IMAGE_THUNK_DATA32 *names=(IMAGE_THUNK_DATA32*)(base+d->OriginalFirstThunk),*iat=(IMAGE_THUNK_DATA32*)(base+d->FirstThunk);
  for(;names->u1.AddressOfData;names++,iat++)if(!IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)){
   IMAGE_IMPORT_BY_NAME *entry=(IMAGE_IMPORT_BY_NAME*)(base+names->u1.AddressOfData);
   if(!strcmp((char*)entry->Name,name))return (void**)&iat->u1.Function;
  }
 }
 return NULL;
}
int native_network_init(void){
 InitializeCriticalSection(&lock);HMODULE module=GetModuleHandleW(L"taskmodulehttp.dll");if(!module)return 0;
 void **connect=import_slot(module,"InternetConnectW"),**request=import_slot(module,"HttpOpenRequestW"),**close=import_slot(module,"InternetCloseHandle");
 if(!connect||!request||!close)return 0;
 originalConnect=(ConnectFn)*connect;originalRequest=(RequestFn)*request;originalClose=(CloseFn)*close;
 // Capture the originals before any hook is visible. Partial installation is
 // safe: every wrapper works independently and always calls its original API.
 void **slots[]={connect,request,close};DWORD protections[3];int writable=0;
 for(;writable<3;writable++)if(!VirtualProtect(slots[writable],4,PAGE_READWRITE,&protections[writable]))break;
 if(writable!=3){for(int i=writable-1;i>=0;i--){DWORD ignored;VirtualProtect(slots[i],4,protections[i],&ignored);}return 0;}
 *connect=NativeInternetConnect;*request=NativeHttpOpenRequest;*close=NativeInternetClose;InterlockedExchange(&installed,1);
 for(int i=2;i>=0;i--){DWORD ignored;VirtualProtect(slots[i],4,protections[i],&ignored);}
 DWORD size=4;InternetQueryOptionW(NULL,INTERNET_OPTION_MAX_CONNS_PER_SERVER,&oldHttp,&size);size=4;
 InternetQueryOptionW(NULL,INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER,&oldHttp10,&size);
 DWORD limit=32;if(oldHttp<limit)InternetSetOptionW(NULL,INTERNET_OPTION_MAX_CONNS_PER_SERVER,&limit,4);
 if(oldHttp10<limit)InternetSetOptionW(NULL,INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER,&limit,4);
 size=4;InternetQueryOptionW(NULL,INTERNET_OPTION_MAX_CONNS_PER_SERVER,&newHttp,&size);size=4;
 InternetQueryOptionW(NULL,INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER,&newHttp10,&size);
 return installed;
}
void native_network_report(char *out,unsigned size){
 EnterCriticalSection(&lock);
 snprintf(out,size,"Native WinINet pool: hooks=%d; sessions=%u; reused=%u; officialConnects=%u; keepAliveRequests=%u; cachedConnections=%u; connectionReuse=%u; fallback=%u; httpLimit=%lu->%lu; http10Limit=%lu->%lu",installed,created,reused,official,keepAlive,cachedConnections,connectionReused,fallback,(unsigned long)oldHttp,(unsigned long)newHttp,(unsigned long)oldHttp10,(unsigned long)newHttp10);
 LeaveCriticalSection(&lock);
}

