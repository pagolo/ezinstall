#include "main.h"
#include <dirent.h>
#include <sys/param.h>

#ifndef HAVE_MEMMEM

/*
 **  MEMMEM.C - A strstr() work-alike for non-text buffers
 **
 **  public domain by Bob Stout --- thank you, Bob...
 */

void *memmem(const void *buf, size_t buflen, const void *pattern, size_t len) {
  size_t i, j;
  char *bf = (char *) buf, *pt = (char *) pattern;

  if (len > buflen)
    return (void *) NULL;

  for (i = 0; i <= (buflen - len); ++i) {
    for (j = 0; j < len; ++j) {
      if (pt[j] != bf[i + j])
        break;
    }
    if (j == len)
      return (bf + i);
  }
  return NULL;
}
#endif

/*********************************************/

char * append_cstring(char *rc, const char *appendable) {
  char *result;

  result = calloc((rc ? strlen(rc) : 0) + strlen(appendable) + 1, 1);
  if (result == NULL) return rc;
  if (rc) {
    strcpy(result, rc);
    free(rc);
    strcat(result, appendable);
  } else strcpy(result, appendable);

  return result;
}

char *mysprintf(const char *format, ...) {
  char *buf = calloc(6, 1);
  int buflen;
  va_list argptr;

  va_start(argptr, format);
  buflen = vsnprintf(buf, 4, format, argptr);
  free(buf);
  va_end(argptr);
  va_start(argptr, format);
  buf = calloc(++buflen, 1);
  vsnprintf(buf, buflen, format, argptr);
  va_end(argptr);

  return buf;
}

int execute(char *s) {
  char *rc;
  FILE *fh;
  if (!(rc = calloc(512, 1))) return 0;
  fh = popen(s, "r");
  if (!(fh)) {
    printf(getstr(S_NO_PROCESS, "External process failure..."));
    free(rc);
    return 0;
  }
  while (!(feof(fh)))
    fgets(rc, 512, fh);

  pclose(fh);
  free(rc);
  return 1;
}

/*
char *mycrypt(char *username, char *password)
{
      // password crittata
      char salt[3];
      char *crypted;
      int i;
      for (i=0; i<3; i++) {
         salt[i]=username[i];
         if (!((salt[i]>='0' && salt[i]<='9')
               ||(salt[i]>='A' && salt[i]<='Z')
               ||(salt[i]>='a' && salt[i]<='z')))
               salt[i]='z';
         }
      salt[2]='\0';
      crypted=crypt(password,salt);
      return crypted;
}
 */

//------------------------------------------

STRING *newstring(char *s) {
  STRING *rc = calloc(sizeof (STRING), 1);
  if (rc) rc->string = strdup(s);
  return rc;
}

int appendstring(STRING **head, char *s) {
  STRING *ptr, *nuovo;

  nuovo = newstring(s);
  if (!nuovo) return 0;

  if (*head == NULL) {
    *head = nuovo;
    return 1;
  }
  for (ptr = *head; ptr->next != NULL; ptr = ptr->next);
  ptr->next = nuovo;
  return 1;
}

int freestringlist(STRING *list) {
  STRING *ptr1, *ptr2;

  if (!list) return 0;

  for (ptr1 = list; ptr1; ptr1 = ptr2) {
    ptr2 = ptr1->next;
    if (ptr1->string) free(ptr1->string);
    free(ptr1);
  }

  return 1;
}

//------------------------------------------

void WriteLog(char *msg) {
  static char tbuffer[32];
  FILE *lh;
  time_t t;
  struct tm *tm;

  t = time(NULL);
  tm = localtime(&t);
  if (!(tm)) return;
  strftime(tbuffer, 32, "%Y-%m-%d %H:%M:%S ", tm);

  lh = fopen(globaldata.gd_logpath, "a");
  if (!(lh)) return; // return no error message to avoid inf. loops

  fprintf(lh, "%s", tbuffer); // write date-time
  // write archive name
  if (globaldata.gd_inidata && globaldata.gd_inidata->web_archive)
    fprintf(lh, "- %s - ", basename(globaldata.gd_inidata->web_archive));
  // write message
  fprintf(lh, "%s\n", msg);

  fclose(lh);
}

//------------------------------------------

char *get_ini_name(int argc, char **argv) {
  char *result = getfieldbyname("url");
  if (result && *result) return result;
  if (argc < 3) return NULL;
  if (argv[2]) return argv[2];
  return NULL;
}

char *get_ini_upload(void) {
  static char template[] = "ezinXXXXXX";
//  char *mark = TEMP_MARK;
  int len = 0, fd = NO_FILE;
  char *buffer = getbinarybyname("ini", &len);

  if (!buffer) return NULL;

  fd = mkstemp(template);
  if (fd == NO_FILE) return NULL;
  // mark removed due to xml format
  //write(fd,mark,strlen(mark)); // mark the file as an easinstall temporary file...
  write(fd, buffer, len);
  write(fd, "\n", 1); // add a newline at end of file
  close(fd);

  return template;
}

char *win_basename(char *path) {
  char *ptr;
  ptr = strrchr(path, '\\');
  if (ptr) return ++ptr;
  ptr = strrchr(path, '/');
  if (ptr) return ++ptr;
  return path;
}

int get_zip_upload(void) {
  mode_t u_mask;
  int len = 0, fd = NO_FILE, overwrite;
  char *overwrite_str;
  char *buffer = getbinarybyname("zip", &len);

  overwrite_str = getfieldbyname("overwrite");
  overwrite = (overwrite_str && strcasecmp(overwrite_str, "on") == 0) ? 1 : 0;

  if (!buffer)
    Error(getstr(S_NO_INPUT, "error receiving archive file..."));

  // controllare il nome del file
  if (strcasecmp(win_basename(getfieldbyname("zip")), basename(globaldata.gd_inidata->web_archive)) != 0)
    Error(getstr(S_NO_ARCHIVE_MATCH, "archive filename does not match with configuration data..."));

  // controllare se il file esiste
  if (access(globaldata.gd_inidata->web_archive, F_OK) == 0 && overwrite == 0)
    Error(getstr(S_ARCHIVE_EXISTS, "archive already exists! Please use the checkbox to overwrite it"));

  u_mask = umask(0);
  fd = open(globaldata.gd_inidata->web_archive, O_CREAT | O_RDWR, 0644);
  umask(u_mask);
  if (fd == NO_FILE)
    Error(getstr(S_NO_CREATE_ARCHIVE, "can't create archive file, check user and permissions."));
  write(fd, buffer, len);
  close(fd);

  if (globaldata.gd_loglevel > LOG_NONE)
    WriteLog(getstr(S_UPLOAD_OK, "upload successfull"));

  return 1;
}

int graburl(char *url, int permissions, int expand, int tempname) {
  int success = 0;
  SOCKET sock;
  char *z = NULL, *site = NULL, *dir = NULL;

  if ((!(url)) || (!(*url))) return 0;
  if (strncasecmp(url, "http://", 7) == 0) url = &url[7];

  z = strchr(url, '/');
  if (z) dir = strdup(z);
  else dir = strdup("/");

  site = strdup(url);
  z = strchr(site, '/');
  if (z) *z = '\0';

  sock = OpenConnection(site, 0, 80, 0);
  if (!(sock)) return 0;

  success = Download(sock, site, dir, tempname ? NULL : basename(url), permissions);
  CloseConnection(sock);
  if (site) free(site);

  if (success && globaldata.gd_loglevel > LOG_NONE)
    WriteLog(getstr(S_DOWNLOAD_OK, "download successfull"));

  return success;
}

//------------------------------------------

char *get_current_dir(void) {
  char *buffer = calloc(MAXPATHLEN, 1);
  if (buffer) getcwd(buffer, MAXPATHLEN);
  return buffer;
}

void chmod_recurse(char *dir, int bits) {
  char *current_dir;
  struct dirent *de;
  struct stat mystat;
  DIR *mydir;

  current_dir = get_current_dir();
  if (chdir(dir) == -1) {
    /*printf("can't change dir %s\n",dir);*/
    return;
  }
  mydir = opendir(".");
  if (mydir) {
    while ((de = readdir(mydir)) && lstat(de->d_name, &mystat) == 0) {
      if ((strcmp(de->d_name, "..") != 0))
        chmod(de->d_name, bits);
      if ((mystat.st_mode & S_IFDIR) && (strcmp(de->d_name, ".") != 0) && (strcmp(de->d_name, "..") != 0))
        chmod_recurse(de->d_name, bits);
    }
  }
  chdir(current_dir);
  free(current_dir);
}

void ChangePermissionsRecurse(void) {
  CHMOD *chm = globaldata.gd_inidata->perm_list_rec;
  while (chm) {
    printf("chmod -R 0%o \"%s\"...<br>", chm->permissions, chm->file);
    chmod_recurse(chm->file, chm->permissions);
    chm = chm->next;
  }
  if (globaldata.gd_inidata->perm_list_rec && globaldata.gd_loglevel > LOG_NONE)
    WriteLog(getstr(S_RECURSE_OK, "recursive permissions setup"));
}

void ChangePermissions(void) {
  CHMOD *chm = globaldata.gd_inidata->perm_list;
  while (chm) {
    printf("chmod 0%o \"%s\"...<br />", chm->permissions, chm->file);
    if (chm->createfolder)
      mkdir(chm->file, chm->permissions);
    chmod(chm->file, chm->permissions);
    chm = chm->next;
  }
  if (globaldata.gd_inidata->perm_list && globaldata.gd_loglevel > LOG_NONE)
    WriteLog(getstr(S_CHMOD_OK, "permissions setup"));
}
