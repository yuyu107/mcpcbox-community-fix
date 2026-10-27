#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <openssl/sha.h>
#include "../cache_keys.h"
#define MAXPATH 4096
#define FALSE 0
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 8
typedef struct {char *hash,*path;int64_t size;} Item;
static char directory[256],key[41];static const char payload[]="shared official bytes";
static int stoppedFlag,nativeCalls,copyCalls,moveFail,copyFail,corruptCopy,corruptNative,nativeFail,stopAfterNative,unsafePath;
static unsigned long hostError;static int lockReads,lockCopies,lockMoves,sleeps,cancelOnSleep,moveError=112;
static unsigned long GetLastError(void){return hostError;}
static void SetLastError(unsigned long e){hostError=e;}
static void Sleep(unsigned ms){assert(ms==50);sleeps++;if(cancelOnSleep)stoppedFlag=1;}
static int stopped(void){return stoppedFlag;}
static void host_path(const wchar_t *path,char out[1024]){char name[512];assert(wcstombs(name,path,sizeof(name))<(size_t)-1);snprintf(out,1024,"%s/%s",directory,name);}
static void write_data(const wchar_t *path,const char*data,size_t n){char p[1024];host_path(path,p);FILE*f=fopen(p,"wb");assert(f);assert(fwrite(data,1,n,f)==n);assert(!fclose(f));}
static void erase(const wchar_t*path){char p[1024];host_path(path,p);unlink(p);}
static void digest(const void*data,size_t n,char out[41]){unsigned char hash[20];assert(SHA1(data,n,hash));for(int i=0;i<20;i++)sprintf(out+2*i,"%02x",hash[i]);out[40]=0;}
static int verified_file_once(const wchar_t*path,const char*expected,int64_t size){hostError=0;if(lockReads){lockReads--;hostError=32;return 0;}char p[1024],data[4096],hash[41];host_path(path,p);FILE*f=fopen(p,"rb");if(!f)return 0;size_t n=fread(data,1,sizeof(data),f);int ok=!ferror(f)&&feof(f)&&(int64_t)n==size;fclose(f);digest(data,n,hash);return ok&&!strcmp(hash,expected);}
static int file_safe(const wchar_t*p){(void)p;return !unsafePath;}
static int ensure_dirs(wchar_t*p,char*err){(void)p;(void)err;return 1;}
static void errwin(char*err,const char*text){snprintf(err,512,"%s (%d)",text,errno);}
static int CopyFileW(const wchar_t*a,const wchar_t*b,int failExists){(void)failExists;copyCalls++;hostError=0;if(lockCopies){lockCopies--;hostError=33;return 0;}if(copyFail){hostError=112;return 0;}char pa[1024],pb[1024],data[4096];host_path(a,pa);host_path(b,pb);FILE*f=fopen(pa,"rb"),*g=fopen(pb,"wb");if(!f||!g){if(f)fclose(f);if(g)fclose(g);return 0;}size_t n;while((n=fread(data,1,sizeof(data),f)))assert(fwrite(data,1,n,g)==n);fclose(f);fclose(g);if(corruptCopy)write_data(b,"damaged",7);return 1;}
static int MoveFileExW(const wchar_t*a,const wchar_t*b,int flags){assert(flags==(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH));hostError=0;if(lockMoves){lockMoves--;hostError=32;return 0;}if(moveFail){hostError=moveError;return 0;}char pa[1024],pb[1024];host_path(a,pa);host_path(b,pb);return !rename(pa,pb);}
static int DeleteFileW(const wchar_t*p){erase(p);return 1;}
#include "../file_retry.h"
#include "../cache_copy.h"
static int download_file_native(Item*item,const wchar_t*part,char*err){(void)item;nativeCalls++;assert(!wcscmp(part,L"object.part"));if(nativeFail){strcpy(err,"native failure");return 0;}write_data(part,corruptNative?"bad":payload,corruptNative?3:strlen(payload));if(stopAfterNative)stoppedFlag=1;return 1;}
#include "../cache_pipeline.h"
static Item item;
static int acquire(const wchar_t*installed,int*source){char err[512]={0};return cache_acquire(&item,installed,L"object",L"object.part",source,err);}
static void reset(void){const wchar_t*paths[]={L"a",L"b",L"object",L"object.part",L"a.mcbox.tmp",L"b.mcbox.tmp",L"object.mcbox.tmp"};for(unsigned i=0;i<sizeof(paths)/sizeof(*paths);i++)erase(paths[i]);stoppedFlag=nativeCalls=copyCalls=moveFail=copyFail=corruptCopy=corruptNative=nativeFail=stopAfterNative=unsafePath=0;}
static pthread_mutex_t stripes[64];
static void*worker(void*arg){int source=0;pthread_mutex_lock(stripes+cache_stripe(key));assert(acquire((const wchar_t*)arg,&source));pthread_mutex_unlock(stripes+cache_stripe(key));return NULL;}
int main(void){strcpy(directory,"/tmp/mcbox-cache-XXXXXX");assert(mkdtemp(directory));digest(payload,strlen(payload),key);item=(Item){key,"libraries/shared.jar",(int64_t)strlen(payload)};int source;
 char canonical[41];assert(cache_key(key,canonical));char upper[41];strcpy(upper,key);for(int i=0;i<40;i++)if(upper[i]>='a'&&upper[i]<='f')upper[i]-=32;assert(cache_key(upper,canonical)&&!strcmp(canonical,key));assert(!cache_key("../bad",canonical)&&!cache_key("gggggggggggggggggggggggggggggggggggggggg",canonical));assert(cache_stripe(key)<64);
 assert(cache_descendant(L"D:\\MCBox\\download\\resources",L"\\\\?\\d:/MCBox/download/resources/file.jar"));assert(!cache_descendant(L"D:\\MCBox\\download\\resources",L"D:\\MCBox\\download\\resources-other\\x.jar"));assert(!cache_descendant(L"D:\\MCBox\\download\\resources",L"D:\\MCBox\\download\\game\\v\\x.jar"));
 reset();assert(acquire(L"a",&source)&&source==CACHE_NATIVE_TRANSFER&&nativeCalls==1);assert(verified_file(L"a",key,item.size)&&verified_file(L"object",key,item.size));assert(acquire(L"b",&source)&&source==CACHE_VERIFIED_OBJECT&&nativeCalls==1);erase(L"object");assert(verified_file(L"a",key,item.size)&&verified_file(L"b",key,item.size));
 reset();write_data(L"a",payload,strlen(payload));assert(acquire(L"a",&source)&&source==CACHE_EXISTING_INSTALL&&nativeCalls==0&&verified_file(L"object",key,item.size));
 reset();write_data(L"object",payload,strlen(payload));write_data(L"a","tampered game",13);assert(acquire(L"a",&source)&&source==CACHE_VERIFIED_OBJECT&&nativeCalls==0);
 reset();write_data(L"object","broken",6);assert(acquire(L"a",&source)&&source==CACHE_NATIVE_TRANSFER&&nativeCalls==1);
 reset();write_data(L"object.part",payload,strlen(payload));assert(acquire(L"a",&source)&&source==CACHE_VERIFIED_PART&&nativeCalls==0);
 reset();write_data(L"object.part","partial",7);assert(acquire(L"a",&source)&&nativeCalls==1);
 reset();corruptNative=1;assert(!acquire(L"a",&source)&&!verified_file(L"object",key,item.size)&&!verified_file(L"a",key,item.size));
 reset();copyFail=1;assert(!acquire(L"a",&source)&&verified_file(L"object",key,item.size));copyFail=0;assert(acquire(L"a",&source)&&source==CACHE_VERIFIED_OBJECT&&nativeCalls==1);
 reset();corruptCopy=1;assert(!acquire(L"a",&source)&&verified_file(L"object",key,item.size)&&!verified_file(L"a",key,item.size));
 reset();moveFail=1;assert(!acquire(L"a",&source)&&verified_file(L"object.part",key,item.size));moveFail=0;assert(acquire(L"a",&source)&&source==CACHE_VERIFIED_PART&&nativeCalls==1);
 reset();nativeFail=1;assert(!acquire(L"a",&source)&&nativeCalls==1);
 reset();stoppedFlag=1;assert(!acquire(L"a",&source)&&nativeCalls==0);
 reset();stopAfterNative=1;assert(!acquire(L"a",&source)&&verified_file(L"object.part",key,item.size)&&!verified_file(L"object",key,item.size));
 reset();write_data(L"object",payload,strlen(payload));unsafePath=1;assert(!acquire(L"a",&source)&&nativeCalls==0);
 reset();lockReads=2;sleeps=0;assert(acquire(L"a",&source)&&nativeCalls==1&&sleeps==2);
 reset();lockMoves=3;sleeps=0;assert(acquire(L"a",&source)&&nativeCalls==1&&sleeps==3);
 reset();lockCopies=2;sleeps=0;assert(acquire(L"a",&source)&&nativeCalls==1&&sleeps==2);
 reset();moveFail=1;moveError=32;sleeps=0;assert(!acquire(L"a",&source)&&sleeps==60&&verified_file(L"object.part",key,item.size));moveError=112;
 reset();moveFail=1;sleeps=0;assert(!acquire(L"a",&source)&&sleeps==0&&verified_file(L"object.part",key,item.size));
 reset();lockMoves=2;cancelOnSleep=1;sleeps=0;assert(!acquire(L"a",&source)&&sleeps==1&&nativeCalls==1);cancelOnSleep=0;lockMoves=0;
 reset();for(int i=0;i<64;i++)assert(!pthread_mutex_init(stripes+i,NULL));pthread_t a,b;assert(!pthread_create(&a,NULL,worker,(void*)L"a"));assert(!pthread_create(&b,NULL,worker,(void*)L"b"));pthread_join(a,NULL);pthread_join(b,NULL);assert(nativeCalls==1&&verified_file(L"a",key,item.size)&&verified_file(L"b",key,item.size));for(int i=0;i<64;i++)pthread_mutex_destroy(stripes+i);
 reset();assert(!rmdir(directory));puts("PASS production cache policy and atomic copy with real host files/SHA-1: shared content across installs, existing-install seeding, corrupt cache/game repair, verified partial recovery, interrupted/corrupt transfer, copy/commit/cancel failures, independent installed copies and same-digest serialized reuse; transient lock recovery without re-download, bounded retry, permanent error and cancellation. Windows native engine and filesystem calls are host substitutes.");
}
