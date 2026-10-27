#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
#include "../history_keys.h"
typedef unsigned long DWORD;
typedef intptr_t INT_PTR;
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 16
#define SW_SHOWNORMAL 1
static wchar_t opened[1024];static intptr_t shellResult=42;static int shellCalls;
static DWORD GetFileAttributesW(const wchar_t *s){
 if(!wcscmp(s,L"D:\\MCBox\\game\\1.8.9")||!wcscmp(s,L"D:\\My Box\\download")||!wcscmp(s,L"D:\\")||!wcscmp(s,L"\\\\server\\share\\download"))return 16;
 if(!wcscmp(s,L"D:\\My Box\\download\\file.zip")||!wcscmp(s,L"D:\\file.zip"))return 0;
 return INVALID_FILE_ATTRIBUTES;
}
static void errwin(char *err,const char*step){snprintf(err,512,"%s (test)",step);}
static INT_PTR ShellExecuteW(void*a,const wchar_t*verb,const wchar_t*path,void*b,void*c,int show){
 assert(!a&&!b&&!c&&!wcscmp(verb,L"open")&&show==1);wcscpy(opened,path);shellCalls++;return shellResult;
}
#include "../history_folder.h"
int main(void){
 HistoryKey keys[100];assert(history_keys("keys=3:1644167169,1:19,3:1644167169",keys)==2);assert(keys[0].type==3&&keys[1].id==19);
 const char *bad[]={"","keys=","keys=1:0","keys=1:-1","keys=1:3,","keys=1:4294967296","keys=1:1&path=D:","keys=1:1;calc","keys=+1:1","keys=1:1x"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!history_keys(bad[i],keys));
 char query[3000]="keys=",part[40];for(int i=1;i<=100;i++){snprintf(part,sizeof(part),"%s3:%d",i==1?"":",",i);strcat(query,part);}assert(history_keys(query,keys)==100);strcat(query,",3:101");assert(!history_keys(query,keys));
 char err[512]={0};assert(history_open_path(L"D:\\MCBox\\game\\1.8.9",err)&&!wcscmp(opened,L"D:\\MCBox\\game\\1.8.9"));
 assert(history_open_path(L"D:\\My Box\\download\\file.zip",err)&&!wcscmp(opened,L"D:\\My Box\\download"));
 assert(history_open_path(L"D:\\My Box\\download\\missing.zip",err)&&!wcscmp(opened,L"D:\\My Box\\download"));
 assert(history_open_path(L"\\\\?\\D:/MCBox/game/1.8.9",err)&&!wcscmp(opened,L"D:\\MCBox\\game\\1.8.9"));
 assert(history_open_path(L"D:\\file.zip",err)&&!wcscmp(opened,L"D:\\"));
 assert(history_open_path(L"\\\\server\\share\\download\\file.zip",err)&&!wcscmp(opened,L"\\\\server\\share\\download"));
 int calls=shellCalls;assert(!history_open_path(L"relative\\path",err));assert(!history_open_path(L"http://evil.example",err));assert(!history_open_path(L"D:\\gone\\missing.zip",err));assert(shellCalls==calls);
 shellResult=5;assert(!history_open_path(L"D:\\MCBox\\game\\1.8.9",err)&&strstr(err,"code 5"));
 puts("PASS history selection validation, duplicate/overflow limits, stored directory/file paths, missing file parent, root/UNC paths and shell failures. Windows file/shell APIs mocked.");
}
