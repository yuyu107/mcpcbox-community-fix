#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define JSMN_STATIC
#include "third_party/jsmn.h"
#include "core.h"
static char *dupstr(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (p)
    memcpy(p, s, n);
  return p;
}
static int fail(char *err, const char *message) {
  snprintf(err, 512, "%s", message);
  return 0;
}
int doc_parse(Doc *d, char *s, char *err) {
  int cap = 2048, n;
  jsmn_parser parser;
  memset(d, 0, sizeof(*d));
  d->s = s;
  for (;;) {
    d->t = malloc((size_t)cap * sizeof(jsmntok_t));
    if (!d->t)
      return fail(err, "JSON allocation failed");
    jsmn_init(&parser);
    n = jsmn_parse(&parser, s, strlen(s), d->t, cap);
    if (n == JSMN_ERROR_NOMEM) {
      free(d->t);
      d->t = NULL;
      cap *= 2;
      if (cap > 2000000)
        return fail(err, "JSON token limit exceeded");
      continue;
    }
    break;
  }
  if (n < 1 || d->t[0].type != JSMN_OBJECT) {
    free(d->t);
    d->t = NULL;
    return fail(err, "Invalid JSON object");
  }
  d->n = n;
  return 1;
}
void doc_free(Doc *d) {
  free(d->t);
  d->t = NULL;
}
int jskip(Doc *d, int i) {
  if (i < 0 || i >= d->n)
    return d->n;
  int end = d->t[i].end;
  i++;
  while (i < d->n && d->t[i].start < end)
    i++;
  return i;
}
int jeq(Doc *d, int i, const char *s) {
  if (i < 0 || i >= d->n)
    return 0;
  return d->t[i].end - d->t[i].start == (int)strlen(s) &&
         !memcmp(d->s + d->t[i].start, s, strlen(s));
}
int jprop(Doc *d, int object, const char *key) {
  if (object < 0 || object >= d->n || d->t[object].type != JSMN_OBJECT)
    return -1;
  int stop = jskip(d, object);
  for (int i = object + 1; i < stop;) {
    int value = i + 1;
    if (value >= stop)
      return -1;
    if (jeq(d, i, key))
      return value;
    i = jskip(d, value);
  }
  return -1;
}
static int hexchar(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
char *jstr(Doc *d, int i) {
  if (i < 0 || i >= d->n)
    return dupstr("");
  int start = d->t[i].start, end = d->t[i].end;
  char *out = malloc((size_t)(end - start) * 3 + 1);
  if (!out)
    return NULL;
  int at = 0;
  for (int k = start; k < end; k++) {
    unsigned char c = (unsigned char)d->s[k];
    if (c == '\\' && k + 1 < end) {
      c = (unsigned char)d->s[++k];
      if (c == 'n')
        c = '\n';
      else if (c == 'r')
        c = '\r';
      else if (c == 't')
        c = '\t';
      else if (c == 'b')
        c = '\b';
      else if (c == 'f')
        c = '\f';
      else if (c == 'u' && k + 4 < end) {
        unsigned cp = 0;
        for (int h = 0; h < 4; h++) {
          int x = hexchar(d->s[++k]);
          if (x < 0) {
            free(out);
            return NULL;
          }
          cp = (cp << 4) | (unsigned)x;
        }
        if (cp >= 0xd800 && cp <= 0xdbff && k + 6 < end &&
            d->s[k + 1] == '\\' && d->s[k + 2] == 'u') {
          unsigned low = 0;
          k += 2;
          for (int h = 0; h < 4; h++) {
            int x = hexchar(d->s[++k]);
            if (x < 0) {
              free(out);
              return NULL;
            }
            low = (low << 4) | (unsigned)x;
          }
          if (low < 0xdc00 || low > 0xdfff) {
            free(out);
            return NULL;
          }
          cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
        }
        if (cp == 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
          free(out);
          return NULL;
        }
        if (cp < 128)
          out[at++] = (char)cp;
        else if (cp < 2048) {
          out[at++] = (char)(0xc0 | (cp >> 6));
          out[at++] = (char)(0x80 | (cp & 63));
        } else if (cp < 65536) {
          out[at++] = (char)(0xe0 | (cp >> 12));
          out[at++] = (char)(0x80 | ((cp >> 6) & 63));
          out[at++] = (char)(0x80 | (cp & 63));
        } else {
          out[at++] = (char)(0xf0 | (cp >> 18));
          out[at++] = (char)(0x80 | ((cp >> 12) & 63));
          out[at++] = (char)(0x80 | ((cp >> 6) & 63));
          out[at++] = (char)(0x80 | (cp & 63));
        }
        continue;
      }
    }
    if (c == 0) {
      free(out);
      return NULL;
    }
    out[at++] = (char)c;
  }
  out[at] = 0;
  return out;
}
int64_t jnum(Doc *d, int i) {
  if (i < 0 || i >= d->n)
    return 0;
  char tmp[40];
  int n = d->t[i].end - d->t[i].start;
  if (n < 1 || n > 30)
    return -1;
  memcpy(tmp, d->s + d->t[i].start, n);
  tmp[n] = 0;
  return (int64_t)strtoll(tmp, NULL, 10);
}
int jbool(Doc *d, int i) { return jeq(d, i, "true"); }
int relative_ok(const char *s) {
  if (!s || !*s || strlen(s) > 3000 || *s == '/' || strchr(s, '\\') ||
      strpbrk(s, ":*?\"<>|"))
    return 0;
  const char *start = s;
  for (const char *p = s;; p++) {
    unsigned char c = (unsigned char)*p;
    if (c && c < 32)
      return 0;
    if (!c || c == '/') {
      size_t n = (size_t)(p - start);
      if (!n || (n == 1 && start[0] == '.') ||
          (n == 2 && !memcmp(start, "..", 2)) || start[n - 1] == '.' ||
          start[n - 1] == ' ')
        return 0;
      if (!c)
        break;
      start = p + 1;
    }
  }
  return 1;
}
int id_ok(const char *s) {
  return s && strlen(s) <= 120 && relative_ok(s) && !strchr(s, '/');
}
int official_url(const char *u) {
  if (!u || strncmp(u, "https://", 8))
    return 0;
  u += 8;
  const char *end = strchr(u, '/');
  if (!end)
    return 0;
  const char *hosts[] = {
      "piston-meta.mojang.com",  "launchermeta.mojang.com",
      "piston-data.mojang.com",  "launcher.mojang.com",
      "libraries.minecraft.net", "resources.download.minecraft.net",
      "client.mojang.com",       NULL};
  for (int i = 0; hosts[i]; i++)
    if (strlen(hosts[i]) == (size_t)(end - u) &&
        !memcmp(u, hosts[i], (size_t)(end - u)))
      return 1;
  return 0;
}
static int hash_ok(const char *s) {
  if (!s || strlen(s) != 40)
    return 0;
  for (int i = 0; i < 40; i++)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
      return 0;
  return 1;
}
char *json_quote(const char *s) {
  if (!s)
    s = "";
  size_t n = strlen(s);
  char *out = malloc(n * 6 + 3);
  if (!out)
    return NULL;
  size_t j = 0;
  out[j++] = '"';
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"' || c == '\\') {
      out[j++] = '\\';
      out[j++] = (char)c;
    } else if (c < 32) {
      snprintf(out + j, 7, "\\u%04x", c);
      j += 6;
    } else
      out[j++] = (char)c;
  }
  out[j++] = '"';
  out[j] = 0;
  return out;
}
static int allowed(Doc *d, int lib, int bits, char *err) {
  int rules = jprop(d, lib, "rules");
  if (rules < 0)
    return 1;
  int result = 0;
  for (int i = rules + 1; i < jskip(d, rules); i = jskip(d, i)) {
    int match = 1, os = jprop(d, i, "os");
    int name = jprop(d, os, "name"), arch = jprop(d, os, "arch"),
        ver = jprop(d, os, "version");
    if (name >= 0 && !jeq(d, name, "windows"))
      match = 0;
    if (arch >= 0 && !jeq(d, arch, bits == 64 ? "x86_64" : "x86"))
      match = 0;
    if (match && ver >= 0) {
      fail(err, "Unsupported Windows version rule in metadata");
      return -1;
    }
    int features = jprop(d, i, "features");
    if (features >= 0)
      for (int k = features + 1; k < jskip(d, features); k = jskip(d, k + 1))
        if (!jeq(d, k + 1, "false"))
          match = 0;
    if (match)
      result = jeq(d, jprop(d, i, "action"), "allow");
  }
  return result;
}
static int add_file(Plan *p, char *url, char *path, char *hash, int64_t size,
                    char *err) {
  if (!url || !path || !hash || !official_url(url) || !relative_ok(path) ||
      !hash_ok(hash) || size < 0) {
    free(url);
    free(path);
    free(hash);
    return fail(err, "Invalid official file metadata or path");
  }
  for (int i = 0; i < p->count; i++)
    if (!strcmp(p->files[i].path, path)) {
      int ok = !strcmp(p->files[i].hash, hash);
      free(url);
      free(path);
      free(hash);
      return ok ? 1 : fail(err, "Conflicting file hashes for one path");
    }
  if (p->count == p->capacity) {
    int cap = p->capacity ? p->capacity * 2 : 128;
    Item *v = realloc(p->files, (size_t)cap * sizeof(Item));
    if (!v) {
      free(url);
      free(path);
      free(hash);
      return fail(err, "File plan allocation failed");
    }
    p->files = v;
    p->capacity = cap;
  }
  Item *f = &p->files[p->count++];
  memset(f, 0, sizeof(*f));
  f->url = url;
  f->path = path;
  f->hash = hash;
  f->size = size;
  p->bytes += size;
  return 1;
}
static char *prefix(const char *base, const char *s) {
  size_t n = strlen(base) + strlen(s) + 1;
  char *p = malloc(n);
  if (p) {
    strcpy(p, base);
    strcat(p, s);
  }
  return p;
}
static int add_json(Plan *p, Doc *d, int o, const char *path, char *err) {
  if (o < 0)
    return fail(err, "Missing official file metadata");
  char *url = jstr(d, jprop(d, o, "url"));
  if (url && !strncmp(url, "http://", 7)) {
    char *newurl = malloc(strlen(url) + 2);
    if (newurl) {
      strcpy(newurl, "https://");
      strcat(newurl, url + 7);
    }
    free(url);
    url = newurl;
  }
  return add_file(p, url, dupstr(path), jstr(d, jprop(d, o, "sha1")),
                  jnum(d, jprop(d, o, "size")), err);
}
static int add_native(Plan *p, Doc *d, int lib, const char *path, char *err) {
  Item *f = NULL;
  for (int i = 0; i < p->count; i++)
    if (!strcmp(path, p->files[i].path)) {
      f = &p->files[i];
      break;
    }
  if (!f)
    return fail(err, "Native file not in download plan");
  f->native = 1;
  int ex = jprop(d, jprop(d, lib, "extract"), "exclude");
  if (ex >= 0)
    for (int k = ex + 1; k < jskip(d, ex); k = jskip(d, k)) {
      char **v =
          realloc(f->exclude, (size_t)(f->excludes + 1) * sizeof(char *));
      if (!v)
        return fail(err, "Native exclusion allocation failed");
      f->exclude = v;
      f->exclude[f->excludes++] = jstr(d, k);
    }
  return 1;
}
int plan_build(Plan *p, const char *id, char *version, int bits,
               FetchJson fetch, char *err) {
  Doc d = {0}, ad = {0};
  memset(p, 0, sizeof(*p));
  p->id = dupstr(id);
  p->versionJson = version;
  p->java = dupstr("");
  if (!id_ok(id) || !doc_parse(&d, version, err))
    return 0;
  if (!jeq(&d, jprop(&d, 0, "id"), id) || jprop(&d, 0, "inheritsFrom") >= 0) {
    doc_free(&d);
    return fail(err, "Unexpected or inherited version metadata");
  }
  free(p->java);
  p->java = jstr(&d, jprop(&d, jprop(&d, 0, "javaVersion"), "majorVersion"));
  char path[3200];
  snprintf(path, sizeof(path), "versions/%s/%s.jar", id, id);
  if (!add_json(p, &d, jprop(&d, jprop(&d, 0, "downloads"), "client"), path,
                err))
    goto failed;
  int libs = jprop(&d, 0, "libraries");
  for (int lib = libs + 1; libs >= 0 && lib < jskip(&d, libs);
       lib = jskip(&d, lib)) {
    int use = allowed(&d, lib, bits, err);
    if (use < 0)
      goto failed;
    if (!use)
      continue;
    int ld = jprop(&d, lib, "downloads");
    if (ld < 0) {
      fail(err, "Library lacks verified download metadata");
      goto failed;
    }
    int artifact = jprop(&d, ld, "artifact");
    if (artifact >= 0) {
      char *rel = jstr(&d, jprop(&d, artifact, "path")),
           *full = prefix("libraries/", rel);
      free(rel);
      int ok = full && add_json(p, &d, artifact, full, err);
      char *name = jstr(&d, jprop(&d, lib, "name"));
      const char *classifier = name ? strrchr(name, ':') : NULL;
      if (ok && classifier &&
          ((bits == 64 && !strcmp(classifier, ":natives-windows")) ||
           (bits == 32 && !strcmp(classifier, ":natives-windows-x86"))))
        ok = add_native(p, &d, lib, full, err);
      free(name);
      free(full);
      if (!ok)
        goto failed;
    }
    int native = jprop(&d, jprop(&d, lib, "natives"), "windows");
    if (native >= 0) {
      char *classifier = jstr(&d, native);
      char key[200];
      char *arch = classifier ? strstr(classifier, "${arch}") : NULL;
      if (arch) {
        *arch = 0;
        snprintf(key, sizeof(key), "%s%s%s", classifier,
                 bits == 64 ? "64" : "32", arch + 7);
      } else
        snprintf(key, sizeof(key), "%s", classifier ? classifier : "");
      free(classifier);
      int file = jprop(&d, jprop(&d, ld, "classifiers"), key);
      char *rel = jstr(&d, jprop(&d, file, "path")),
           *full = prefix("libraries/", rel);
      free(rel);
      int ok = full && add_json(p, &d, file, full, err) &&
               add_native(p, &d, lib, full, err);
      free(full);
      if (!ok)
        goto failed;
    }
  }
  int log = jprop(&d, jprop(&d, jprop(&d, 0, "logging"), "client"), "file");
  if (log >= 0) {
    char *name = jstr(&d, jprop(&d, log, "id")),
         *full = prefix("assets/log_configs/", name);
    free(name);
    int ok = full && add_json(p, &d, log, full, err);
    free(full);
    if (!ok)
      goto failed;
  }
  int index = jprop(&d, 0, "assetIndex");
  if (index >= 0) {
    p->indexId = jstr(&d, jprop(&d, index, "id"));
    char *url = jstr(&d, jprop(&d, index, "url")),
         *hash = jstr(&d, jprop(&d, index, "sha1"));
    if (!id_ok(p->indexId) || !official_url(url) || !hash_ok(hash)) {
      free(url);
      free(hash);
      fail(err, "Invalid asset index metadata");
      goto failed;
    }
    p->indexJson = fetch(url, hash, jnum(&d, jprop(&d, index, "size")), err);
    free(url);
    free(hash);
    if (!p->indexJson || !doc_parse(&ad, p->indexJson, err))
      goto failed;
    p->virtualAssets = jbool(&ad, jprop(&ad, 0, "virtual"));
    p->mapResources = jbool(&ad, jprop(&ad, 0, "map_to_resources"));
    int objects = jprop(&ad, 0, "objects");
    if (objects < 0) {
      fail(err, "Missing asset objects");
      goto failed;
    }
    for (int i = objects + 1; i < jskip(&ad, objects); i = jskip(&ad, i + 1)) {
      char *hashValue = jstr(&ad, jprop(&ad, i + 1, "hash"));
      if (!hash_ok(hashValue)) {
        free(hashValue);
        fail(err, "Invalid asset hash");
        goto failed;
      }
      char urlValue[180];
      snprintf(urlValue, sizeof(urlValue),
               "https://resources.download.minecraft.net/%.2s/%s", hashValue,
               hashValue);
      snprintf(path, sizeof(path), "assets/objects/%.2s/%s", hashValue,
               hashValue);
      if (!add_file(p, dupstr(urlValue), dupstr(path), hashValue,
                    jnum(&ad, jprop(&ad, i + 1, "size")), err))
        goto failed;
      if (p->virtualAssets || p->mapResources) {
        char *name = jstr(&ad, i),
             *hashCopy = jstr(&ad, jprop(&ad, i + 1, "hash"));
        if (!relative_ok(name)) {
          free(name);
          free(hashCopy);
          fail(err, "Invalid resource mapping path");
          goto failed;
        }
        Asset *a =
            realloc(p->aliases, (size_t)(p->aliasCount + 1) * sizeof(Asset));
        if (!a) {
          free(name);
          free(hashCopy);
          fail(err, "Asset mapping allocation failed");
          goto failed;
        }
        p->aliases = a;
        a[p->aliasCount].name = name;
        a[p->aliasCount++].hash = hashCopy;
      }
    }
    doc_free(&ad);
  }
  doc_free(&d);
  return 1;
failed:
  doc_free(&ad);
  doc_free(&d);
  return 0;
}
void plan_free(Plan *p) {
  for (int i = 0; i < p->count; i++) {
    free(p->files[i].url);
    free(p->files[i].path);
    free(p->files[i].hash);
    for (int k = 0; k < p->files[i].excludes; k++)
      free(p->files[i].exclude[k]);
    free(p->files[i].exclude);
  }
  for (int i = 0; i < p->aliasCount; i++) {
    free(p->aliases[i].name);
    free(p->aliases[i].hash);
  }
  free(p->files);
  free(p->aliases);
  free(p->id);
  free(p->java);
  free(p->indexId);
  free(p->indexJson);
  free(p->versionJson);
  memset(p, 0, sizeof(*p));
}
