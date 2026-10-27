#ifndef MCPC_CACHE_PIPELINE_H
#define MCPC_CACHE_PIPELINE_H
enum {CACHE_EXISTING_INSTALL=1,CACHE_VERIFIED_OBJECT=2,CACHE_VERIFIED_PART=3,CACHE_NATIVE_TRANSFER=4};
// Production policy shared with portable tests. Callers serialize by object
// digest. All publication and installation copies use verified independent files.
static int cache_acquire(Item *item,const wchar_t*installed,const wchar_t*object,const wchar_t*part,int*source,char*err){
 *source=0;if(stopped())return 0;
 if(verified_file(installed,item->hash,item->size)){
  if(!verified_file(object,item->hash,item->size)&&!copy_verified_file(installed,object,item->hash,item->size,err))return 0;
  *source=CACHE_EXISTING_INSTALL;return 1;
 }
 if(verified_file(object,item->hash,item->size))*source=CACHE_VERIFIED_OBJECT;
 else{
  if(verified_file(part,item->hash,item->size))*source=CACHE_VERIFIED_PART;
  else{if(!download_file_native(item,part,err))return 0;*source=CACHE_NATIVE_TRANSFER;}
  if(stopped())return 0;
  if(!verified_file(part,item->hash,item->size)){strcpy(err,"Downloaded cache object failed SHA-1/size verification");return 0;}
  if(!move_file_ready(part,object,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){errwin(err,"Cannot publish verified cache object");return 0;}
 }
 if(stopped())return 0;
 return copy_verified_file(object,installed,item->hash,item->size,err);
}
#endif
