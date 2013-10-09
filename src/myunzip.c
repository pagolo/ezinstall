#include "main.h"
#include <errno.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>

#include "unzip.h"

FILE *fopen64(const char *filename, const char *type);

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)
#define MAXFILENAME (256)

/* change_file_date : change the date/time of a file
    filename : the filename of the file where date/time must be modified
    dosdate : the new date at the MSDos format (4 bytes)
    tmu_date : the SAME new date at the tm_unz format */
void change_file_date(const char *filename, tm_unz tmu_date) {
#ifdef unix
  struct utimbuf ut;
  struct tm newdate;
  newdate.tm_sec = tmu_date.tm_sec;
  newdate.tm_min = tmu_date.tm_min;
  newdate.tm_hour = tmu_date.tm_hour;
  newdate.tm_mday = tmu_date.tm_mday;
  newdate.tm_mon = tmu_date.tm_mon;
  if (tmu_date.tm_year > 1900)
    newdate.tm_year = tmu_date.tm_year - 1900;
  else
    newdate.tm_year = tmu_date.tm_year;
  newdate.tm_isdst = -1;

  ut.actime = ut.modtime = mktime(&newdate);
  utime(filename, &ut);
#else
#error Unix only
#endif
}

/* mymkdir and change_file_date are not 100 % portable
   As I don't know well Unix, I wait feedback for the unix portion */

int mymkdir(const char* dirname) {
  int ret = 0;
  ret = mkdir(dirname, 0755); // changed 775 -> 755
  return ret;
}

int makedir(const char *newdir, STRING **list) {
  char *buffer;
  char *p;
  int len = (int) strlen(newdir);

  if (len <= 0)
    return 0;

  buffer = (char*) malloc(len + 1);
  if (buffer == NULL) {
    DaemonError(_("Error allocating memory\n"), list);
  }
  strcpy(buffer, newdir);

  if (buffer[len - 1] == '/') {
    buffer[len - 1] = '\0';
  }
  if (mymkdir(buffer) == 0) {
    free(buffer);
    return 1;
  }

  p = buffer + 1;
  while (1) {
    char hold;

    while (*p && *p != '\\' && *p != '/')
      p++;
    hold = *p;
    *p = 0;
    if ((mymkdir(buffer) == -1) && (errno == ENOENT)) {
      DaemonError(mysprintf(_("couldn't create directory %s"), buffer), list);
    }
    if (hold == 0)
      break;
    *p++ = hold;
  }
  free(buffer);
  return 1;
}

int do_extract_currentfile(uf, popt_extract_without_path, password, list)
unzFile uf;
const int* popt_extract_without_path;
const char* password;
STRING **list;
{
  char filename_inzip[256];
  char* filename_withoutpath;
  char* p;
  int err = UNZ_OK;
  FILE *fout = NULL;
  void* buf;
  uInt size_buf;

  unz_file_info64 file_info;
  //    uLong ratio=0;
  err = unzGetCurrentFileInfo64(uf, &file_info, filename_inzip, sizeof (filename_inzip), NULL, 0, NULL, 0);

  if (err != UNZ_OK) {
    DaemonError(mysprintf(_("error %d with zipfile in unzGetCurrentFileInfo"), err), list);
  }

  size_buf = WRITEBUFFERSIZE;
  buf = (void*) malloc(size_buf);
  if (buf == NULL) {
    DaemonError(_("Error allocating memory"), list);
  }

  p = filename_withoutpath = filename_inzip;
  while ((*p) != '\0') {
    if (((*p) == '/') || ((*p) == '\\'))
      filename_withoutpath = p + 1;
    p++;
  }

  if ((*filename_withoutpath) == '\0') {
    if ((*popt_extract_without_path) == 0) {
      mymkdir(filename_inzip);
    }
  } else {
    const char* write_filename;
    int skip = 0;

    if ((*popt_extract_without_path) == 0)
      write_filename = filename_inzip;
    else
      write_filename = filename_withoutpath;

    err = unzOpenCurrentFilePassword(uf, password);
    if (err != UNZ_OK) {
      DaemonError(mysprintf(_("error %d with zipfile in unzOpenCurrentFilePassword"), err), list);
    }

    if ((skip == 0) && (err == UNZ_OK)) {
      fout = fopen64(write_filename, "wb");

      /* some zipfile don't contain directory alone before file */
      if ((fout == NULL) && ((*popt_extract_without_path) == 0) &&
              (filename_withoutpath != (char*) filename_inzip)) {
        char c = *(filename_withoutpath - 1);
        *(filename_withoutpath - 1) = '\0';
        makedir(write_filename, list);
        *(filename_withoutpath - 1) = c;
        fout = fopen64(write_filename, "wb");
      }

      if (fout == NULL) {
        DaemonError(mysprintf(_("error opening %s"), err), list);
      }
    }

    if (fout != NULL) {
      do {
        err = unzReadCurrentFile(uf, buf, size_buf);
        if (err < 0) {
          DaemonError(mysprintf(_("error %d with zipfile in unzReadCurrentFile"), err), list);
          break;
        }
        if (err > 0)
          if (fwrite(buf, err, 1, fout) != 1) {
            err = UNZ_ERRNO;
            break;
          }
      } while (err > 0);
      if (fout)
        fclose(fout);

      if (err == 0)
        change_file_date(write_filename,
              file_info.tmu_date);
    }

    if (err == UNZ_OK) {
      err = unzCloseCurrentFile(uf);
      if (err != UNZ_OK) {
        DaemonError(mysprintf(_("error %d with zipfile in unzCloseCurrentFile"), err), list);
      }
    } else
      unzCloseCurrentFile(uf); /* don't lose the error */
  }

  free(buf);
  return err;
}

int do_extract(unzFile uf, int opt_extract_without_path, const char *password, STRING **list) {
  uLong i;
  unz_global_info64 gi;
  int err;
  //    FILE* fout=NULL;

  err = unzGetGlobalInfo64(uf, &gi);
  if (err != UNZ_OK)
    DaemonError(mysprintf(_("error %d with zipfile in unzGetGlobalInfo"), err), list);

  for (i = 0; i < gi.number_entry; i++) {
    if (do_extract_currentfile(uf, &opt_extract_without_path,
            password, list) != UNZ_OK)
      break;

    if ((i + 1) < gi.number_entry) {
      err = unzGoToNextFile(uf);
      if (err != UNZ_OK) {
        DaemonError(mysprintf(_("error %d with zipfile in unzGoToNextFile"), err), list);
      }
    }
    {
      char *s = mysprintf(_("Extracting (%d%%)"), ((i + 1) * 100) / gi.number_entry);
      HandleSemaphoreText(s, list, !i ? 1 : 0);
      if (s) free(s);
    }
  }

  return 0;
}

int Unzip(const char *zipfilename, STRING **list) {
  const char *password = NULL;
  char filename_try[MAXFILENAME + 16] = "";
  int ret_value = 0;
  unzFile uf = NULL;

  if (!zipfilename) return -1;

  strncpy(filename_try, zipfilename, MAXFILENAME - 1);
  /* strncpy doesnt append the trailing NULL, of the string is too long. */
  filename_try[ MAXFILENAME ] = '\0';

  uf = unzOpen64(zipfilename);
  if (uf == NULL) {
    DaemonError(mysprintf(_("Cannot open %s"), zipfilename), list);
  }
  {
    char *s = mysprintf(_("Opening %s..."), filename_try);
    HandleSemaphoreText(s, list, 1);
    if (s) free(s);
  }
  ret_value = do_extract(uf, 0, password, list);
  unzClose(uf);

  return ret_value ? -1 : 0;
}
