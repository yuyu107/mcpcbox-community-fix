#ifndef MCPC_FILE_RETRY_H
#define MCPC_FILE_RETRY_H
/* Win32 sharing/lock/access errors only. Never retry a hash mismatch, missing
   path, disk-full error or a cancelled task as if it were a network failure. */
static int file_retry_again(unsigned *waits,unsigned long error){
 if((error!=32&&error!=33&&error!=5)||*waits>=60||stopped())return 0;
 ++*waits;Sleep(50);return !stopped();
}
static int verified_file(const wchar_t*path,const char*hash,int64_t size){
 unsigned waits=0;for(;;){if(verified_file_once(path,hash,size))return 1;
  unsigned long error=GetLastError();if(!file_retry_again(&waits,error)){SetLastError(error);return 0;}}
}
static int move_file_ready(const wchar_t*from,const wchar_t*to,unsigned long flags){
 unsigned waits=0;for(;;){if(stopped())return 0;if(MoveFileExW(from,to,flags))return 1;
  unsigned long error=GetLastError();if(!file_retry_again(&waits,error)){SetLastError(error);return 0;}}
}
static int copy_file_ready(const wchar_t*from,const wchar_t*to,int failExists){
 unsigned waits=0;for(;;){if(stopped())return 0;if(CopyFileW(from,to,failExists))return 1;
  unsigned long error=GetLastError();if(!file_retry_again(&waits,error)){SetLastError(error);return 0;}}
}
#endif
