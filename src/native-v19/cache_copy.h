#ifndef MCPC_CACHE_COPY_H
#define MCPC_CACHE_COPY_H
static int copy_verified_file(const wchar_t*from,const wchar_t*to,const char*hash,int64_t size,char*err){
 wchar_t target[MAXPATH];if(wcslen(to)+12>=MAXPATH){strcpy(err,"Cache/install temporary path too long");return 0;}wcscpy(target,to);
 if(stopped())return 0;if(!file_safe(from)||!file_safe(to)||!ensure_dirs(target,err)){if(!*err)strcpy(err,"Unsafe cache/install file path");return 0;}
 if(verified_file(to,hash,size))return 1;
 wchar_t temp[MAXPATH];if(wcslen(to)+12>=MAXPATH){strcpy(err,"Cache/install temporary path too long");return 0;}
 wcscpy(temp,to);wcscat(temp,L".mcbox.tmp");if(!file_safe(temp)){strcpy(err,"Refusing cache/install temporary file link");return 0;}
 if(!copy_file_ready(from,temp,FALSE)){errwin(err,"Cannot copy verified cache/install file");DeleteFileW(temp);return 0;}
 if(stopped()||!verified_file(temp,hash,size)){DeleteFileW(temp);if(!*err)strcpy(err,"Cache/install copy failed SHA-1/size verification");return 0;}
 if(!move_file_ready(temp,to,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){errwin(err,"Cannot commit verified cache/install copy");DeleteFileW(temp);return 0;}return 1;
}
#endif
