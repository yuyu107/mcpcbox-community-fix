static int cache_path(const char *relative,wchar_t*out,char*err){
 if(!*cacheRootDir||!relative_ok(relative)){strcpy(err,"Invalid shared-cache path");return 0;}
 wchar_t name[MAXPATH];if(!from_utf8(relative,name,MAXPATH)||wcslen(cacheRootDir)+wcslen(name)+2>=MAXPATH){strcpy(err,"Shared-cache path too long");return 0;}
 wcscpy(out,cacheRootDir);wcscat(out,L"\\");wcscat(out,name);for(wchar_t*p=out;*p;p++)if(*p=='/')*p='\\';
 if(!file_safe(out)){strcpy(err,"Refusing shared-cache file link");return 0;}return 1;
}
#include "cache_copy.h"
static int cache_index(int complete,char*err){
 size_t cap=2048+strlen(job.id)*6;
 for(int i=0;i<plannedFileCount;i++){size_t n=strlen(jobPlan.files[i].path)*6+180;if(n>64*1024*1024||cap>64*1024*1024-n){strcpy(err,"Cache index size limit exceeded");return 0;}cap+=n;}
 char*data=malloc(cap),*qid=json_quote(job.id);if(!data||!qid){free(data);free(qid);strcpy(err,"Cannot allocate cache index");return 0;}
 int n=snprintf(data,cap,"{\"version\":%s,\"complete\":%s,\"clientProbe\":%s,\"objectLayout\":\"../../objects/<sha1-first-2>/<sha1>\",\"files\":[",qid,complete?"true":"false",clientProbe?"true":"false");free(qid);size_t used=n>0?(size_t)n:cap;
 for(int i=0;i<plannedFileCount&&used<cap;i++){
  Item*item=jobPlan.files+i;char key[41];if(!cache_key(item->hash,key)){free(data);strcpy(err,"Official file has invalid SHA-1");return 0;}
  char*qpath=json_quote(item->path);if(!qpath){free(data);return 0;}
  n=snprintf(data+used,cap-used,"%s{\"path\":%s,\"sha1\":\"%s\",\"size\":%I64d}",i?",":"",qpath,key,(long long)item->size);free(qpath);if(n<0||(size_t)n>=cap-used){used=cap;break;}used+=(size_t)n;
 }
 if(used+3>=cap){free(data);strcpy(err,"Cache index overflow");return 0;}memcpy(data+used,"]}",3);used+=2;
 wchar_t path[MAXPATH];if(wcslen(cacheVersionDir)+24>=MAXPATH){free(data);return 0;}
 swprintf(path,MAXPATH,L"%ls\\%ls",cacheVersionDir,clientProbe?L"client-probe.json":L"files.json");
 int ok=write_atomic(path,data,used,err);free(data);return ok;
}
