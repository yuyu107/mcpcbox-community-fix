// Exact ABI for the user-supplied mcoption.dll, SHA-1:
// e568c557abf461fc5d8f1918b3d45412f9a01976 (621808 bytes).
// GameInfo stride 0x4b0; +4 is serialized as "islocal", +8 as "resid",
// +0xc as "status", +0xd8 version name, +0x268 game root.
// +0x4a8 is versionType: the original pure-list predicate requires 1.
extern int NativeOptionGuard(void *module);
static size_t bounded_wlen(const wchar_t *s,size_t cap){size_t n=0;while(n<cap&&s[n])n++;return n;}
static int normalize_game_path(const wchar_t *source,wchar_t out[260]){
 if(!wcsncmp(source,L"\\\\?\\",4))source+=4;
 size_t n=bounded_wlen(source,260);if(!n||n>=260)return 0;
 memcpy(out,source,(n+1)*sizeof(wchar_t));for(size_t i=0;i<n;i++)if(out[i]=='/')out[i]='\\';
 while(n>3&&out[n-1]=='\\')out[--n]=0;return 1;
}
static unsigned char *find_installed(void *opt,const wchar_t *root,const wchar_t *version){
 uintptr_t begin=*(uintptr_t*)((char*)opt+0x148),end=*(uintptr_t*)((char*)opt+0x14c);
 if(end<begin||(end-begin)%0x4b0||(end-begin)/0x4b0>100000)return NULL;
 for(uintptr_t row=begin;row<end;row+=0x4b0){
  wchar_t normalized[260];const wchar_t *name=(wchar_t*)(row+0xd8);
  if(*(DWORD*)(row+0xc)!=2||bounded_wlen(name,100)>=100||_wcsicmp(name,version))continue;
  if(normalize_game_path((wchar_t*)(row+0x268),normalized)&&!_wcsicmp(normalized,root))return (unsigned char*)row;
 }
 return NULL;
}
static uint32_t official_resid(void *opt,const wchar_t *root){
 uint32_t hash=2166136261u;
 for(const wchar_t *p=root;*p;p++){wchar_t c=*p;if(c>='A'&&c<='Z')c+=32;hash=(hash^(uint16_t)c)*16777619u;}
 uint32_t id=0x50000000u|(hash&0x0fffffffu);
 uintptr_t begin=*(uintptr_t*)((char*)opt+0x148),end=*(uintptr_t*)((char*)opt+0x14c);
 for(int attempt=0;attempt<100000;attempt++,id++){
  int found=0;for(uintptr_t row=begin;row<end;row+=0x4b0)if(*(DWORD*)(row+8)==id){found=1;break;}
  if(!found)return id;
 }
 return 0;
}
static int register_official(void *opt,const wchar_t *path,char *err){
 HMODULE module=GetModuleHandleW(L"mcoption.dll");
 if(!module||!NativeOptionGuard(module)||*(uintptr_t*)opt!=(uintptr_t)module+0x6ae2c){
  strcpy(err,"Official version categorization requires the verified mcoption.dll supplied for v11");return 0;
 }
 wchar_t root[260];if(!normalize_game_path(path,root)){strcpy(err,"Invalid official game root");return 0;}
 const wchar_t *version=wcsrchr(root,'\\');version=version?version+1:root;
 if(!*version||wcslen(version)>=100){strcpy(err,"Version ID exceeds the original game record capacity");return 0;}
 unsigned char *record=find_installed(opt,root,version);
 if(!record){
  void **vt=*(void***)opt;
  if(!((unsigned char(__attribute__((thiscall))*)(void*,const wchar_t*))vt[0x9c/4])(opt,path)){
   strcpy(err,"Original version importer did not accept the installed directory");return 0;
  }
  record=find_installed(opt,root,version);
 }
 if(!record){strcpy(err,"Imported record does not match the exact official version directory");return 0;}
 if(record[4]||!*(DWORD*)(record+8)){
  uint32_t id=official_resid(opt,root);if(!id){strcpy(err,"Cannot allocate an official version resource ID");return 0;}
  record[4]=0;*(DWORD*)(record+8)=id;
 }
 *(DWORD*)(record+0x4a8)=1; // Pure version: original UI filters this independently of islocal.
 // Reuse this exact DLL's own configuration serializer and UI observer notification.
 ((void(__attribute__((thiscall))*)(void*))((char*)module+0xa350))(opt);
 ((void(__stdcall*)(void*))((char*)module+0x10070))(opt);
 return 1;
}
static int unregister_official(void *opt,const wchar_t *path,char *err){
 HMODULE module=GetModuleHandleW(L"mcoption.dll");
 if(!module||!NativeOptionGuard(module)||*(uintptr_t*)opt!=(uintptr_t)module+0x6ae2c){strcpy(err,"Cannot clear version registration with an unverified mcoption.dll");return 0;}
 wchar_t root[260];if(!normalize_game_path(path,root)){strcpy(err,"Invalid installed version directory");return 0;}
 const wchar_t*version=wcsrchr(root,'\\');version=version?version+1:root;
 unsigned char*record=find_installed(opt,root,version);if(!record)return 1;
 if(record[4]||*(DWORD*)(record+0x4a8)!=1){strcpy(err,"Matching version is not an adapter-registered pure installation");return 0;}
 DWORD id=*(DWORD*)record;void**vt=*(void***)opt;
 // Exact supplied DLL removeVersionBySign(id,false): clear registration only.
 // The true branch schedules filesystem removal; it must not be used here.
 ((void(__attribute__((thiscall))*)(void*,DWORD,int))vt[0x94/4])(opt,id,0);
 if(find_installed(opt,root,version)){strcpy(err,"Original option module did not clear the deleted version registration");return 0;}
 ((void(__attribute__((thiscall))*)(void*))((char*)module+0xa350))(opt);return 1;
}

