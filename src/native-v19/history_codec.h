#ifndef MCPC_HISTORY_CODEC_H
#define MCPC_HISTORY_CODEC_H
#include "history_text.h"
#define HISTORY_RECORD_LIMIT 10000
#define HISTORY_FILE_LIMIT (64u*1024u*1024u)
static int history_uint(Doc*d,int at,uint64_t*out){
 if(at<0||at>=d->n||d->t[at].type!=JSMN_PRIMITIVE)return 0;
 int start=d->t[at].start,end=d->t[at].end;if(start==end)return 0;uint64_t n=0;
 for(int i=start;i<end;i++){unsigned c=(unsigned char)d->s[i];if(c<'0'||c>'9'||n>(0x7fffffffffffffffULL-(c-'0'))/10)return 0;n=n*10+c-'0';}
 *out=n;return 1;
}
static int history_stored_path(const wchar_t*p){
 size_t n=wcslen(p);if(n<3||n>=1024)return 0;
 int drive=((p[0]>='A'&&p[0]<='Z')||(p[0]>='a'&&p[0]<='z'))&&p[1]==':'&&(p[2]=='\\'||p[2]=='/');
 if(!drive && !(n>4&&p[0]=='\\'&&p[1]=='\\'&&p[2]!='?'&&p[2]!='.'))return 0;
 for(size_t i=0;i<n;i++)if(p[i]<32||p[i]=='"'||p[i]=='*'||p[i]=='?'||(p[i]==':'&&!(drive&&i==1)))return 0;
 return 1;
}
static int history_decode(char*data,HistoryRow**out,int*count,char*err){
 *out=NULL;*count=0;Doc d={0};if(strlen(data)>HISTORY_FILE_LIMIT||!doc_parse(&d,data,err))return 0;
 int ok=0,at=jprop(&d,0,"groups"),format=jprop(&d,0,"format");uint64_t v;
 if(jskip(&d,0)!=d.n||!history_uint(&d,format,&v)||v!=1||at<0||d.t[at].type!=JSMN_ARRAY){strcpy(err,"Unsupported completed history format");goto done;}
 int total=0,end=jskip(&d,at);for(int i=at+1;i<end;i=jskip(&d,i))if(++total>HISTORY_RECORD_LIMIT){strcpy(err,"Completed history exceeds record limit");goto done;}
 HistoryRow*rows=calloc(total+1,sizeof(*rows));if(!rows){strcpy(err,"Cannot allocate stored history");goto done;}
 int index=0;
 for(int i=at+1;i<end;i=jskip(&d,i)){
  HistoryRow*r=rows+index;uint64_t id,bytes;char*name=NULL,*path=NULL;
  int valid=d.t[i].type==JSMN_OBJECT&&history_uint(&d,jprop(&d,i,"id"),&id)&&id<=0xffffffffULL&&(id&0xff000000)==0x62000000&&history_uint(&d,jprop(&d,i,"bytes"),&bytes);
  if(valid){name=jstr(&d,jprop(&d,i,"name"));path=jstr(&d,jprop(&d,i,"path"));valid=name&&*name&&path&&from_utf8(name,r->name,512)&&from_utf8(path,r->path,1024)&&history_stored_path(r->path);}
  free(name);free(path);
  for(int j=0;valid&&j<index;j++)if(rows[j].id==(uint32_t)id)valid=0;
  if(!valid){free(rows);strcpy(err,"Invalid or duplicate completed history record");goto done;}
  r->type=3;r->id=(uint32_t)id;r->isGroup=1;r->bytes=bytes;index++;
 }
 *out=rows;*count=total;ok=1;
done:doc_free(&d);return ok;
}
static char*history_encode(const HistoryRow*rows,int count,char*err){
 if(count<0||count>HISTORY_RECORD_LIMIT){strcpy(err,"Completed history exceeds record limit");return NULL;}
 HistoryJson b={0};history_append(&b,"{\"format\":1,\"groups\":[");
 for(int i=0;i<count&&!b.failed;i++){
  const HistoryRow*r=rows+i;char info[160];
  if((r->id&0xff000000)!=0x62000000||r->bytes>0x7fffffffffffffffULL||!r->isGroup){b.failed=1;break;}
  char*name=utf8(r->name),*path=utf8(r->path);
  if(!name||!*name||!path||!history_stored_path(r->path)){free(name);free(path);b.failed=1;break;}
  snprintf(info,sizeof(info),"%s{\"id\":%lu,\"bytes\":%llu,\"name\":",i?",":"",(unsigned long)r->id,r->bytes);history_append(&b,info);history_quoted(&b,name);history_append(&b,",\"path\":");history_quoted(&b,path);history_append(&b,"}");free(name);free(path);
 }
 history_append(&b,"]}");if(b.failed||b.len>HISTORY_FILE_LIMIT){free(b.data);strcpy(err,"Cannot serialize completed history");return NULL;}return b.data;
}
#endif
