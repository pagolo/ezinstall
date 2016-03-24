#ifndef __CURL_H__
#define __CURL_H__

#include <curl/curl.h>

#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL     1

struct myprogress {
  double lastruntime;
  CURL *curl;
  STRING **list;
};

int CurlDownload(char *url, int permissions, int tempname, STRING **list);
char *CurlPhp(char *url);

#endif

