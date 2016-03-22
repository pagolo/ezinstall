#include "main.h"

static int xferinfo(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
  struct myprogress *myp = (struct myprogress *)p;
  CURL *curl = myp->curl;
  STRING **list = myp->list;
  double curtime = 0;

  curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);

  /* under certain circumstances it may be desirable for certain functionality
     to only run every N seconds, in order to do this the transaction time can
     be used */
  if ((curtime - myp->lastruntime) >= MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL) {
    myp->lastruntime = curtime;
  }

  if (list && dltotal) {
    long long x = ((dlnow * (long)100) / dltotal);
    char *s = mysprintf(_("Downloading archive (%d%%)"), x > 100 ? 100 : x);
    HandleSemaphoreText(s, list, 0);
    if (s) free(s);
  }

  return 0;
}

/* for libcurl older than 7.32.0 (CURLOPT_PROGRESSFUNCTION) */
static int older_progress(void *p,
                          double dltotal, double dlnow,
                          double ultotal, double ulnow)
{
  return xferinfo(p,
                  (curl_off_t)dltotal,
                  (curl_off_t)dlnow,
                  (curl_off_t)ultotal,
                  (curl_off_t)ulnow);
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

int CurlDownload(char *url, int mask, int tempname, STRING **list) {
  CURL *curl;
  CURLcode res = CURLE_GOT_NOTHING;
  struct myprogress prog;
  char *urlfile;
  FILE *pagefile;
  int fd = NO_FILE;
//  static char cwd[512];
  
  if ((!(url)) || (!(*url))) return 0;
  urlfile = globaldata.gd_inidata->filename;

  curl = curl_easy_init();
  if(curl) {
    prog.lastruntime = 0;
    prog.curl = curl;
    prog.list = list;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Ezinstall downloader/0.1");
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, older_progress);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &prog);

#if LIBCURL_VERSION_NUM >= 0x072000
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
#endif

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, list == NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
//    getcwd(cwd, 512);
//    ChDirRoot();
    
    fd = create_file(tempname ? NULL : urlfile, mask);
    if (fd == NO_FILE) {
      curl_easy_cleanup(curl);
      if (list)
        DaemonError(_( "Can't create archive file"), list);
      else
        Error(_( "Can't create archive file"));
    }

//    chdir(cwd);
        
    pagefile = fdopen(fd, "wb");
    if (pagefile) {
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);
      res = curl_easy_perform(curl);
      fclose(pagefile);
    }

    if(res != CURLE_OK) {
      char *msg = mysprintf("%s", curl_easy_strerror(res));
      curl_easy_cleanup(curl);
      if (list) DaemonError(msg, list);
      else Error(msg);
    }

    curl_easy_cleanup(curl);
  }

  return (res == CURLE_OK);
}