#ifndef MCPC_HISTORY_DELETE_H
#define MCPC_HISTORY_DELETE_H
#include <stdlib.h>
// Validate the exact stored item, then use the Windows shell deletion UI.
// No path is accepted from an HTTP query. The containing directory of a
// single-file record must NEVER be substituted as the deletion target.
static int history_prepare_delete(const wchar_t *stored,int grouped,wchar_t target[1026],char *err){
const wchar_t*p=!wcsncmp(stored,L"\\\\?\\",4)?stored+4:stored;
 size_t n=wcslen(p);if(n<4||n>=260){strcpy(err,"删除目标路径为空或超出原盒子支持的长度");return 0;}
 if(!((p[0]&&p[1]==':'&&(p[2]=='\\'||p[2]=='/'))||!wcsncmp(p,L"\\\\",2))){strcpy(err,"下载记录没有有效的绝对路径");return 0;}
 memcpy(target,p,(n+1)*sizeof(wchar_t));for(size_t i=0;i<n;i++)if(target[i]=='/')target[i]='\\';
 // Reject roots, traversal components, alternate streams, wildcards and
 // shell aliases before invoking a recursive filesystem operation.
 if(target[n-1]=='\\'){strcpy(err,"拒绝删除磁盘根目录或未明确指向文件的路径");return 0;}
 for(size_t i=2;i<n;i++)if(target[i]==':'||target[i]=='*'||target[i]=='?'||target[i]=='"'||target[i]=='|'||target[i]=='<'||target[i]=='>'){strcpy(err,"删除目标包含不支持的路径字符");return 0;}
 const wchar_t*component=target+((target[1]==':')?3:2);
 for(const wchar_t*q=component;;q++)if(*q=='\\'||!*q){size_t len=(size_t)(q-component);if(!len||component[len-1]=='.'||component[len-1]==' '){strcpy(err,"拒绝含空目录或路径别名的删除目标");return 0;}if(!*q)break;component=q+1;}
 if(grouped){
  wchar_t*last=wcsrchr(target,'\\');if(!last||!last[1]){strcpy(err,"无法确定版本文件夹");return 0;}
  wchar_t*parent=last;while(parent>target&&parent[-1]!='\\')parent--;
  if((size_t)(last-parent)!=4||_wcsnicmp(parent,L"game",4)){strcpy(err,"拒绝删除版本目录以外的位置");return 0;}
 }
 DWORD attrs=GetFileAttributesW(target);
 if(attrs==INVALID_FILE_ATTRIBUTES){DWORD error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return 2;errwin(err,"无法访问要删除的文件");return 0;}
 if(!!(attrs&FILE_ATTRIBUTE_DIRECTORY)!=!!grouped){strcpy(err,grouped?"版本记录的目标不是文件夹":"单文件记录意外指向文件夹，已停止删除");return 0;}
 if(attrs&FILE_ATTRIBUTE_REPARSE_POINT){strcpy(err,"目标是链接或重解析点，请在资源管理器中检查后处理");return 0;}
 target[n+1]=0;return 1;
}
typedef struct {const wchar_t*stored;int grouped,ok,valid;wchar_t target[1026];char error[512];} HistoryDeleteItem;
static void history_delete_items(HistoryDeleteItem *items,int count){
 if(count<1||count>100)return;int eligible=0;
 for(int i=0;i<count;i++){
  items[i].ok=items[i].valid=0;items[i].error[0]=0;
  if(!items[i].stored){strcpy(items[i].error,"Completed record no longer exists");continue;}
  int result=history_prepare_delete(items[i].stored,items[i].grouped,items[i].target,items[i].error);
  items[i].ok=result==2;items[i].valid=result==1;eligible+=items[i].valid;
 }
 if(!eligible)return;
 wchar_t *sources=calloc((size_t)count*261+2,sizeof(wchar_t));size_t length=0;
 if(!sources){for(int i=0;i<count;i++)if(items[i].valid)strcpy(items[i].error,"Cannot allocate batch deletion paths");return;}
 for(int i=0;i<count;i++)if(items[i].valid){
  int covered=0;size_t own=wcslen(items[i].target);
  for(int j=0;j<count;j++)if(j!=i&&items[j].valid){size_t other=wcslen(items[j].target);
   if((j<i&&own==other&&!_wcsnicmp(items[i].target,items[j].target,own))||
      (items[j].grouped&&other<own&&items[i].target[other]=='\\'&&!_wcsnicmp(items[i].target,items[j].target,other))){covered=1;break;}
  }
  if(!covered){memcpy(sources+length,items[i].target,(own+1)*sizeof(wchar_t));length+=own+1;}
 }
 sources[length]=0;SHFILEOPSTRUCTW operation;memset(&operation,0,sizeof(operation));
 operation.wFunc=FO_DELETE;operation.pFrom=sources;operation.fFlags=FOF_ALLOWUNDO|FOF_WANTNUKEWARNING;
 HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 if(FAILED(initialized)){for(int i=0;i<count;i++)if(items[i].valid)strcpy(items[i].error,"无法初始化 Windows 文件操作界面，记录已保留");free(sources);return;}
 int result=SHFileOperationW(&operation);CoUninitialize();free(sources);
 // A cancelled shell batch can have partially completed. Only targets whose
 // absence is confirmed may lose their records; untouched/failed items remain.
 for(int i=0;i<count;i++)if(items[i].valid){
  DWORD attrs=GetFileAttributesW(items[i].target);
  if(attrs==INVALID_FILE_ATTRIBUTES){DWORD error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND){items[i].ok=1;continue;}errwin(items[i].error,"无法确认文件是否已删除，记录已保留");}
  else if(result||operation.fAnyOperationsAborted)snprintf(items[i].error,512,"删除未完成或已取消，记录已保留（Windows Shell code %d）",result);
  else strcpy(items[i].error,"文件仍然存在，下载记录已保留");
 }
}
static int history_delete_path(const wchar_t *stored,int grouped,char *err){
 HistoryDeleteItem item;memset(&item,0,sizeof(item));item.stored=stored;item.grouped=grouped;history_delete_items(&item,1);strcpy(err,item.error);return item.ok;
}
#endif
