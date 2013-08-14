#ifndef __LOGIN_H__
#define __LOGIN_H__
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

void unescape (char *src);
void putcookie(char *name, char *opaque, time_t t, char *path, char *domain, int secure);
char *getcookie(char *name);
int AreDataOk(char *id,char *pass,char *username,char *password);
#endif
