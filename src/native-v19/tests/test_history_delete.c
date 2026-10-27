#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
typedef unsigned long DWORD;
typedef int32_t HRESULT;
#define COINIT_APARTMENTTHREADED 2
#define FAILED(x) ((x)<0)
static int comFailure,comBalance,comInits;
static HRESULT CoInitializeEx(void*p,int flags){assert(!p&&flags==2);comInits++;if(comFailure)return -1;comBalance++;return 0;}
static void CoUninitialize(void){assert(comBalance>0);comBalance--;}
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 16
#define FILE_ATTRIBUTE_REPARSE_POINT 1024
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define FO_DELETE 3
#define FOF_ALLOWUNDO 64
#define FOF_WANTNUKEWARNING 0x4000
typedef struct {void*hwnd;unsigned wFunc;const wchar_t*pFrom,*pTo;unsigned fFlags;int fAnyOperationsAborted;} SHFILEOPSTRUCTW;
static int shellCalls,aborted,result,remaining,reparse,denied,missing;
static DWORD lastError;static wchar_t lastPath[1026],shellPaths[100][1026];static int sourceCount,batchMode;
static int _wcsnicmp(const wchar_t*a,const wchar_t*b,size_t n){for(size_t i=0;i<n;i++){wint_t x=towlower(a[i]),y=towlower(b[i]);if(x!=y)return x<y?-1:1;if(!x)break;}return 0;}
static DWORD GetLastError(void){return lastError;}
static DWORD GetFileAttributesW(const wchar_t *s){
 if(batchMode){const wchar_t*p=wcsstr(s,L"file");int index=-1;if(p)swscanf(p,L"file%d.zip",&index);
  if(index==99||(shellCalls&&index>=0&&index%2==0)){lastError=ERROR_FILE_NOT_FOUND;return INVALID_FILE_ATTRIBUTES;}return 0;}

 if(denied){lastError=5;return INVALID_FILE_ATTRIBUTES;}
 if(missing||(!remaining&&shellCalls)){lastError=ERROR_FILE_NOT_FOUND;return INVALID_FILE_ATTRIBUTES;}
 return (wcsstr(s,L".zip")?0:FILE_ATTRIBUTE_DIRECTORY)|(reparse?FILE_ATTRIBUTE_REPARSE_POINT:0);
}
static void errwin(char*err,const char*step){snprintf(err,512,"%s (error %lu)",step,lastError);}
static int SHFileOperationW(SHFILEOPSTRUCTW*op){
 shellCalls++;assert(op->wFunc==FO_DELETE&&op->fFlags==(FOF_ALLOWUNDO|FOF_WANTNUKEWARNING)&&!op->pTo);
 sourceCount=0;for(const wchar_t*p=op->pFrom;*p;p+=wcslen(p)+1){assert(sourceCount<100);wcscpy(shellPaths[sourceCount++],p);}assert(sourceCount>0);wcscpy(lastPath,op->pFrom);op->fAnyOperationsAborted=aborted;return result;
}
#include "../history_delete.h"
static void reset(void){assert(!comBalance);comFailure=comInits=sourceCount=batchMode=0;shellCalls=aborted=result=remaining=reparse=denied=missing=0;lastPath[0]=0;lastError=0;}
int main(void){char err[512]={0};
 reset();assert(history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err));assert(!wcscmp(lastPath,L"D:\\MCBox\\download\\file.zip"));
 reset();assert(history_delete_path(L"\\\\?\\D:/MCBox/game/1.8.9",1,err));assert(!wcscmp(lastPath,L"D:\\MCBox\\game\\1.8.9"));
 const wchar_t*bad[]={L"D:\\",L"D:\\MCBox\\game",L"D:\\MCBox",L"relative\\file.zip",L"D:\\MCBox\\game\\..",L"D:\\MCBox\\game\\one\\..\\two",L"D:\\MCBox\\game\\x*",L"D:\\MCBox\\game\\x ",L"D:\\MCBox\\game\\x.",L"D:\\MCBox\\game\\x:stream"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){reset();assert(!history_delete_path(bad[i],1,err));assert(!shellCalls);}
 reset();assert(!history_delete_path(L"D:\\MCBox\\download",0,err)&&!shellCalls); // Never delete a file's containing folder.
 reset();assert(!history_delete_path(L"D:\\MCBox\\game\\file.zip",1,err)&&!shellCalls);
 reset();reparse=1;assert(!history_delete_path(L"D:\\MCBox\\game\\1.8.9",1,err)&&!shellCalls);
 reset();aborted=1;remaining=1;assert(!history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err));
 reset();result=5;remaining=1;assert(!history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err));
 reset();remaining=1;assert(!history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err));
 reset();denied=1;assert(!history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err)&&!shellCalls);
 reset();missing=1;assert(history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err)&&!shellCalls);
 reset();comFailure=1;assert(!history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err)&&!shellCalls);
 reset();aborted=1;assert(history_delete_path(L"D:\\MCBox\\download\\file.zip",0,err)); // Partial completion: this target is confirmed gone.
 HistoryDeleteItem *items=calloc(100,sizeof(*items));wchar_t names[100][260];assert(items);
 reset();batchMode=1;aborted=1;
 for(int i=0;i<100;i++){swprintf(names[i],260,L"D:\\MCBox\\download\\file%d.zip",i);items[i].stored=names[i];}
 history_delete_items(items,100);assert(shellCalls==1&&comInits==1&&sourceCount==99);
 for(int i=0;i<100;i++)assert(items[i].ok==(i==99||i%2==0));
 reset();memset(items,0,100*sizeof(*items));items[0].stored=L"D:\\MCBox\\download\\file.zip";items[1].stored=L"d:\\MCBox\\download\\file.zip";
 history_delete_items(items,2);assert(shellCalls==1&&sourceCount==1&&items[0].ok&&items[1].ok);
 reset();memset(items,0,100*sizeof(*items));items[0].stored=L"D:\\MCBox\\game\\1.8.9";items[0].grouped=1;items[1].stored=L"D:\\MCBox\\game\\1.8.9\\.minecraft\\client.zip";
 history_delete_items(items,2);assert(shellCalls==1&&sourceCount==1&&items[0].ok&&items[1].ok&&!wcscmp(lastPath,items[0].stored));
 reset();memset(items,0,100*sizeof(*items));items[0].stored=L"relative.zip";items[1].stored=L"D:\\MCBox\\download\\file.zip";
 history_delete_items(items,2);assert(!items[0].ok&&items[1].ok&&shellCalls==1&&sourceCount==1);
 free(items);
 puts("PASS 100 targets in one shell+COM call, partial cancellation, duplicate/ancestor coverage and explicit stored-file/version-folder deletion, root/alias/type/link rejection, double-NUL shell target, recycle/confirmation flags, cancellation/failure/existing/missing outcomes. Windows APIs mocked; no real files deleted.");
}
