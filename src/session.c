#include "main.h"

#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>

int checktime(struct stat *stat) {
  time_t diff = time(NULL) - stat->st_mtime;

  // time_t session_timeout = 20 * 60;
  if (diff > globaldata.gd_session_timeout)
    return 0;

  return 1;
}

void freesession(char *session) {
  remove(session);
}

char *checksession(char *session, int logout) {
  struct stat stat;
  char *read_buf;
  char *username = NULL;
  int fd;

  fd = open(session, O_EXCL);

  if (fd == -1) return NULL;
  fstat(fd, &stat);
  if (!(checktime(&stat))) {
    close(fd);
    return NULL;
  }
  read_buf = calloc(stat.st_size + 1, 1);
  if (!(read_buf)) {
    close(fd);
    return NULL;
  }
  read(fd, read_buf, stat.st_size);
  if (strncmp(read_buf, "EZINSTALL", 8) != 0) {
    free(read_buf);
    close(fd);
    return NULL;
  }
  close(fd);
  
  if (stat.st_size > 9)
    username = strdup(&read_buf[9]);
  free(read_buf);

  // se uscita allora disabilita sessione, altrimenti aggiornala
  if (logout) { // nel caso di un logout (per ora disabilitato...)
    freesession(session);
  } else {
    utime(session, NULL);
    globaldata.gd_session = session;
  }

  return username;
}

STRING *removes;

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

void checkndrop(char *session) {
  struct stat stat;
  char *read_buf;
  int fd;

  fd = open(session, O_EXCL);
  if (fd == -1) return;

  fstat(fd, &stat);
  if ((checktime(&stat))) {
    close(fd);
    return;
  }
  read_buf = calloc(stat.st_size + 1, 1);
  if (!(read_buf)) {
    close(fd);
    return;
  }
  read(fd, read_buf, stat.st_size);
  if (strncmp(read_buf, "EZINSTALL", 8) != 0) {
    free(read_buf);
    close(fd);
    return;
  }
  close(fd);
  appendstring(&removes, session);
}

void freeoldsessions(void) {
  struct dirent *de;
  DIR *mydir = opendir(".");

  if (mydir) {
    while ((de = readdir(mydir))) {
      if (strncmp(de->d_name, "EZ", 2) == 0 && strlen(de->d_name) == 8)
        checkndrop(de->d_name);
    }
  }
  if (removes) {
    STRING *s;
    for (s = removes; s; s = s->next) remove(s->string);
    freestringlist(removes);
    removes = NULL;
  }
}

int createsession(char *username) {
  int fd;
  char *tmp = strdup("EZXXXXXX");

  freeoldsessions();

#ifdef __GNUC__
  fd = mkstemp(tmp);
#else
  tmp = tmpnam(tmp);
  fd = open(tmp, _O_CREAT);
#endif

  if (fd == -1) {
    free(tmp);
    return 0;
  }
  write(fd, "EZINSTALL", 9);
  write(fd, username, strlen(username));
  fchmod(fd, S_IRUSR | S_IWUSR);
  close(fd);
  globaldata.gd_session = tmp;

  return 1;
}
