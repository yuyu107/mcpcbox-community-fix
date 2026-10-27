#include "../core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char *readall(const char *path) {
  FILE *f = fopen(path, "rb");
  assert(f);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  char *s = malloc(n + 1);
  assert(s);
  assert(fread(s, 1, n, f) == (size_t)n);
  s[n] = 0;
  fclose(f);
  return s;
}
static char *fixture(const char *url, const char *hash, int64_t size,
                     char *err) {
  (void)url;
  (void)err;
  char path[400];
  if (!strcmp(hash, "f6ad102bcaa53b1a58358f16e376d548d44933ec"))
    strcpy(path, "native-v19/tests/official-assets-1.8.json");
  else
    snprintf(path, sizeof(path), "native-v19/tests/assets-%s.json", hash);
  char *p = readall(path);
  assert((int64_t)strlen(p) == size);
  return p;
}
int main(void) {
  assert(relative_ok("versions/1.8.9/1.8.9.json"));
  const char *bad[] = {"../escape", "/absolute", "a\\b", "a/../b",
                       "x:ads",     "a./x",      "a//b", NULL};
  for (int i = 0; bad[i]; i++)
    assert(!relative_ok(bad[i]));
  assert(official_url("https://libraries.minecraft.net/a.jar"));
  assert(!official_url("https://libraries.minecraft.net.evil/a"));
  assert(!official_url("http://libraries.minecraft.net/a"));
  assert(!official_url("https://user@libraries.minecraft.net/a"));
  Doc d;
  char err[512] = {0};
  char raw[] = "{\"id\":\"1.8.9\",\"text\":\"\\u4e2d\\u6587 "
               "\\ud83d\\ude00\",\"obj\":{\"nested\":[1,2,3]},\"flag\":true}";
  assert(doc_parse(&d, raw, err));
  assert(jeq(&d, jprop(&d, 0, "id"), "1.8.9"));
  char *s = jstr(&d, jprop(&d, 0, "text"));
  assert(!strcmp(s, "中文 😀"));
  free(s);
  assert(jbool(&d, jprop(&d, 0, "flag")));
  doc_free(&d);
  char *q = json_quote("a\"b\\c\n");
  assert(!strcmp(q, "\"a\\\"b\\\\c\\u000a\""));
  free(q);
  for (int k = 0; k < 2; k++) {
    const char *id = k ? "26.3" : "1.8.9";
    char path[400];
    snprintf(path, sizeof(path), "native-v19/tests/official-%s.json", id);
    Plan p;
    assert(plan_build(&p, id, readall(path), 64, fixture, err));
    printf("%s: %d files, %lld bytes, Java %s\n", id, p.count,
           (long long)p.bytes, p.java);
    if (!k) {
      assert(p.count == 759);
      assert(p.bytes == 145778490);
    } else {
      assert(p.count == 5223);
      assert(p.bytes == 614142551);
    }
    int natives = 0;
    for (int i = 0; i < p.count; i++)
      if (p.files[i].native)
        natives++;
    assert(natives == (k ? 11 : 4));
    plan_free(&p);
  }
  Plan p;
  assert(plan_build(&p, "1.8.9", readall("native-v19/tests/official-1.8.9.json"),
                    32, fixture, err));
  plan_free(&p);
  char *wrong = readall("native-v19/tests/official-1.8.9.json");
  assert(!plan_build(&p, "../../bad", wrong, 64, fixture, err));
  plan_free(&p);
  puts("PASS: official plans match v5 reference, 32/64-bit rules, native "
       "selection, UTF8 JSON, paths and URL validation.");
  return 0;
}
