#ifndef MCPC_CACHE_KEYS_H
#define MCPC_CACHE_KEYS_H
#include <string.h>
#include <wchar.h>
static int cache_key(const char*hash,char out[41]){
 if(!hash||strlen(hash)!=40)return 0;
 for(int i=0;i<40;i++){unsigned char c=(unsigned char)hash[i];if(c>='A'&&c<='F')c+=32;if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return 0;out[i]=(char)c;}out[40]=0;return 1;
}
static unsigned cache_stripe(const char key[41]){unsigned n=2166136261u;for(int i=0;i<40;i++)n=(n^(unsigned char)key[i])*16777619u;return n%64;}
static wchar_t cache_path_char(wchar_t c){if(c=='/')c='\\';if(c>='A'&&c<='Z')c+=32;return c;}
static int cache_descendant(const wchar_t*root,const wchar_t*path){
 if(!wcsncmp(root,L"\\\\?\\",4))root+=4;if(!wcsncmp(path,L"\\\\?\\",4))path+=4;
 size_t n=wcslen(root),p=wcslen(path);while(n&&cache_path_char(root[n-1])=='\\')n--;
 if(!n||p<=n+1||cache_path_char(path[n])!='\\')return 0;
 for(size_t i=0;i<n;i++)if(cache_path_char(root[i])!=cache_path_char(path[i]))return 0;return 1;
}
#endif
