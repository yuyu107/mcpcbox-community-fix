#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "../core.h"
#define MAXPATH 4096
#define INVALID_FILE_ATTRIBUTES ((unsigned long)-1)
#define INVALID_HANDLE_VALUE -1
#define ERROR_FILE_NOT_FOUND ENOENT
#define ERROR_PATH_NOT_FOUND ENOTDIR
#define ERROR_ALREADY_EXISTS EEXIST
#define ERROR_FILE_EXISTS EEXIST
#define GENERIC_WRITE 1
#define CREATE_NEW 1
#define FILE_ATTRIBUTE_NORMAL 0
#define MOVEFILE_WRITE_THROUGH 8
#define NULL_HANDLE 0
typedef unsigned long DWORD;typedef int HANDLE;
static char directory[256];static int failWrite,failMove,unsafe,race;
static void host(const wchar_t*p,char out[MAXPATH]){assert(wcstombs(out,p,MAXPATH)<MAXPATH);}
static int game_path(const char*r,wchar_t*out,char*err){(void)r;if(unsafe){strcpy(err,"unsafe path");return 0;}char p[MAXPATH];snprintf(p,sizeof(p),"%s/options.txt",directory);assert(mbstowcs(out,p,MAXPATH)<MAXPATH);return 1;}
static int ensure_dirs(wchar_t*p,char*err){(void)p;(void)err;return 1;}
static DWORD GetLastError(void){return errno;}static void SetLastError(DWORD e){errno=e;}
static void errwin(char*err,const char*s){snprintf(err,512,"%s (%d)",s,errno);}
static DWORD GetFileAttributesW(const wchar_t*p){char q[MAXPATH];host(p,q);struct stat s;return stat(q,&s)?INVALID_FILE_ATTRIBUTES:0;}
static DWORD GetCurrentThreadId(void){return 1;}
static HANDLE CreateFileW(const wchar_t*p,int access,int share,void*sec,int mode,int attr,void*t){(void)sec;(void)t;assert(access==GENERIC_WRITE&&share==0&&mode==CREATE_NEW&&attr==FILE_ATTRIBUTE_NORMAL);char q[MAXPATH];host(p,q);return open(q,O_WRONLY|O_CREAT|O_EXCL,0600);}
static int WriteFile(HANDLE f,const void*b,DWORD n,DWORD*w,void*p){(void)p;if(failWrite){errno=EIO;return 0;}ssize_t got=write(f,b,n);*w=got<0?0:got;return got>=0;}
static int FlushFileBuffers(HANDLE f){return !fsync(f);}static void CloseHandle(HANDLE f){assert(!close(f));}
static int DeleteFileW(const wchar_t*p){char q[MAXPATH];host(p,q);return !unlink(q);}
static int MoveFileExW(const wchar_t*a,const wchar_t*b,int flags){assert(flags==MOVEFILE_WRITE_THROUGH);char x[MAXPATH],y[MAXPATH];host(a,x);host(b,y);if(failMove){errno=EACCES;return 0;}if(race){int f=open(y,O_WRONLY|O_CREAT|O_EXCL,0600);assert(f>=0);assert(write(f,"lang:ja_jp\n",11)==11);close(f);}/* link+unlink emulates atomic no-replace publication on this host. */if(link(x,y))return 0;return !unlink(x);}
#include "../first_language.h"
static void check(Plan*p,const char*expected){const char*r=first_language(p);if(expected)assert(r&&!strcmp(r,expected));else assert(!r);}
static char *fixture(const char*path){FILE*f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char*b=malloc(n+1);assert(fread(b,1,n,f)==(size_t)n);b[n]=0;fclose(f);return b;}
static void contents(const char*expected){char p[MAXPATH],buf[100]={0};snprintf(p,sizeof(p),"%s/options.txt",directory);FILE*f=fopen(p,"rb");assert(f);size_t n=fread(buf,1,99,f);fclose(f);assert(n==strlen(expected)&&!strcmp(buf,expected));}
static void clear(void){char p[MAXPATH];snprintf(p,sizeof(p),"%s/options.txt",directory);unlink(p);snprintf(p,sizeof(p),"%s/options.txt.mcbox-1.tmp",directory);assert(access(p,F_OK));}
int main(void){Plan p={0};char err[512]={0};p.id="1.8.9";p.indexJson=fixture("native-v19/tests/official-assets-1.8.json");check(&p,"zh_CN");free(p.indexJson);p.indexJson=fixture("native-v19/tests/assets-0c8f43b28f837869c1e54a6aeb22c396a1548946.json");check(&p,"zh_cn");free(p.indexJson);p.indexJson="{\"objects\":{\"minecraft/lang/zh_cn.lang\":{}}}";check(&p,"zh_cn");p.indexJson="{\"objects\":{}}";check(&p,NULL);p.indexJson=NULL;p.id="1.5.2";check(&p,"zh_CN");p.id="1.0";check(&p,NULL);p.id="b1.8.1";check(&p,NULL);p.id="16w32a";check(&p,NULL);
strcpy(directory,"/tmp/mcpc-language-XXXXXX");assert(mkdtemp(directory));p.id="1.5.2";assert(initialize_language(&p,err));contents("lang:zh_CN\r\n");p.indexJson="{\"objects\":{\"minecraft/lang/zh_cn.json\":{}}}";assert(initialize_language(&p,err));contents("lang:zh_CN\r\n");clear();assert(initialize_language(&p,err));contents("lang:zh_cn\r\n");clear();failWrite=1;assert(!initialize_language(&p,err));failWrite=0;clear();failMove=1;assert(!initialize_language(&p,err));failMove=0;clear();race=1;assert(initialize_language(&p,err));contents("lang:ja_jp\n");race=0;clear();unsafe=1;assert(!initialize_language(&p,err));unsafe=0;clear();assert(!rmdir(directory));puts("PASS production language selection: real official old/new indexes, transitional lowercase .lang, no-language releases; no-replace settings init, existing preferences preserved, failed write/publication cleanup, publication race and unsafe path. Real host files; Windows calls substituted.");}
