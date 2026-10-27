#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include "../core.h"
#include "../adapter.h"
#define MAXPATH 4096
#define WINAPI
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define FILE_ATTRIBUTE_DIRECTORY 16
#define FILE_ATTRIBUTE_REPARSE_POINT 1024
#define INVALID_FILE_ATTRIBUTES ((unsigned long)-1)
typedef unsigned long DWORD;typedef long LONG;typedef void* HANDLE;typedef pthread_mutex_t CRITICAL_SECTION;
static char directory[1024];static DWORD winError;static HistoryRow live[8];static int liveCount,failWrite,writeCount,changedDuringWrite,restores;
static void EnterCriticalSection(CRITICAL_SECTION*l){assert(!pthread_mutex_lock(l));}static void LeaveCriticalSection(CRITICAL_SECTION*l){assert(!pthread_mutex_unlock(l));}
static LONG InterlockedIncrement(volatile LONG*p){return __sync_add_and_fetch(p,1);}static LONG InterlockedExchange(volatile LONG*p,LONG v){return __sync_lock_test_and_set(p,v);}static LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG expected){return __sync_val_compare_and_swap(p,expected,v);}
static int SetEvent(HANDLE h){(void)h;return 1;}static void Sleep(DWORD n){(void)n;}static DWORD WaitForSingleObject(HANDLE h,DWORD n){(void)h;(void)n;return 0;}
static DWORD GetLastError(void){return winError;}
static DWORD GetEnvironmentVariableW(const wchar_t*n,wchar_t*out,DWORD cap){assert(!wcscmp(n,L"APPDATA")&&cap>30);wcscpy(out,L"D:\\用户\\AppData\\Roaming");return wcslen(out);}
static void local_path(char out[1200]){snprintf(out,1200,"%s/history.json",directory);}
static DWORD GetFileAttributesW(const wchar_t*p){assert(wcsstr(p,L"completed-history-v1.json"));char path[1200];local_path(path);struct stat s;if(stat(path,&s)){winError=2;return INVALID_FILE_ATTRIBUTES;}return S_ISDIR(s.st_mode)?16:0;}
int adapter_history_available(void){return 1;}int adapter_history_attach(char*e){(void)e;return 1;}
static int file_safe(const wchar_t*p){(void)p;return 1;}
static void errwin(char*e,const char*s){snprintf(e,512,"%s (%lu)",s,winError);}static void log_event(const char*s){(void)s;}
static char*utf8(const wchar_t*s){size_t n=wcstombs(NULL,s,0);if(n==(size_t)-1)return NULL;char*p=malloc(n+1);assert(p);wcstombs(p,s,n+1);return p;}
static int from_utf8(const char*s,wchar_t*out,int cap){size_t n=mbstowcs(out,s,cap);return n!=(size_t)-1&&n<(size_t)cap;}
static int read_file(const wchar_t*p,char**out,size_t*n,size_t limit){(void)p;char path[1200];local_path(path);FILE*f=fopen(path,"rb");if(!f)return 0;fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);if(len<0||(size_t)len>limit){fclose(f);return 0;}*out=malloc(len+1);assert(*out);assert(fread(*out,1,len,f)==(size_t)len);(*out)[len]=0;*n=len;fclose(f);return 1;}
int adapter_group_export(HistoryRow**rows,int*count,char*e){(void)e;*rows=calloc(liveCount+1,sizeof(**rows));assert(*rows);memcpy(*rows,live,liveCount*sizeof(*live));*count=liveCount;return 1;}
int adapter_group_restore(const HistoryRow*rows,int count,char*e){(void)e;assert(count<=8);memcpy(live,rows,count*sizeof(*live));liveCount=count;restores++;return 1;}
static void NativeHistoryChanged(void);
static int write_atomic(const wchar_t*p,const void*data,size_t n,char*err){(void)p;writeCount++;if(failWrite){strcpy(err,"simulated write/commit failure");return 0;}char path[1200],temp[1300];local_path(path);snprintf(temp,sizeof(temp),"%s.tmp",path);FILE*f=fopen(temp,"wb");assert(f);assert(fwrite(data,1,n,f)==n);assert(!fflush(f));assert(!fsync(fileno(f)));assert(!fclose(f));assert(!rename(temp,path));if(changedDuringWrite){changedDuringWrite=0;live[liveCount]=live[0];live[liveCount].id+=3;liveCount++;NativeHistoryChanged();}return 1;}
/* Production entry point is external; its Win32 transport is substituted above. */
#define NativeHistoryChanged NativeHistoryChangedImpl
#include "../history_store.inc"
#undef NativeHistoryChanged
static void NativeHistoryChanged(void){NativeHistoryChangedImpl();}
static void record(int index,uint32_t id,const wchar_t*name){HistoryRow*r=live+index;memset(r,0,sizeof(*r));r->id=id;r->type=3;r->isGroup=1;r->bytes=8589934592ULL;wcscpy(r->name,name);wcscpy(r->path,L"D:\\MCBox\\download\\resources\\official\\versions\\1.8.9");}
static void codec_cases(void){char err[512]={0};HistoryRow row={0},*rows=NULL;int count=0;record(0,0x62000001,L"Minecraft 中文\"测试");row=live[0];char*data=history_encode(&row,1,err);assert(data&&history_decode(data,&rows,&count,err)&&count==1&&rows[0].bytes==row.bytes&&!wcscmp(rows[0].name,row.name));free(rows);free(data);
 const char*bad[]={"{}","{\"format\":2,\"groups\":[]}","{\"format\":1,\"groups\":[{\"id\":1,\"bytes\":10,\"name\":\"x\",\"path\":\"D:/cache\"}]}","{\"format\":1,\"groups\":[{\"id\":1644167169,\"bytes\":9223372036854775808,\"name\":\"x\",\"path\":\"D:/cache\"}]}","{\"format\":1,\"groups\":[]} {}",NULL};
 for(int i=0;bad[i];i++){char*copy=strdup(bad[i]);assert(!history_decode(copy,&rows,&count,err)&&!rows);free(copy);}
 assert(!history_stored_path(L"relative\\cache")&&!history_stored_path(L"\\\\?\\GLOBALROOT\\x"));
 liveCount=0;
}
static void child(const char*phase){char err[512]={0};assert(!pthread_mutex_init(&historyStoreLock,NULL));(void)history_store_worker;
 if(!strcmp(phase,"corrupt")){assert(!NativeHistoryEnsure(err)&&!historyLoaded);NativeHistoryChanged();assert(!NativeHistoryFlush(err)&&writeCount==0);return;}
 assert(NativeHistoryEnsure(err));assert(NativeHistoryEnsure(err));assert(restores<=1);
 if(!strcmp(phase,"complete")){assert(liveCount==0);record(0,0x62000001,L"Minecraft 中文");record(1,0x62000002,L"Minecraft 26.3");liveCount=2;NativeHistoryChanged();assert(NativeHistoryFlush(err)&&writeCount==1);}
 else if(!strcmp(phase,"restore")){assert(liveCount==2&&live[0].id==0x62000001&&live[0].bytes==8589934592ULL&&!wcscmp(live[0].name,L"Minecraft 中文"));assert(NativeHistoryFlush(err)&&writeCount==0);}
 else if(!strcmp(phase,"delete_one")){assert(liveCount==2);live[0]=live[1];liveCount=1;NativeHistoryChanged();assert(NativeHistoryFlush(err));}
 else if(!strcmp(phase,"one")){assert(liveCount==1&&live[0].id==0x62000002);}
 else if(!strcmp(phase,"delete_all")){liveCount=0;NativeHistoryChanged();assert(NativeHistoryFlush(err));}
 else if(!strcmp(phase,"empty")){assert(liveCount==0);}
 else if(!strcmp(phase,"failure")){assert(liveCount==2);liveCount=1;NativeHistoryChanged();failWrite=1;assert(!NativeHistoryFlush(err)&&historySavedEpoch!=historyEpoch);failWrite=0;assert(NativeHistoryFlush(err)&&historySavedEpoch==historyEpoch);}
 else if(!strcmp(phase,"concurrent")){assert(liveCount==2);changedDuringWrite=1;NativeHistoryChanged();assert(NativeHistoryFlush(err)&&historySavedEpoch!=historyEpoch);assert(NativeHistoryFlush(err)&&historySavedEpoch==historyEpoch);assert(writeCount==2&&liveCount==3);}
 else if(!strcmp(phase,"three")){assert(liveCount==3);}
 else assert(0);
}
static void run(const char*self,const char*phase){pid_t p=fork();assert(p>=0);if(!p){execl(self,self,phase,directory,(char*)NULL);_exit(127);}int status;assert(waitpid(p,&status,0)==p&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}
int main(int argc,char**argv){assert(setlocale(LC_ALL,"C.UTF-8"));if(argc==3){strcpy(directory,argv[2]);child(argv[1]);return 0;}codec_cases();strcpy(directory,"/tmp/mcbox-history-XXXXXX");assert(mkdtemp(directory));run(argv[0],"complete");run(argv[0],"restore");run(argv[0],"delete_one");run(argv[0],"one");run(argv[0],"delete_all");run(argv[0],"empty");run(argv[0],"complete");run(argv[0],"failure");run(argv[0],"delete_all");run(argv[0],"complete");run(argv[0],"concurrent");run(argv[0],"three");
 char path[1200];local_path(path);FILE*f=fopen(path,"wb");assert(f);fputs("{broken history",f);fclose(f);run(argv[0],"corrupt");f=fopen(path,"rb");assert(f);char buf[64]={0};fread(buf,1,63,f);fclose(f);assert(!strcmp(buf,"{broken history"));assert(!unlink(path)&&!rmdir(directory));puts("PASS production history codec/store across separate processes: completed restore, Unicode/64-bit sizes/stable IDs, repeat load deduplication, single/all removal persists, no cache rescan, failed save retains dirty state, concurrent mutation reflush, corrupt file preserved. Real host files; Win32 and adapter calls substituted.");}
