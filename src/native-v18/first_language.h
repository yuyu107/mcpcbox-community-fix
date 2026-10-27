#ifndef MCPC_FIRST_LANGUAGE_H
#define MCPC_FIRST_LANGUAGE_H
/* Choose exact case from verified official language assets, including snapshots.
   Releases before external asset indexes need a narrow legacy fallback. */
static const char *first_language(Plan *plan) {
  if (plan->indexJson) {
    Doc d={0};char error[512]={0};const char *language=NULL;
    if (!doc_parse(&d,plan->indexJson,error)) return NULL;
    int objects=jprop(&d,0,"objects");
    if(jprop(&d,objects,"minecraft/lang/zh_cn.json")>=0 ||
       jprop(&d,objects,"minecraft/lang/zh_cn.lang")>=0) language="zh_cn";
    else if(jprop(&d,objects,"minecraft/lang/zh_CN.lang")>=0) language="zh_CN";
    doc_free(&d);return language;
  }
  int major=0,minor=0;char tail=0;
  if(plan->id && sscanf(plan->id,"%d.%d%c",&major,&minor,&tail)>=2 &&
     major==1 && minor>=1 && minor<=5 && (!tail || tail=='.')) return "zh_CN";
  return NULL; /* Alpha/Beta/1.0 and unknown formats: do not invent support. */
}

/* Returns success for an existing file: user settings are never modified.
   The final rename deliberately does not use MOVEFILE_REPLACE_EXISTING. */
static int initialize_language(Plan *plan,char *err) {
  const char *language=first_language(plan);if(!language)return 1;
  wchar_t path[MAXPATH],temp[MAXPATH];
  if(!game_path("options.txt",path,err)||!ensure_dirs(path,err))return 0;
  DWORD a=GetFileAttributesW(path);
  if(a!=INVALID_FILE_ATTRIBUTES)return 1;
  if(GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND){errwin(err,"Cannot inspect initial language settings");return 0;}
  if(wcslen(path)+32>=MAXPATH){strcpy(err,"Language settings path too long");return 0;}
  swprintf(temp,MAXPATH,L"%ls.mcbox-%lu.tmp",path,(unsigned long)GetCurrentThreadId());
  HANDLE f=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(f==INVALID_HANDLE_VALUE){errwin(err,"Cannot create initial language settings");return 0;}
  char data[32];DWORD written=0;int length=snprintf(data,sizeof(data),"lang:%s\r\n",language);
  int ok=WriteFile(f,data,length,&written,NULL)&&written==(DWORD)length;
  if(ok)ok=FlushFileBuffers(f);CloseHandle(f);
  if(ok)ok=MoveFileExW(temp,path,MOVEFILE_WRITE_THROUGH);
  if(!ok){DWORD reason=GetLastError();DeleteFileW(temp);
    if(reason==ERROR_ALREADY_EXISTS || reason==ERROR_FILE_EXISTS)return 1;
    SetLastError(reason);errwin(err,"Cannot commit initial language settings");return 0;}
  return 1;
}
#endif
