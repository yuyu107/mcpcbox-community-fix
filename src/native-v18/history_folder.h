#ifndef MCPC_HISTORY_FOLDER_H
#define MCPC_HISTORY_FOLDER_H
// Open a validated stored path; never accept a user-supplied shell command.
static int history_open_path(const wchar_t *stored,char *err){
 wchar_t directory[1024];
 const wchar_t*path=!wcsncmp(stored,L"\\\\?\\",4)?stored+4:stored;
 if(!*path||wcslen(path)>=1024||!((path[0]&&path[1]==':'&&(path[2]=='\\'||path[2]=='/'))||!wcsncmp(path,L"\\\\",2))){strcpy(err,"Completed path is not an absolute directory/file path");return 0;}
 wcscpy(directory,path);for(wchar_t*p=directory;*p;p++)if(*p=='/')*p='\\';
 DWORD attrs=GetFileAttributesW(directory);
 if(attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_DIRECTORY)){
  wchar_t*tail=wcsrchr(directory,'\\');if(!tail){strcpy(err,"Cannot find the containing folder");return 0;}
  if(tail==directory+2&&directory[1]==':')tail[1]=0;else *tail=0;
 }
 attrs=GetFileAttributesW(directory);if(attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_DIRECTORY)){errwin(err,"Download folder is missing or inaccessible");return 0;}
 INT_PTR launched=(INT_PTR)ShellExecuteW(NULL,L"open",directory,NULL,NULL,SW_SHOWNORMAL);
 if(launched<=32){snprintf(err,512,"Cannot open download folder (ShellExecute code %ld)",(long)launched);return 0;}return 1;
}
#endif
