#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "core.h"
#include "adapter.h"
#include "transfer_watch.h"
#include "request_options.h"
#include "cache_keys.h"
#include "third_party/miniz_tinfl.h"
#include "ui.inc"
#include "history-ui.inc"
#define PORT 19481
#define MAXPATH 4096
#define MANIFEST                                                               \
  "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"
static CRITICAL_SECTION jobLock, manifestLock, logLock,cacheLocks[64];
static HINTERNET httpSession;
static wchar_t baseDir[MAXPATH];
static char token[65], *manifestData;
static int manifestCached;
static volatile LONG busy, cancelJob, failedJob, nextFile;
static Plan jobPlan;
static struct {
  char id[128], state[24], message[1024], error[1024], java[24],
      directory[12000];
  int totalFiles, doneFiles;
  int64_t totalBytes, doneBytes, reusedBytes;
  int cacheHits,localHits,metadataHits;
  int64_t cacheBytes,localBytes;
  DWORD startedAt,taskId;
} job;
static int systemBits;
static wchar_t gameBaseDir[MAXPATH];
static wchar_t cacheRootDir[MAXPATH],cacheVersionDir[MAXPATH];
static int registeredGame;
static DWORD taskSerial;
static int clientProbe,plannedFileCount,fileConcurrency=8;
void *original_entry;
typedef struct {
  HINTERNET connection, request;
} Reply;
static void errwin(char *err, const char *step) {
  snprintf(err, 512, "%s (Windows error %lu)", step,
           (unsigned long)GetLastError());
}
static void status_message(const char *s) {
  EnterCriticalSection(&jobLock);
  snprintf(job.message, sizeof(job.message), "%s", s);
  LeaveCriticalSection(&jobLock);
}
static int stopped(void) { return cancelJob || failedJob; }
void NativeGroupCancel(void){InterlockedExchange(&cancelJob,1);}
static void wait_group(void){while(!stopped()&&adapter_group_paused())Sleep(120);}
static void set_failure(const char *s) {
  EnterCriticalSection(&jobLock);
  if (!failedJob)
    snprintf(job.error, sizeof(job.error), "%s", s);
  InterlockedExchange(&failedJob, 1);
  LeaveCriticalSection(&jobLock);
}
static char *utf8(const wchar_t *s) {
  int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
  if (n <= 0)
    return NULL;
  char *r = malloc(n);
  if (r)
    WideCharToMultiByte(CP_UTF8, 0, s, -1, r, n, NULL, NULL);
  return r;
}
static int from_utf8(const char *s, wchar_t *out, int cap) {
  return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out, cap) >
         0;
}
static int file_safe(const wchar_t *path) {
  DWORD a = GetFileAttributesW(path);
  return a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_REPARSE_POINT);
}
static int ensure_dirs(wchar_t *p, char *err) {
  size_t n = wcslen(p);
  for (size_t i = 7; i < n; i++)
    if (p[i] == '\\') {
      wchar_t save = p[i];
      p[i] = 0;
      DWORD a = GetFileAttributesW(p);
      int ok = 1;
      if (a != INVALID_FILE_ATTRIBUTES) {
        if (!(a & FILE_ATTRIBUTE_DIRECTORY) ||
            (a & FILE_ATTRIBUTE_REPARSE_POINT))
          ok = 0;
      } else if (!CreateDirectoryW(p, NULL) &&
                 GetLastError() != ERROR_ALREADY_EXISTS)
        ok = 0;
      p[i] = save;
      if (!ok) {
        errwin(err, "Unsafe or unwritable directory");
        return 0;
      }
    }
  return 1;
}
static int path_for(const char *relative, wchar_t *out, char *err) {
  if (!relative_ok(relative)) {
    snprintf(err, 512, "Invalid installation path");
    return 0;
  }
  wchar_t tmp[MAXPATH];
  if (!from_utf8(relative, tmp, MAXPATH) ||
      wcslen(baseDir) + wcslen(tmp) + 2 >= MAXPATH) {
    snprintf(err, 512, "Installation path is too long");
    return 0;
  }
  wcscpy(out, baseDir);
  wcscat(out, L"\\");
  wcscat(out, tmp);
  for (wchar_t *p = out; *p; p++)
    if (*p == '/')
      *p = '\\';
  if (!file_safe(out)) {
    snprintf(err, 512, "Refusing file link");
    return 0;
  }
  return 1;
}
static int game_path(const char *relative,wchar_t *out,char *err){
 if(!relative_ok(relative)||!*gameBaseDir){snprintf(err,512,"Box game directory not ready or invalid relative path");return 0;}
 wchar_t name[MAXPATH];if(!from_utf8(relative,name,MAXPATH)||wcslen(gameBaseDir)+wcslen(name)+2>=MAXPATH){snprintf(err,512,"Game path is too long");return 0;}
 wcscpy(out,gameBaseDir);wcscat(out,L"\\");wcscat(out,name);for(wchar_t *p=out;*p;p++)if(*p=='/')*p='\\';
 if(!file_safe(out)){snprintf(err,512,"Refusing game file link");return 0;}return 1;
}

static void log_event(const char *message) {
  wchar_t path[MAXPATH];
  char err[512];
  if (!path_for("MCPCBox-native-download.log", path, err) ||
      !ensure_dirs(path, err))
    return;
  EnterCriticalSection(&logLock);
  HANDLE f = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE){LeaveCriticalSection(&logLock);return;}
  DWORD n;
  WriteFile(f, message, (DWORD)strlen(message), &n, NULL);
  WriteFile(f, "\r\n", 2, &n, NULL);
  CloseHandle(f);
  LeaveCriticalSection(&logLock);
}
static int read_file(const wchar_t *path, char **data, size_t *length,
                     size_t limit) {
  HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return 0;
  LARGE_INTEGER n;
  if (!GetFileSizeEx(f, &n) || n.QuadPart < 0 || (uint64_t)n.QuadPart > limit) {
    CloseHandle(f);
    return 0;
  }
  char *b = malloc((size_t)n.QuadPart + 1);
  if (!b) {
    CloseHandle(f);
    return 0;
  }
  size_t got = 0;
  while (got < (size_t)n.QuadPart) {
    DWORD m = 0, ask = (DWORD)(((size_t)n.QuadPart - got) > 65536
                                   ? 65536
                                   : ((size_t)n.QuadPart - got));
    if (!ReadFile(f, b + got, ask, &m, NULL) || !m) {
      free(b);
      CloseHandle(f);
      return 0;
    }
    got += m;
  }
  CloseHandle(f);
  b[got] = 0;
  *data = b;
  *length = got;
  return 1;
}
static int write_atomic(const wchar_t *path, const void *data, size_t size,
                        char *err) {
  wchar_t target[MAXPATH], temp[MAXPATH];
  if (wcslen(path) + 6 >= MAXPATH)
    return 0;
  wcscpy(target, path);
  if (!ensure_dirs(target, err))
    return 0;
  wcscpy(temp, path);
  wcscat(temp, L".tmp");
  if (!file_safe(temp)) {
    snprintf(err, 512, "Refusing temporary file link");
    return 0;
  }
  HANDLE f = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE) {
    errwin(err, "Cannot create file");
    return 0;
  }
  size_t at = 0;
  int ok = 1;
  while (at < size) {
    DWORD wrote = 0, n = (DWORD)((size - at) > 65536 ? 65536 : (size - at));
    if (!WriteFile(f, (const char *)data + at, n, &wrote, NULL) || wrote != n) {
      ok = 0;
      break;
    }
    at += n;
  }
  if (!FlushFileBuffers(f))
    ok = 0;
  CloseHandle(f);
  if (ok)
    ok = MoveFileExW(temp, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  if (!ok) {
    errwin(err, "Cannot commit file");
    DeleteFileW(temp);
  }
  return ok;
}
static int sha_memory(const void *data, size_t n, char out[41]) {
  HCRYPTPROV provider = 0;
  HCRYPTHASH hash = 0;
  BYTE digest[20];
  DWORD size = 20;
  int ok = CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
                                CRYPT_VERIFYCONTEXT) &&
           CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash);
  if (ok)
    ok = CryptHashData(hash, data, (DWORD)n, 0) &&
         CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0);
  if (hash)
    CryptDestroyHash(hash);
  if (provider)
    CryptReleaseContext(provider, 0);
  if (ok) {
    for (int i = 0; i < 20; i++)
      sprintf(out + i * 2, "%02x", digest[i]);
    out[40] = 0;
  }
  return ok;
}
static int verified_file_once(const wchar_t *path, const char *expected,
                         int64_t length) {
  HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return 0;
  LARGE_INTEGER n;
  if (!GetFileSizeEx(f, &n)) {DWORD reason=GetLastError();CloseHandle(f);SetLastError(reason);return 0;}
  if(n.QuadPart!=length){CloseHandle(f);SetLastError(ERROR_CRC);return 0;}
  HCRYPTPROV provider = 0;
  HCRYPTHASH hash = 0;
  int ok = CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
                                CRYPT_VERIFYCONTEXT) &&
           CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash);
  BYTE buf[65536], digest[20];
  DWORD got = 0, size = 20;
  if (ok)
    for (;;) {
      if (!ReadFile(f, buf, sizeof(buf), &got, NULL)) {
        ok = 0;
        break;
      }
      if (!got)
        break;
      if (!CryptHashData(hash, buf, got, 0)) {
        ok = 0;
        break;
      }
    }
  DWORD failure=ok?0:GetLastError();
  CloseHandle(f);
  if (ok) {
    ok = CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0);if(!ok)failure=GetLastError();}
  if (hash)
    CryptDestroyHash(hash);
  if (provider)
    CryptReleaseContext(provider, 0);
  char text[41];
  if (ok) {
    for (int i = 0; i < 20; i++)
      sprintf(text + i * 2, "%02x", digest[i]);
    text[40] = 0;
    ok = !strcmp(text, expected);if(!ok)failure=ERROR_CRC;
  }
  if(!ok)SetLastError(failure?failure:ERROR_CRC);return ok;
}
#include "file_retry.h"
#include "cache_runtime.inc"
int NativeOptionGuard(void *module){
 static void *approved;static int accepted;
 if(approved==module)return accepted;
 wchar_t path[MAXPATH];DWORD n=GetModuleFileNameW((HMODULE)module,path,MAXPATH);
 if(!n||n>=MAXPATH)return 0;
 accepted=verified_file(path,"e568c557abf461fc5d8f1918b3d45412f9a01976",621808);approved=module;return accepted;
}
extern int native_network_init(void);
extern void native_network_report(char *out,unsigned size);
static void reply_close(Reply *r) {
  if (r->request)
    WinHttpCloseHandle(r->request);
  if (r->connection)
    WinHttpCloseHandle(r->connection);
  memset(r, 0, sizeof(*r));
}
static int open_url(const char *source, Reply *r, char *err) {
  char address[8192];
  if (strlen(source) >= sizeof(address)) {
    snprintf(err, 512, "URL too long");
    return 0;
  }
  strcpy(address, source);
  memset(r, 0, sizeof(*r));
  for (int i = 0; i < 6; i++) {
    if (!official_url(address)) {
      snprintf(err, 512, "Non-official URL rejected");
      return 0;
    }
    wchar_t url[8192], host[256], path[8192], extra[2048];
    if (!from_utf8(address, url, 8192))
      return 0;
    URL_COMPONENTS c;
    memset(&c, 0, sizeof(c));
    c.dwStructSize = sizeof(c);
    c.lpszHostName = host;
    c.dwHostNameLength = 256;
    c.lpszUrlPath = path;
    c.dwUrlPathLength = 8192;
    c.lpszExtraInfo = extra;
    c.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(url, 0, 0, &c) || c.nScheme != INTERNET_SCHEME_HTTPS) {
      errwin(err, "Invalid HTTPS URL");
      return 0;
    }
    if (wcslen(path) + wcslen(extra) >= 8192) {
      snprintf(err, 512, "URL path too long");
      return 0;
    }
    wcscat(path, extra);
    r->connection = WinHttpConnect(httpSession, host, c.nPort, 0);
    if (!r->connection) {
      errwin(err, "HTTPS connect failed");
      return 0;
    }
    r->request = WinHttpOpenRequest(
        r->connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!r->request) {
      errwin(err, "HTTPS request failed");
      reply_close(r);
      return 0;
    }
    DWORD flags = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(r->request, WINHTTP_OPTION_DISABLE_FEATURE, &flags,
                     sizeof(flags));
    if (!WinHttpSendRequest(r->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(r->request, NULL)) {
      errwin(err, "HTTPS/TLS request failed");
      reply_close(r);
      return 0;
    }
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(
            r->request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
            WINHTTP_NO_HEADER_INDEX)) {
      errwin(err, "Missing HTTP status");
      reply_close(r);
      return 0;
    }
    if (status >= 300 && status < 400) {
      wchar_t location[8192];
      size = sizeof(location);
      int ok = WinHttpQueryHeaders(r->request, WINHTTP_QUERY_LOCATION,
                                   WINHTTP_HEADER_NAME_BY_INDEX, location,
                                   &size, WINHTTP_NO_HEADER_INDEX);
      reply_close(r);
      if (!ok) {
        snprintf(err, 512, "Redirect missing location");
        return 0;
      }
      char *next = utf8(location);
      if (!next || !official_url(next) || strlen(next) >= sizeof(address)) {
        free(next);
        snprintf(err, 512, "Unexpected redirect rejected");
        return 0;
      }
      strcpy(address, next);
      free(next);
      continue;
    }
    if (status != 200) {
      snprintf(err, 512, "Official server returned HTTP %lu",
               (unsigned long)status);
      reply_close(r);
      return 0;
    }
    return 1;
  }
  snprintf(err, 512, "Too many redirects");
  return 0;
}
static char *fetch_json_network(const char *url, const char *hash, int64_t expected,
                        char *err) {
  Reply r;
  if (!open_url(url, &r, err))
    return NULL;
  size_t cap = 65536, used = 0;
  char *data = malloc(cap + 1);
  if (!data) {
    reply_close(&r);
    return NULL;
  }
  for (;;) {
    DWORD got = 0;
    if (cap - used < 65536) {
      if (cap >= 32 * 1024 * 1024) {
        free(data);
        reply_close(&r);
        snprintf(err, 512, "Metadata size limit exceeded");
        return NULL;
      }
      cap *= 2;
      char *v = realloc(data, cap + 1);
      if (!v) {
        free(data);
        reply_close(&r);
        return NULL;
      }
      data = v;
    }
    if (!WinHttpReadData(r.request, data + used, 65536, &got)) {
      errwin(err, "Metadata read failed");
      free(data);
      reply_close(&r);
      return NULL;
    }
    if (!got)
      break;
    used += got;
  }
  reply_close(&r);
  data[used] = 0;
  if (expected > 0 && (int64_t)used != expected) {
    snprintf(err, 512, "Metadata size mismatch");
    free(data);
    return NULL;
  }
  if (hash && *hash) {
    char digest[41];
    if (!sha_memory(data, used, digest) || strcmp(hash, digest)) {
      snprintf(err, 512, "Metadata SHA-1 mismatch");
      free(data);
      return NULL;
    }
  }
  Doc doc;
  if (!doc_parse(&doc, data, err)) {
    free(data);
    return NULL;
  }
  doc_free(&doc);
  return data;
}
static char *fetch_json(const char*url,const char*hash,int64_t expected,char*err){
 if(!hash||!*hash||!*cacheRootDir)return fetch_json_network(url,hash,expected,err);
 char key[41],relative[96];wchar_t path[MAXPATH];
 if(!cache_key(hash,key)){strcpy(err,"Invalid official metadata digest");return NULL;}
 snprintf(relative,sizeof(relative),"metadata/%s.json",key);
 if(!cache_path(relative,path,err)||!ensure_dirs(path,err))return NULL;
 char*data=NULL;size_t length=0;
 if(read_file(path,&data,&length,32*1024*1024)){
  char digest[41];Doc doc;memset(&doc,0,sizeof(doc));
  int valid=(expected<=0||(int64_t)length==expected)&&sha_memory(data,length,digest)&&!strcmp(digest,key)&&doc_parse(&doc,data,err);
  doc_free(&doc);if(valid){EnterCriticalSection(&jobLock);job.metadataHits++;LeaveCriticalSection(&jobLock);return data;}free(data);data=NULL;*err=0;
 }
 data=fetch_json_network(url,key,expected,err);
 if(data){char cacheError[512]={0};if(!write_atomic(path,data,strlen(data),cacheError))log_event(cacheError);}
 return data;
}
static char *load_manifest(int refresh, char *err) {
  EnterCriticalSection(&manifestLock);
  if (!manifestData || refresh) {
    char *newData = fetch_json(MANIFEST, NULL, 0, err);
    wchar_t path[MAXPATH];
    char patherr[512];
    int pathok =
        path_for("official-cache/version_manifest_v2.json", path, patherr);
    if (newData) {
      Doc d;
      int valid = doc_parse(&d, newData, err) && jprop(&d, 0, "versions") >= 0;
      doc_free(&d);
      if (valid) {
        free(manifestData);
        manifestData = newData;
        manifestCached = 0;
        if (pathok)
          write_atomic(path, newData, strlen(newData), patherr);
      } else
        free(newData);
    } else {
      if (!manifestData && pathok) {
        size_t n = 0;
        char *cache = NULL;
        if (read_file(path, &cache, &n, 4 * 1024 * 1024)) {
          Doc d;
          if (doc_parse(&d, cache, patherr) && jprop(&d, 0, "versions") >= 0) {
            manifestData = cache;
            cache = NULL;
          }
          doc_free(&d);
          free(cache);
        }
      }
      if (manifestData)
        manifestCached = 1;
    }
  }
  char *copy = manifestData ? _strdup(manifestData) : NULL;
  LeaveCriticalSection(&manifestLock);
  return copy;
}
static char *version_json(const char *id, char *err) {
  if (!id_ok(id)) {
    snprintf(err, 512, "Invalid version ID");
    return NULL;
  }
  char *m = load_manifest(0, err);
  if (!m)
    return NULL;
  Doc d;
  if (!doc_parse(&d, m, err)) {
    free(m);
    return NULL;
  }
  int a = jprop(&d, 0, "versions");
  char *data = NULL;
  for (int i = a + 1; a >= 0 && i < jskip(&d, a); i = jskip(&d, i))
    if (jeq(&d, jprop(&d, i, "id"), id)) {
      char *url = jstr(&d, jprop(&d, i, "url")),
           *hash = jstr(&d, jprop(&d, i, "sha1"));
      if (url && hash && strlen(hash) == 40)
        data = fetch_json(url, hash, 0, err);
      else
        snprintf(err, 512, "Version lacks verified metadata");
      free(url);
      free(hash);
      break;
    }
  if (!data && !*err)
    snprintf(err, 512, "Version not found in official manifest");
  doc_free(&d);
  free(m);
  return data;
}
// Do not reuse a target while its previous original task still owns it.
static int settle_native_task(void*task,DWORD budget){
 DWORD start=GetTickCount();unsigned waits=0;
 while(adapter_task_pending(task)){
  if((DWORD)(GetTickCount()-start)>=budget)return 0;
  Sleep(waits++<5?10:50);
 }return 1;
}
static int download_file_native(Item *item,const wchar_t*path,char *err){
 wait_group();if(stopped())return 0;
 const wchar_t *normal=!wcsncmp(path,L"\\\\?\\",4)?path+4:path;
 if(wcslen(normal)>=MAX_PATH){snprintf(err,512,"Target exceeds the legacy downloader's MAX_PATH limit; choose a shorter download directory in box settings");return 0;}
 wchar_t url[3000],name[512],id[128];if(!from_utf8(item->url,url,3000)||!from_utf8(job.id,id,128))return 0;
 const char *filename=strrchr(item->path,'/');filename=filename?filename+1:item->path;
 wchar_t file[300];if(!from_utf8(filename,file,300)){snprintf(err,512,"Filename too long for native task label");return 0;}
 swprintf(name,512,L"Minecraft %ls - %ls",id,file);
 for(int attempt=0;attempt<3&&!stopped();attempt++){
  void *task=NULL;long long credited=0;if(!adapter_submit(name,url,normal,&task,err))return 0;
  DWORD queuedAt=GetTickCount(),previousTick=queuedAt;TransferWatch watch={0};int done=0;status_message(item->path);
  while(!stopped()){
   unsigned long long current=0;int error=0;int state=adapter_status(task,&current,&error);
   if(current>(unsigned long long)item->size)current=(unsigned long long)item->size;
   EnterCriticalSection(&jobLock);job.doneBytes+=(int64_t)current-credited;adapter_group_progress(job.doneBytes);LeaveCriticalSection(&jobLock);credited=(long long)current;
   DWORD now=GetTickCount();int paused=adapter_group_paused();unsigned action=transfer_watch(&watch,current,now-previousTick,paused);previousTick=now;
   if(paused)queuedAt=now;
   if(state==2){done=1;break;}
   if(!paused&&((action&WATCH_VERIFY)||state<0)&&verified_file(path,item->hash,item->size)){
    done=1;char note[3800];snprintf(note,sizeof(note),"Official SHA-1/size verified before native completion notification: %s",item->path);log_event(note);break;
   }
   if(state<0){snprintf(err,512,"Box native download failed/cancelled (error %d): %s",error,item->path);break;}
   if(!paused&&state==0&&(DWORD)(now-queuedAt)>15000){snprintf(err,512,"Box did not process the native task: %s",item->path);break;}
   if(action&WATCH_RETRY){snprintf(err,512,"Native task stalled for 45s; retry %d/3: %s",attempt+1,item->path);break;}
   if(watch.idle>=10000&&!paused){char note[3800];snprintf(note,sizeof(note),"原下载模块暂无进度（%lu 秒），等待重试：%s",(unsigned long)(watch.idle/1000),item->path);status_message(note);}
   adapter_wait(task,500);
  }
  if(done&&!stopped()&&!settle_native_task(task,5000)){done=0;snprintf(err,512,"Native completion cleanup timed out: %s",item->path);}
  if(stopped()||!done){
   if(!stopped()){char detail[5000];snprintf(detail,sizeof(detail),"Native retry %d/3; file=%s; url=%s; expected=%lld; reason=%s",attempt+1,item->path,item->url,(long long)item->size,err);log_event(detail);}
   adapter_cancel(task);
   // The original task-thread marker must finish retirement before a retry
   // can write the same .part path. A timeout is surfaced, never overlapped.
   if(!settle_native_task(task,5000)){
    adapter_release(task);snprintf(err,512,"Native retry cleanup timed out; refusing an overlapping transfer: %s",item->path);
    EnterCriticalSection(&jobLock);job.doneBytes-=credited;adapter_group_progress(job.doneBytes);LeaveCriticalSection(&jobLock);return 0;
   }
  }
  adapter_release(task);
  if(done&&!stopped()&&verified_file(path,item->hash,item->size)){
   EnterCriticalSection(&jobLock);job.doneBytes+=item->size-credited;job.doneFiles++;adapter_group_progress(job.doneBytes);LeaveCriticalSection(&jobLock);return 1;
  }
  if(done&&!stopped())snprintf(err,512,"Native task completed but official SHA-1/size verification failed");
  EnterCriticalSection(&jobLock);job.doneBytes-=credited;adapter_group_progress(job.doneBytes);LeaveCriticalSection(&jobLock);
  if(!stopped()&&done)DeleteFileW(path);
 }
 return 0;
}
#include "cache_pipeline.h"
static int download_file(Item*item,char*err){
 char key[41],relative[96];wchar_t installed[MAXPATH],object[MAXPATH],part[MAXPATH];
 if(!cache_key(item->hash,key)){strcpy(err,"Official file has invalid SHA-1");return 0;}
 snprintf(relative,sizeof(relative),"objects/%.2s/%s",key,key);
 if(!game_path(item->path,installed,err)||!cache_path(relative,object,err)||wcslen(object)+6>=MAXPATH)return 0;
 wcscpy(part,object);wcscat(part,L".part");
 if(wcslen(part+4)>=MAX_PATH){strcpy(err,"Shared cache exceeds the native downloader path limit; choose a shorter box download directory");return 0;}
 wait_group();if(stopped())return 0;
 unsigned stripe=cache_stripe(key);EnterCriticalSection(cacheLocks+stripe);
 int ok=0,source=0;Item normalized=*item;normalized.hash=key;
 {char message[3600];snprintf(message,sizeof(message),"校验缓存并安装：%s",item->path);status_message(message);}
 if(!stopped()&&file_safe(part)&&ensure_dirs(object,err)&&ensure_dirs(installed,err))ok=cache_acquire(&normalized,installed,object,part,&source,err);
 if(ok){
  EnterCriticalSection(&jobLock);
  if(source!=CACHE_NATIVE_TRANSFER){job.doneFiles++;job.doneBytes+=item->size;job.reusedBytes+=item->size;}
  if(source==CACHE_EXISTING_INSTALL){job.localHits++;job.localBytes+=item->size;}
  else if(source==CACHE_VERIFIED_OBJECT||source==CACHE_VERIFIED_PART){job.cacheHits++;job.cacheBytes+=item->size;}
  adapter_group_progress(job.doneBytes);LeaveCriticalSection(&jobLock);
 }
 if(!ok&&!cancelJob){if(!*err)snprintf(err,512,"Cache/install processing failed (Windows error %lu)",(unsigned long)GetLastError());char detail[5000];snprintf(detail,sizeof(detail),"Cache/install failure; file=%s; reason=%s",item->path,err);log_event(detail);}
 LeaveCriticalSection(cacheLocks+stripe);return ok;
}
static DWORD WINAPI file_worker(void *unused) {
  (void)unused;
  for (;;) {
    LONG i = InterlockedIncrement(&nextFile) - 1;
    if (stopped() || i >= plannedFileCount)
      break;
    char err[512] = {0};
    if (!download_file(&jobPlan.files[i], err)) {
      if (!cancelJob)
        set_failure(err);
      break;
    }
  }
  return 0;
}
static unsigned u16(const unsigned char *b) {
  return (unsigned)b[0] | ((unsigned)b[1] << 8);
}
static uint32_t u32(const unsigned char *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
         ((uint32_t)b[3] << 24);
}
static int extract_native(Item *item, char *err) {
  wchar_t path[MAXPATH];
  if (!game_path(item->path, path, err))
    return 0;
  char *raw = NULL;
  size_t size = 0;
  if (!read_file(path, &raw, &size, 128 * 1024 * 1024)) {
    snprintf(err, 512, "Cannot read verified native JAR");
    return 0;
  }
  unsigned char *b = (unsigned char *)raw;
  int64_t end = -1;
  for (int64_t i = (int64_t)size - 22; i >= 0 && i >= (int64_t)size - 65557;
       i--)
    if (u32(b + i) == 0x06054b50) {
      end = i;
      break;
    }
  if (end < 0) {
    free(raw);
    snprintf(err, 512, "Native ZIP directory missing");
    return 0;
  }
  uint32_t cursor = u32(b + end + 16);
  unsigned count = u16(b + end + 10);
  int ok = 1;
  for (unsigned i = 0; i < count && ok && !stopped(); i++) {
    if ((uint64_t)cursor + 46 > size || u32(b + cursor) != 0x02014b50) {
      ok = 0;
      break;
    }
    unsigned flags = u16(b + cursor + 8), method = u16(b + cursor + 10),
             nameLength = u16(b + cursor + 28), extra = u16(b + cursor + 30),
             comment = u16(b + cursor + 32);
    uint32_t compressed = u32(b + cursor + 20), expanded = u32(b + cursor + 24),
             offset = u32(b + cursor + 42);
    if ((uint64_t)cursor + 46 + nameLength + extra + comment > size ||
        nameLength >= 3000) {
      ok = 0;
      break;
    }
    char name[3001];
    memcpy(name, b + cursor + 46, nameLength);
    name[nameLength] = 0;
    cursor += 46 + nameLength + extra + comment;
    if (memchr(name, 0, nameLength)) {
      ok = 0;
      break;
    }
    int skip = nameLength == 0 || name[nameLength - 1] == '/' ||
               !strncmp(name, "META-INF/", 9);
    for (int ex = 0; ex < item->excludes; ex++)
      if (!strncmp(name, item->exclude[ex], strlen(item->exclude[ex])))
        skip = 1;
    if (skip)
      continue;
    if (!relative_ok(name) || (flags & 1) || expanded > 128 * 1024 * 1024 ||
        (uint64_t)offset + 30 > size || u32(b + offset) != 0x04034b50) {
      ok = 0;
      break;
    }
    uint64_t data =
        (uint64_t)offset + 30 + u16(b + offset + 26) + u16(b + offset + 28);
    if (data + compressed > size) {
      ok = 0;
      break;
    }
    unsigned char *decoded = malloc(expanded ? expanded : 1);
    if (!decoded) {
      ok = 0;
      break;
    }
    if (method == 0) {
      if (compressed != expanded)
        ok = 0;
      else
        memcpy(decoded, b + data, expanded);
    } else if (method == 8) {
      if (tinfl_decompress_mem_to_mem(decoded, expanded, b + data, compressed,
                                      0) != expanded)
        ok = 0;
    } else
      ok = 0;
    char relative[3400];
    snprintf(relative, sizeof(relative), "versions/%s/natives/%s", job.id,
             name);
    if (ok)
      ok = game_path(relative, path, err) &&
           write_atomic(path, decoded, expanded, err);
    free(decoded);
  }
  free(raw);
  if (!ok && !*err)
    snprintf(err, 512, "Unsafe or unsupported native ZIP entry");
  return ok && !stopped();
}
static int copy_asset(const char *src, const char *dst, char *err) {
  wchar_t a[MAXPATH], b[MAXPATH], temp[MAXPATH];
  if (!game_path(src, a, err) || !game_path(dst, b, err) ||
      !ensure_dirs(b, err) || wcslen(b) + 6 >= MAXPATH)
    return 0;
  wcscpy(temp, b);
  wcscat(temp, L".tmp");
  if (!file_safe(temp))
    return 0;
  if (!CopyFileW(a, temp, FALSE) ||
      !MoveFileExW(temp, b,
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    errwin(err, "Resource mapping failed");
    DeleteFileW(temp);
    return 0;
  }
  return 1;
}
#include "first_language.h"
static DWORD WINAPI download_worker(void *unused) {
  (void)unused;
  char err[512] = {0};
  int ok=0,groupCreated=0,centerSynced=1;char *version=NULL;wchar_t root[MAXPATH],normal[MAXPATH],wid[128];
  registeredGame=0;
  if(!adapter_root(root,MAXPATH,err))goto finish;
  if(!from_utf8(job.id,wid,128)||wcslen(root)+wcslen(wid)+20>=MAXPATH){snprintf(err,512,"Invalid or overlong box installation path");goto finish;}
  swprintf(normal,MAXPATH,L"%ls\\game\\%ls",root,wid);
  if(!wcsncmp(normal,L"\\\\",2)){snprintf(err,512,"Use a local drive for this native adapter test");goto finish;}
  wcscpy(gameBaseDir,L"\\\\?\\");wcscat(gameBaseDir,normal);wcscat(gameBaseDir,L"\\.minecraft");
  swprintf(cacheRootDir,MAXPATH,L"\\\\?\\%ls\\resources\\official",root);
  swprintf(cacheVersionDir,MAXPATH,L"%ls\\versions\\%ls",cacheRootDir,wid);
  if(wcslen(cacheRootDir)+64>=MAX_PATH||wcslen(cacheVersionDir)+32>=MAXPATH){strcpy(err,"Configured box directory is too long for shared caching");goto finish;}
  char *dir=utf8(normal);EnterCriticalSection(&jobLock);if(dir)snprintf(job.directory,sizeof(job.directory),"%s",dir);LeaveCriticalSection(&jobLock);free(dir);
  version = version_json(job.id, err);

  ok = version &&
           plan_build(&jobPlan, job.id, version, systemBits, fetch_json, err);
  if (!ok) {
    if (!jobPlan.versionJson)
      free(version);
    goto finish;
  }
  EnterCriticalSection(&jobLock);
  plannedFileCount=clientProbe?1:jobPlan.count;
  job.totalFiles = plannedFileCount;
  job.totalBytes = clientProbe?jobPlan.files[0].size:jobPlan.bytes;
  snprintf(job.java, sizeof(job.java), "%s", jobPlan.java);
  strcpy(job.state, "downloading");
  LeaveCriticalSection(&jobLock);
  if (stopped())
    goto finish;
  if(!cache_index(0,err)){ok=0;goto finish;}
  {wchar_t groupName[200];swprintf(groupName,200,L"Minecraft %ls%ls",wid,clientProbe?L" (client test)":L"");if(!adapter_group_begin(groupName,cacheVersionDir+4,job.totalBytes,err)){ok=0;goto finish;}groupCreated=1;}
  HANDLE threads[16] = {0};
  int workers=clientProbe?1:fileConcurrency;if(workers>plannedFileCount)workers=plannedFileCount;
  int created = 0;
  for (int i = 0; i < workers; i++) {
    threads[i] = CreateThread(NULL, 0, file_worker, NULL, 0, NULL);
    if (threads[i])
      created++;
    else {
      errwin(err, "Cannot start file worker");
      set_failure(err);
      break;
    }
  }
  for (int i = 0; i < created; i++) {
    WaitForSingleObject(threads[i], INFINITE);
    CloseHandle(threads[i]);
  }
  if (stopped())
    goto finish;
  if(!cache_index(1,err)){ok=0;goto finish;}
  if(clientProbe)goto finish;
  wait_group();if(stopped())goto finish;
  status_message("Extracting native libraries and mapping assets");
  for (int i = 0; i < jobPlan.count && !stopped(); i++)
    if (jobPlan.files[i].native && !extract_native(&jobPlan.files[i], err)) {
      ok = 0;
      goto finish;
    }
  for (int i = 0; i < jobPlan.aliasCount && !stopped(); i++) {
    char src[200], dst[3400];
    Asset *a = &jobPlan.aliases[i];
    snprintf(src, sizeof(src), "assets/objects/%.2s/%s", a->hash, a->hash);
    if (jobPlan.virtualAssets) {
      snprintf(dst, sizeof(dst), "assets/virtual/%s/%s", jobPlan.indexId,
               a->name);
      if (!copy_asset(src, dst, err)) {
        ok = 0;
        goto finish;
      }
    }
    if (jobPlan.mapResources) {
      snprintf(dst, sizeof(dst), "resources/%s", a->name);
      if (!copy_asset(src, dst, err)) {
        ok = 0;
        goto finish;
      }
    }
  }
  if (stopped())
    goto finish;
  wchar_t out[MAXPATH];
  char relative[512];
  if (jobPlan.indexJson) {
    snprintf(relative, sizeof(relative), "assets/indexes/%s.json",
             jobPlan.indexId);
    if (!game_path(relative, out, err) ||
        !write_atomic(out, jobPlan.indexJson, strlen(jobPlan.indexJson), err)) {
      ok = 0;
      goto finish;
    }
  }
  snprintf(relative, sizeof(relative), "versions/%s/%s.json", job.id, job.id);
  if (!game_path(relative, out, err) ||
      !write_atomic(out, jobPlan.versionJson, strlen(jobPlan.versionJson),
                    err)) {
    ok = 0;
    goto finish;
  }
  {char languageError[512]={0};if(!initialize_language(&jobPlan,languageError)){log_event(languageError);EnterCriticalSection(&jobLock);snprintf(job.error,sizeof(job.error),"首次中文设置未完成：%s",languageError);LeaveCriticalSection(&jobLock);}}
  {wchar_t importPath[MAXPATH];char importError[512]={0};wcscpy(importPath,gameBaseDir+4);wchar_t *tail=wcsrchr(importPath,L'\\');if(tail)*tail=0;
   registeredGame=adapter_register(importPath,importError);
   if(!registeredGame){EnterCriticalSection(&jobLock);size_t used=strlen(job.error);snprintf(job.error+used,sizeof(job.error)-used,"%sFiles verified; native registration failed: %s",used?"; ":"",importError);LeaveCriticalSection(&jobLock);}
  }
  char *qid=json_quote(job.id),*qdir=json_quote(job.directory);char report[16000];
  snprintf(report,sizeof(report),"{\"version\":%s,\"boxImportDirectory\":%s,\"verifiedFiles\":%d,\"registeredInBox\":%s,\"downloadEngine\":\"CDownloadManager / taskmodulehttp.dll\",\"source\":\"%s\"}",qid?qid:"null",qdir?qdir:"null",jobPlan.count,registeredGame?"true":"false",MANIFEST);
  free(qid);free(qdir);
  if(!game_path("official-install-report.json",out,err)||!write_atomic(out,report,strlen(report),err)){ok=0;goto finish;}

finish:
  if(groupCreated){
   adapter_group_finish(cancelJob?5:(!ok||failedJob)?3:4);
   if(!(centerSynced=adapter_group_sync()))log_event("Download center terminal refresh timed out; projection is terminal, UI refresh remains queued");
  }
  EnterCriticalSection(&jobLock);
  if (cancelJob) {
    strcpy(job.state, "cancelled");
    strcpy(job.message, "已取消；再次下载会校验并复用已完成文件。");
  } else if (!ok || failedJob) {
    strcpy(job.state, "failed");
    if (*err)
      snprintf(job.error, sizeof(job.error), "%s", err);
    strcpy(job.message, "下载失败，已结束本次任务；可展开错误提示后重试，已校验缓存将复用。");
  } else {
    strcpy(job.state, "complete");
    strcpy(job.message, clientProbe?"原下载模块的单文件任务已完成并通过校验；此测试未安装整套游戏。":registeredGame?"文件安装完成，已更新为盒子下载版本登记。请在启动游戏列表核对。":"文件安装完成，但自动登记未成功；请查看提示并手动添加。");
  }
  if(!centerSynced){size_t used=strlen(job.error);snprintf(job.error+used,sizeof(job.error)-used,"%s下载中心界面刷新超时，请重新打开下载中心确认。",used?"; ":"");}
  LeaveCriticalSection(&jobLock);
  {char summary[1024];snprintf(summary,sizeof(summary),"Version=%s; state=%s; concurrency=%d; elapsedMs=%lu; verifiedBytes=%lld; reusedBytes=%lld; cacheHits=%d; cacheBytes=%lld; existingInstallHits=%d; existingInstallBytes=%lld; metadataHits=%d",job.id,job.state,clientProbe?1:fileConcurrency,(unsigned long)(GetTickCount()-job.startedAt),(long long)job.doneBytes,(long long)job.reusedBytes,job.cacheHits,(long long)job.cacheBytes,job.localHits,(long long)job.localBytes,job.metadataHits);log_event(summary);}
  {char info[512];native_network_report(info,sizeof(info));log_event(info);}
  plan_free(&jobPlan);
  InterlockedExchange(&busy, 0);
  return 0;
}
static void send_all(SOCKET s, const char *b, size_t n) {
  while (n) {
    int sent = send(s, b, (int)(n > 32768 ? 32768 : n), 0);
    if (sent <= 0)
      break;
    b += sent;
    n -= (size_t)sent;
  }
}
static void response(SOCKET s, int code, const char *type, const char *body) {
  char head[1200];
  size_t n = strlen(body);
  snprintf(head, sizeof(head),
           "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: "
           "%u\r\nCache-Control: no-store\r\nX-Content-Type-Options: "
           "nosniff\r\nX-Frame-Options: DENY\r\nContent-Security-Policy: "
           "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src "
           "'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors "
           "'none'\r\nConnection: close\r\n\r\n",
           code, code == 200 ? "OK" : "Error", type, (unsigned)n);
  send_all(s, head, strlen(head));
  send_all(s, body, n);
}
static void response_error(SOCKET s, int code, const char *err) {
  char *q = json_quote(err);
  char body[4000];
  snprintf(body, sizeof(body), "{\"error\":%s}", q ? q : "\"Internal error\"");
  free(q);
  response(s, code, "application/json; charset=utf-8", body);
}
static char *replace_page_token(const unsigned char *html) {
  const char *at = strstr((const char *)html, "__MCPC_TOKEN__");
  if (!at)
    return NULL;
  size_t before = (size_t)(at - (const char *)html), after = strlen(at + 14);
  char *p = malloc(before + strlen(token) + after + 1);
  if (!p)
    return NULL;
  memcpy(p, html, before);
  strcpy(p + before, token);
  strcpy(p + before + strlen(token), at + 14);
  return p;
}
static int decode_id(const char *query, char out[128]) {
  if (strncmp(query, "id=", 3))
    return 0;
  query += 3;
  int n = 0;
  while (*query && *query!='&') {
    unsigned char c = (unsigned char)*query++;
    if (c == '%') {
      char temp[3];
      if (!query[0] || !query[1])
        return 0;
      temp[0] = query[0];
      temp[1] = query[1];
      temp[2] = 0;
      char *end;
      long v = strtol(temp, &end, 16);
      if (*end)
        return 0;
      c = (unsigned char)v;
      query += 2;
    }
    if (!c || n >= 127)
      return 0;
    out[n++] = (char)c;
  }
  out[n] = 0;
  return id_ok(out);
}
static char *status_json(void) {
  EnterCriticalSection(&jobLock);
  char *id = json_quote(job.id),
       *state = json_quote(*job.state ? job.state : "idle"),
       *msg = json_quote(job.message), *error = json_quote(job.error),
       *java = json_quote(job.java), *directory = json_quote(job.directory);
  size_t n = 16000 + strlen(directory ? directory : "");
  char *p = malloc(n);
  if (p)
    snprintf(p, n,
             "{\"id\":%s,\"state\":%s,\"message\":%s,\"error\":%s,\"java\":%s,"
             "\"directory\":%s,\"totalFiles\":%d,\"doneFiles\":%d,"
             "\"totalBytes\":%I64d,\"doneBytes\":%I64d,\"parallel\":%d,\"cacheHits\":%d,\"localHits\":%d,\"reusedBytes\":%I64d,\"cacheBytes\":%I64d,\"metadataHits\":%d,\"taskId\":%lu}",
             id ? id : "null", state ? state : "null", msg ? msg : "null",
             error ? error : "null", java ? java : "null",
             directory ? directory : "null", job.totalFiles, job.doneFiles,
             job.totalBytes, job.doneBytes,clientProbe?1:fileConcurrency,job.cacheHits,job.localHits,job.reusedBytes,job.cacheBytes,job.metadataHits,(unsigned long)job.taskId);
  free(id);
  free(state);
  free(msg);
  free(error);
  free(java);
  free(directory);
  LeaveCriticalSection(&jobLock);
  return p;
}
#include "history_api.inc"
static DWORD WINAPI client_worker(void *arg) {
  SOCKET s = (SOCKET)(uintptr_t)arg;
  DWORD timeout = 10000;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
  char request[16384], method[16], route[8192],
      host[128] = {0}, origin[256] = {0}, session[128] = {0};
  int used = 0, valid = 0;
  while (used < (int)sizeof(request) - 1) {
    int got = recv(s, request + used, (int)sizeof(request) - 1 - used, 0);
    if (got <= 0)
      goto done;
    used += got;
    request[used] = 0;
    if (strstr(request, "\r\n\r\n")) {
      valid = 1;
      break;
    }
  }
  if (!valid)
    goto done;
  if (sscanf(request, "%15s %8191s", method, route) != 2)
    goto done;
  char *line = strstr(request, "\r\n");
  if (!line)
    goto done;
  line += 2;
  while (*line) {
    char *end = strstr(line, "\r\n");
    if (!end)
      break;
    *end = 0;
    if (!*line)
      break;
    char *colon = strchr(line, ':');
    if (colon) {
      *colon = 0;
      char *value = colon + 1;
      while (*value == ' ' || *value == '\t')
        value++;
      if (!_stricmp(line, "Host"))
        snprintf(host, sizeof(host), "%s", value);
      else if (!_stricmp(line, "Origin"))
        snprintf(origin, sizeof(origin), "%s", value);
      else if (!_stricmp(line, "X-MCPC-Token"))
        snprintf(session, sizeof(session), "%s", value);
    }
    line = end + 2;
  }
  if (strcmp(host, "127.0.0.1:19481") ||
      (*origin && strcmp(origin, "http://127.0.0.1:19481"))) {
    response_error(s, 403, "Host or origin rejected");
    goto done;
  }
  char *query = strchr(route, '?');
  if (query)
    *query++ = 0;
  else
    query = "";
  if (!strcmp(method, "GET") &&
      (!strcmp(route, "/") || !strcmp(route, "/index.html") || !strcmp(route,"/completed.html"))) {
    char *page = replace_page_token(!strcmp(route,"/completed.html")?HISTORY_HTML:UI_HTML);
    if (page) {
      response(s, 200, "text/html; charset=utf-8", page);
      free(page);
    }
    goto done;
  }
  if (strncmp(route, "/api/", 5) || strcmp(session, token)) {
    response_error(s, 403, "Invalid local session");
    goto done;
  }
  if(!strcmp(method,"GET")&&!strcmp(route,"/api/history")){
    char err[512]={0},*body=history_list_json(err);if(!body)response_error(s,500,err);else{response(s,200,"application/json; charset=utf-8",body);free(body);}goto done;
  }
  if(!strcmp(method,"GET")&&!strcmp(route,"/api/history/revision")){
    char body[80];snprintf(body,sizeof(body),"{\"revision\":%lu}",adapter_history_revision());response(s,200,"application/json; charset=utf-8",body);goto done;
  }
  if(!strcmp(method,"POST")&&(!strcmp(route,"/api/history/remove")||!strcmp(route,"/api/history/delete-files")||!strcmp(route,"/api/history/open"))){
    HistoryKey keys[100];int count=history_keys(query,keys);if(!count||(!strcmp(route,"/api/history/open")&&count!=1)){response_error(s,400,"Invalid completed record selection (maximum 100)");goto done;}
    if(!strcmp(route,"/api/history/remove")||!strcmp(route,"/api/history/delete-files"))history_remove_response(s,keys,count,!strcmp(route,"/api/history/delete-files"));
    else{char err[512]={0};if(history_open(keys[0],err))response(s,200,"application/json; charset=utf-8","{\"opened\":true}");else response_error(s,500,err);}goto done;
  }
  if (!strcmp(method, "GET") && !strcmp(route, "/api/status")) {
    char *body = status_json();
    if (body) {
      response(s, 200, "application/json; charset=utf-8", body);
      free(body);
    }
    goto done;
  }
  if (!strcmp(method, "GET") && !strcmp(route, "/api/versions")) {
    char err[512] = {0}, *m = load_manifest(!strcmp(query, "refresh=1"), err);
    if (!m) {
      response_error(s, 500, err);
      goto done;
    }
    size_t n = strlen(m);
    while (n && ((unsigned char)m[n - 1] <= 32))
      n--;
    if (!n || m[n - 1] != '}') {
      free(m);
      response_error(s, 500, "Invalid manifest");
      goto done;
    }
    m[n - 1] = 0;
    char *body = malloc(n + 600);
    if (body) {
      snprintf(body, n + 600,
               "%s,\"cached\":%s,\"warning\":\"%s\",\"source\":\"%s\","
               "\"architecture\":\"%d\",\"windows\":\"native\"}",
               m, manifestCached ? "true" : "false",
               manifestCached ? "官方请求失败，正在显示本地缓存。" : "",
               MANIFEST, systemBits);
      response(s, 200, "application/json; charset=utf-8", body);
      free(body);
    }
    free(m);
    goto done;
  }
  if (!strcmp(method, "POST") && !strcmp(route, "/api/cancel")) {
    InterlockedExchange(&cancelJob, 1);
    response(s, 200, "application/json; charset=utf-8", "{\"accepted\":true}");
    goto done;
  }
  if (!strcmp(method, "POST") && (!strcmp(route, "/api/download")||!strcmp(route,"/api/test-client"))) {
    char id[128];
    if (!decode_id(query, id)) {
      response_error(s, 400, "Invalid version ID");
      goto done;
    }
    if (InterlockedCompareExchange(&busy, 1, 0)) {
      response_error(s, 409, "已有下载任务，请等待或取消。");
      goto done;
    }
    InterlockedExchange(&cancelJob, 0);
    InterlockedExchange(&failedJob, 0);
    InterlockedExchange(&nextFile, 0);
    EnterCriticalSection(&jobLock);
    memset(&job, 0, sizeof(job));
    clientProbe=!strcmp(route,"/api/test-client");
    fileConcurrency=download_parallel(query);
    job.startedAt=GetTickCount();job.taskId=++taskSerial;strcpy(job.id, id);
    strcpy(job.state, "preparing");
    strcpy(job.message, "读取官方版本元数据…");
    LeaveCriticalSection(&jobLock);
    HANDLE t = CreateThread(NULL, 0, download_worker, NULL, 0, NULL);
    if (!t) {
      EnterCriticalSection(&jobLock);strcpy(job.state,"failed");strcpy(job.error,"Cannot start download worker");strcpy(job.message,"下载任务未能启动，请重试。");LeaveCriticalSection(&jobLock);
      InterlockedExchange(&busy, 0);
      response_error(s, 500, "Cannot start download worker");
    } else {
      CloseHandle(t);
      response(s, 200, "application/json; charset=utf-8",
               "{\"accepted\":true}");
    }
    goto done;
  }
  response_error(s, 404, "Not found");
done:
  shutdown(s, SD_BOTH);
  closesocket(s);
  return 0;
}
static DWORD WINAPI native_server(void *unused) {
  (void)unused;
  native_network_init();
  {char info[512];native_network_report(info,sizeof(info));log_event(info);}
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
    log_event("Winsock startup failed");
    return 0;
  }
  SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) {
    WSACleanup();
    return 0;
  }
  BOOL exclusive = TRUE;
  setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&exclusive,
             sizeof(exclusive));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ||
      listen(listener, 12) == SOCKET_ERROR) {
    log_event("Cannot bind local port 19481; another instance may be using it");
    closesocket(listener);
    WSACleanup();
    return 0;
  }
  httpSession = WinHttpOpen(L"MCPCBox-NativeAdapter/0.7",
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!httpSession) {
    log_event("WinHTTP session startup failed");
    closesocket(listener);
    WSACleanup();
    return 0;
  }
  DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
  if (!WinHttpSetOption(httpSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
                        &protocols, sizeof(protocols)))
    log_event(
        "TLS 1.2 option failed; check Windows updates and Schannel settings");
  WinHttpSetTimeouts(httpSession, 15000, 15000, 25000, 25000);
  log_event("Native download module ready in MCPCBox.exe; loopback port 19481");
  for (;;) {
    SOCKET s = accept(listener, NULL, NULL);
    if (s == INVALID_SOCKET)
      break;
    HANDLE t =
        CreateThread(NULL, 0, client_worker, (void *)(uintptr_t)s, 0, NULL);
    if (t)
      CloseHandle(t);
    else
      closesocket(s);
  }
  WinHttpCloseHandle(httpSession);
  closesocket(listener);
  WSACleanup();
  return 0;
}
extern void _pei386_runtime_relocator(void);
__attribute__((used)) void StartNative(void) {
  _pei386_runtime_relocator();
  original_entry = (char *)GetModuleHandleW(NULL) + 0x2c5578;
  adapter_init();
  InitializeCriticalSection(&jobLock);
  InitializeCriticalSection(&manifestLock);
  InitializeCriticalSection(&logLock);
  for(int i=0;i<64;i++)InitializeCriticalSection(cacheLocks+i);
  wchar_t module[MAXPATH];
  DWORD n = GetModuleFileNameW(NULL, module, MAXPATH);
  if (!n || n >= MAXPATH)
    return;
  wchar_t *slash = wcsrchr(module, '\\');
  if (!slash)
    return;
  *slash = 0;
  if (wcslen(module) + 10 >= MAXPATH)
    return;
  if (!wcsncmp(module, L"\\\\", 2)) {
    wcscpy(baseDir, L"\\\\?\\UNC\\");
    wcscat(baseDir, module + 2);
  } else {
    wcscpy(baseDir, L"\\\\?\\");
    wcscat(baseDir, module);
  }
  BOOL wow = FALSE;
  IsWow64Process(GetCurrentProcess(), &wow);
  systemBits = wow ? 64 : 32;
  HCRYPTPROV provider = 0;
  BYTE bytes[32];
  if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
                            CRYPT_VERIFYCONTEXT))
    return;
  int ok = CryptGenRandom(provider, sizeof(bytes), bytes);
  CryptReleaseContext(provider, 0);
  if (!ok)
    return;
  {DWORD seed;memcpy(&seed,bytes,sizeof(seed));adapter_seed(seed);}
  for (int i = 0; i < 32; i++)
    sprintf(token + i * 2, "%02x", bytes[i]);
  token[64] = 0;
  HANDLE t = CreateThread(NULL, 0, native_server, NULL, 0, NULL);
  if (t)
    CloseHandle(t);
}
__attribute__((naked, used)) void NativeEntry(void) {
  __asm__ __volatile__(
      "pushfl\n\tpushal\n\tcall _StartNative\n\tpopal\n\tpopfl\n\tjmp "
      "*_original_entry\n\t");
}
