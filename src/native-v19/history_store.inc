#include "history_codec.h"
static CRITICAL_SECTION historyStoreLock;
static HANDLE historyWake;
static volatile LONG historyEpoch,historyLoaded;
static LONG historySavedEpoch;
static wchar_t historyStorePath[MAXPATH];
void NativeHistoryChanged(void){InterlockedIncrement(&historyEpoch);if(historyWake)SetEvent(historyWake);}
static int history_store_path(char*err){
 if(*historyStorePath)return 1;wchar_t appdata[MAXPATH];
 DWORD n=GetEnvironmentVariableW(L"APPDATA",appdata,MAXPATH);
 if(!n||n>=MAXPATH||n+100>=MAXPATH||!history_stored_path(appdata)){strcpy(err,"无法确定当前用户的下载记录配置目录");return 0;}
 if(!wcsncmp(appdata,L"\\\\",2)){wcscpy(historyStorePath,L"\\\\?\\UNC\\");wcscat(historyStorePath,appdata+2);}
 else{wcscpy(historyStorePath,L"\\\\?\\");wcscat(historyStorePath,appdata);}
 wcscat(historyStorePath,L"\\duowan\\mcpcbox\\community-fix\\completed-history-v1.json");return 1;
}
static int NativeHistoryEnsure(char*err){
 if(InterlockedCompareExchange(&historyLoaded,0,0))return 1;
 if(!adapter_history_available()){strcpy(err,"下载中心尚未初始化，请稍后刷新");return 0;}
 if(!adapter_history_attach(err))return 0;
 EnterCriticalSection(&historyStoreLock);int ok=0;LONG baseline=historyEpoch;
 if(historyLoaded){ok=1;goto done;}if(!history_store_path(err))goto done;
 DWORD attrs=GetFileAttributesW(historyStorePath);
 if(attrs==INVALID_FILE_ATTRIBUTES){DWORD reason=GetLastError();if(reason==ERROR_FILE_NOT_FOUND||reason==ERROR_PATH_NOT_FOUND){historySavedEpoch=baseline;InterlockedExchange(&historyLoaded,1);ok=1;}else errwin(err,"Cannot inspect completed history");goto done;}
 if((attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))||!file_safe(historyStorePath)){strcpy(err,"Refusing unsafe completed history file");goto done;}
 char*data=NULL;size_t length=0;
 if(!read_file(historyStorePath,&data,&length,HISTORY_FILE_LIMIT)){errwin(err,"Cannot read completed history");goto done;}
 HistoryRow*rows=NULL;int count=0;
 if(memchr(data,0,length)){strcpy(err,"Completed history contains embedded NUL");free(data);goto done;}
 if(history_decode(data,&rows,&count,err)&&adapter_group_restore(rows,count,err)){historySavedEpoch=baseline;InterlockedExchange(&historyLoaded,1);ok=1;}
 free(rows);free(data);
done:LeaveCriticalSection(&historyStoreLock);return ok;
}
static int NativeHistoryFlush(char*err){
 if(!NativeHistoryEnsure(err))return 0;
 EnterCriticalSection(&historyStoreLock);LONG revision=historyEpoch;int ok=1;
 if(revision!=historySavedEpoch){
  HistoryRow*rows=NULL;int count=0;ok=adapter_group_export(&rows,&count,err);
  char*data=ok?history_encode(rows,count,err):NULL;free(rows);
  if(ok){ok=data&&file_safe(historyStorePath)&&write_atomic(historyStorePath,data,strlen(data),err);free(data);}
  if(ok)historySavedEpoch=revision;else if(!*err)strcpy(err,"Cannot persist completed history");
 }
 LeaveCriticalSection(&historyStoreLock);return ok;
}
static DWORD WINAPI history_store_worker(void*unused){
 (void)unused;char previous[512]={0};
 for(;;){
  if(adapter_history_available()){
   char err[512]={0};if(!NativeHistoryEnsure(err)||!NativeHistoryFlush(err)){
    if(strcmp(previous,err)){char line[700];snprintf(line,sizeof(line),"Completed history persistence: %s",err);log_event(line);snprintf(previous,sizeof(previous),"%s",err);}
   }else previous[0]=0;
  }
  if(historyWake)WaitForSingleObject(historyWake,1000);else Sleep(1000);
 }
 return 0;
}
