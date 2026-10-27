#ifndef MCPC_CORE_H
#define MCPC_CORE_H
#include <stddef.h>
#include <stdint.h>
#define JSMN_HEADER
#include "third_party/jsmn.h"
typedef struct {
  char *s;
  jsmntok_t *t;
  int n;
} Doc;
typedef struct {
  char *url, *path, *hash;
  int64_t size;
  int native;
  char **exclude;
  int excludes;
} Item;
typedef struct {
  char *name, *hash;
} Asset;
typedef struct {
  Item *files;
  int count, capacity;
  Asset *aliases;
  int aliasCount;
  int64_t bytes;
  char *id, *java, *indexId, *indexJson, *versionJson;
  int virtualAssets, mapResources;
} Plan;
typedef char *(*FetchJson)(const char *, const char *, int64_t, char *);
int doc_parse(Doc *, char *, char *);
void doc_free(Doc *);
int jskip(Doc *, int);
int jprop(Doc *, int, const char *);
int jeq(Doc *, int, const char *);
char *jstr(Doc *, int);
int64_t jnum(Doc *, int);
int jbool(Doc *, int);
int relative_ok(const char *);
int id_ok(const char *);
int official_url(const char *);
char *json_quote(const char *);
int plan_build(Plan *, const char *, char *, int, FetchJson, char *);
void plan_free(Plan *);
#endif
