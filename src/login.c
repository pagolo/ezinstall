#include "main.h"

/******* decodifica url ***********/
void unescape(char *src) {
  int i, ii, c;
  int len = strlen(src);
  char buf[4];
  char *tail;
  char *ptr = calloc(1, len + 1);

  if (!ptr) return;
  buf[2] = '\0';

  for (i = ii = 0; i < len; ii++) {
    if (src[i] == '%') {
      strncpy(buf, &src[i + 1], 2);
      c = strtol(buf, &tail, 16);
      ptr[ii] = (char) c;
      i += 3;
    } else ptr[ii] = src[i++];
  }
  ptr[ii] = '\0';
  strcpy(src, ptr);
  free(ptr);
}

/* data compatibile con cookies */
char *cookietime(time_t *t) {
  struct tm *tm;
  static char tbuf[36];

  tm = gmtime(t);
  strftime(tbuf, sizeof tbuf, "%a %d %b %Y %H:%M:%S GMT", tm);

  return tbuf;
}

void putcookie(char *name, char *opaque,
        time_t t, char *path, char *domain, int secure) {
  char *expires;

  printf("Set-Cookie: %s=%s;", name, opaque);

  if (t != -1L) {
    expires = cookietime(&t);
    printf(" expires=%s;", expires);
  }
  if (path != NULL)
    printf(" path=%s;", path);
  if (domain != NULL)
    printf(" domain=%s;", domain);
  if (secure)
    printf(" secure");
  printf("\n");
}

char *getcookie(char *name) {
  char *rc, *env, *ptr;
  env = getenv("HTTP_COOKIE");
  if (!env || !*env) return NULL;
  rc = calloc(strlen(env) + 1, 1);
  if (!(rc)) return NULL;
  ptr = strstr(env, name);
  if (!(ptr && ptr[strlen(name)] == '=')) {
    free(rc);
    return NULL;
  }
  ptr = &ptr[strlen(name) + 1];
  strcpy(rc, ptr);
  ptr = rc;
  // vecchia versione -->	while (isalnum(*ptr) || (*ptr=='/') || (*ptr=='.') || (*ptr=='|')) ++ptr;
  while (*ptr != '\0' && *ptr != ';') ++ptr;
  *ptr = '\0';
  unescape(rc);
  return *rc ? rc : NULL;
}

int AreDataOk(char *id, char *pass, char *username, char *password) {
  if (!(strcmp(id, username) == 0)) return 0;
  if (strcmp(do_hash(pass), password) == 0) return 1;
  return 0;
}

