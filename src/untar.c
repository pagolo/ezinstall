#define _GNU_SOURCE
#include "main.h"
#include "untar.h"
#include <errno.h>

int inf_bz(FILE *f, int dest, STRING **list) {
  BZFILE* b;
  int nBuf;
  char buf[5000];
  int bzerror;
  int totalsize;
  struct stat filestat;

  if (fstat(fileno(f), &filestat) < 0) return -1; // *** stat() file
  totalsize = filestat.st_size; // *** get size of file

  if (!f) {
    return BZ_IO_ERROR;
  }
  b = BZ2_bzReadOpen(&bzerror, f, 0, 0, NULL, 0);
  if (bzerror != BZ_OK) {
    BZ2_bzReadClose(&bzerror, b);
    return bzerror;
  }

  bzerror = BZ_OK;
  while (bzerror == BZ_OK) {
    nBuf = BZ2_bzRead(&bzerror, b, buf, 5000);
    if (bzerror == BZ_OK || bzerror == BZ_STREAM_END) {
      write(dest, buf, nBuf);
      {
        static int i = 0;
        char *s = mysprintf(_("Extracting (%d%%)"), (ftell(f) * 100ul) / totalsize);
        HandleSemaphoreText(s, list, !i++ ? 1 : 0);
        if (s) free(s);
      }
    }
  }
  if (bzerror != BZ_STREAM_END) {
    BZ2_bzReadClose(&bzerror, b);
    return bzerror;
  } else {
    BZ2_bzReadClose(&bzerror, b);
  }
  return BZ_OK;
}

int inf(FILE *source, int dest, STRING **list) {
  int ret;
  unsigned have;
  z_stream strm;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];
  int totalsize; // ***size of source file
  int progress = 0; // ***progress of inflation
  struct stat filestat; // ***stat structure

  if (source == NULL || dest < 0) { //** added file opened control
    if (source) fclose(source);
    return Z_ERRNO; // *** check file handles
  }
  if (fstat(fileno(source), &filestat) < 0) return Z_ERRNO; // *** stat() file
  totalsize = filestat.st_size; // *** get size of file

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  //ret = inflateInit(&strm); .Z file
  ret = inflateInit2(&strm, 16 + MAX_WBITS); // gzip file
  if (ret != Z_OK)
    return ret;

  /* decompress until deflate stream ends or end of file */
  do {
    strm.avail_in = fread(in, 1, CHUNK, source);
    if (ferror(source)) {
      (void) inflateEnd(&strm);
      fclose(source);
      return Z_ERRNO;
    }
    if (strm.avail_in == 0) {
      //WriteLog("READ=0");
      break;
    }
    strm.next_in = in;
    progress += strm.avail_in; // *** increment progress
    {
      static int i = 0;
      char *s = mysprintf(_("Extracting (%d%%)"), (progress * 100ul) / totalsize);
      HandleSemaphoreText(s, list, !i++ ? 1 : 0);
      if (s) free(s);
    }

    /* run inflate() on input until output buffer not full */
    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = inflate(&strm, Z_NO_FLUSH);
      //***removed assertion
      //assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      switch (ret) {
        //*** added case Z_STREAM_ERROR
        case Z_STREAM_ERROR:
          (void) inflateEnd(&strm);
          fclose(source);
          return ret;
          //abort();
        case Z_NEED_DICT:
          ret = Z_DATA_ERROR; /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          (void) inflateEnd(&strm);
          fclose(source);
          return ret;
      }
      have = CHUNK - strm.avail_out;
      if (write(dest, out, have) != have) {
        (void) inflateEnd(&strm);
        fclose(source);
        return Z_ERRNO;
      }
    } while (strm.avail_out == 0);

    /* done when inflate() says it's done */
  } while (ret != Z_STREAM_END);

  /* clean up and return */
  (void) inflateEnd(&strm);
  fclose(source);
  //WriteLog("Uncompress end");
  return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

void Expand(const char *filename, STRING **list, int fd, int zip_format) {
  int (*tar_inflate) (FILE *source, int dest, STRING **list);
  int ret;

  if (zip_format == BZ2_TAR)
    tar_inflate = &inf_bz;
  else
    tar_inflate = &inf;
  ret = (*tar_inflate)(fopen(filename, "rb"), fd, list);
  if (ret && !(zip_format == BZ2_TAR && (ret == BZ_STREAM_END || ret == BZ_SEQUENCE_ERROR)))
    DaemonError("generic error", list);
  close(fd);
  exit(0);
}

/* tar follows */

void ReadBuffer(int fd, char *buffer, int size) {
  static int check_zero;
  int charsno = read(fd, buffer, size);
  if (charsno == size) return; // all ok
  if (charsno == 0) {
    if (check_zero++ > 500) return; // unexpected end of file?
  } else check_zero = 0; // reset counter
  if (charsno < 0) {
    exit(100);
  } // error
  if (charsno < size) { // incomplete buffer, recurse...
    nanosleep((struct timespec[]) {
      {0, 500000}}, NULL);
    ReadBuffer(fd, &buffer[charsno], size - charsno);
  }
}

void ResetNextData(char **wnf, char **nextname, long int *nextlink, TAR_EXT *extension) {
  if (*wnf) free(*wnf);
  if (*nextname) free(*nextname);
  if (extension->path) {
    free(extension->path);
    extension->path = NULL;
  }
  if (extension->linkpath) {
    free(extension->linkpath);
    extension->linkpath = NULL;
  }
  *wnf = NULL;
  *nextname = NULL;
  *nextlink = 0;
}

void GetTarExtension(char *buffer, int len, TAR_EXT *extension) {
  char *dat, *ptr = dat = buffer;
  while (ptr < &buffer[len]) {
    int datlen = atoi(ptr);
    if (datlen) {
      char *z, *sp2z, *name, *value;
      while (isdigit(*ptr)) ++ptr;
      while (isspace(*ptr)) ++ptr;
      if (ptr >= &buffer[len]) break;
      z = sp2z = strchr(ptr, '=');
      if (z) {
        *z = '\0';
        name = ptr;
        while (--sp2z > ptr && isspace(*sp2z))
          *sp2z = '\0';
        ptr = ++z;
        while (isspace(*ptr)) ++ptr;
        z = strchr(ptr, 0x0A);
        if (z == &dat[datlen-1]) {
          *z = '\0';
          value = ptr;
          if (strcmp(name, "path") == 0) {
            extension->path = strdup(value);
          }
          if (strcmp(name, "linkpath") == 0) {
            extension->linkpath = strdup(value);
          }
        }
      }
    } else {
      break;
    }
    dat = ptr = &dat[datlen];
  }
}

void CreateFolderIfNotExists(char *path, mode_t mode) {
  char *folder = strdup(path);
  char *ptr = strrchr(folder, '/');
  if (ptr) {
    struct stat sb;
    *ptr = '\0';
    if (!(stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode)))
    {
      if (mkdir(folder, mode) < 0) {
        CreateFolderIfNotExists(folder, 0755);
        if (mkdir(folder, mode) < 0) {
          exit(100);
        }
      }
    }
  }
  free(folder);
}

int Untar(const char *filename, STRING **list) {
  int _fd[2];
  int fd;
  struct raw_tar tar;
  long int len = 0, work, mod, nextlink = 0, do_save = (globaldata.gd_inidata->archive_dir && *(globaldata.gd_inidata->archive_dir))? 0 : 1;
  pid_t childpid;
  char *buf = NULL;
  char *nextname = NULL;
  char *tar_name = NULL;
  char *subdir = NULL;
  TAR_EXT extension;

  memset(&extension, 0, sizeof(TAR_EXT));
  
  if (access(filename, F_OK) != 0)
    return -1;
  
  _fd[0] = _fd[1] = -1;
  pipe(_fd);

  if ((childpid = fork()) == -1) {
    // @TODO remove perror()
    perror("fork");
    exit(1);
  }
  if (childpid == 0) {
    /* Child process closes up input side of pipe */
    close(_fd[0]);
    Expand(filename, list, _fd[1], globaldata.gd_inidata->zip_format);
  }

  close(_fd[1]);
  fd = _fd[0];

  for (;;) {
    char *work_name_free, *work_name;
    work_name = work_name_free = NULL;
    ReadBuffer(fd, (char *) &tar, sizeof (tar));
    if (!tar.name[0]) break;
    tar_name = calloc(101,  sizeof(char));
    if (!tar_name) break; // no memory
    strncpy(tar_name,tar.name,100);
    len = strtol(tar.size, NULL, 8);
    work = len;
    mod = work % 512;
    if (work && mod) work += 512 - mod;
    if (work) {
      buf = calloc(work + 1, sizeof(char));
      if (!buf) break; // no memory
      ReadBuffer(fd, buf, work);
    }
    mode_t mode = (mode_t)strtol(tar.mode, NULL, 8);

    switch (tar.type[0]) {
      case '5': // directory @TODO: check for error
        work_name = (nextname && !nextlink) ? nextname : tar_name;
        if (extension.path) work_name = extension.path;
        else if (tar.prefix[0]) {
          work_name_free = work_name = mysprintf("%.155s/%.100s", tar.prefix, tar_name);
        }
        if (do_save) {
          if (subdir && strstr(work_name, subdir) == work_name)
            work_name = &work_name[strlen(subdir) + 1];
          CreateFolderIfNotExists(work_name, mode);
        } else if (strcmp(basename(work_name), globaldata.gd_inidata->archive_dir) == 0) {
          subdir = strdup(work_name);
          do_save = 1;
        }
        ResetNextData(&work_name_free, &nextname, &nextlink, &extension);
        break;
      case '1':
      case '2':
        work_name = (nextname && nextlink) ? nextname : tar_name;
        if (extension.path) work_name = extension.path;
        else if (tar.prefix[0]) {
          work_name_free = work_name = mysprintf("%.155s/%.100s", tar.prefix, tar_name);
        }
        if (!do_save && globaldata.gd_inidata->archive_dir) {
          char *ptr = strstr(work_name, globaldata.gd_inidata->archive_dir);
          if (ptr) {
            char *z = strchr(ptr, '/');
            if (z) *z = '\0';
            subdir = strdup(work_name);
            if (z) *z = '/';
            do_save = 1;
          }
        }
        if (do_save) {
          char *tar_linked = tar.linked;
          if (extension.linkpath) tar_linked = extension.linkpath;
          if (subdir && strstr(work_name, subdir) == work_name)
            work_name = &work_name[strlen(subdir) + 1];
          if (subdir && strstr(tar_linked, subdir) == tar_linked)
            tar_linked = &tar_linked[strlen(subdir) + 1];
          CreateFolderIfNotExists(work_name, 0755);
          if (tar.type[0] == '1') link(tar_linked, work_name);
          else symlink(tar_linked, work_name);                    
        }          
        ResetNextData(&work_name_free, &nextname, &nextlink, &extension);
        break;
      case 'x':
        if (memmem(buf, len, "path", 4))
          GetTarExtension(buf, len, &extension);
        break;
      case 'K':
        nextlink = 1;
      case 'L':
        if (!buf) break;
        nextname = calloc(len + 1, 1);
        if (!nextname) {
          exit(100);
        }
        strncpy(nextname, buf, len);
        break;
      case '0': // real file
      case '\0':
        work_name = (nextname && !nextlink) ? nextname : tar_name;
        if (extension.path) work_name = extension.path;
        else if (tar.prefix[0]) {
          work_name_free = work_name = mysprintf("%.155s/%.100s", tar.prefix, tar_name);
        }
        if (!do_save && globaldata.gd_inidata->archive_dir) {
          char *ptr = strstr(work_name, globaldata.gd_inidata->archive_dir);
          if (ptr) {
            char *z = strchr(ptr, '/');
            if (z) *z = '\0';
            subdir = strdup(work_name);
            if (z) *z = '/';
            do_save = 1;
          }
        }
        int is_normal = (do_save && (subdir == NULL || strncmp(work_name, subdir, strlen(subdir)) == 0));
        if (is_normal || FileIsSQL(work_name)) {
          if (is_normal) {
            if (subdir) work_name = &work_name[strlen(subdir) + 1];
            CreateFolderIfNotExists(work_name, 0755);
          } else {
            work_name = basename(work_name);
          }
          FILE *fh = fopen(work_name, "w");
          if (fh) {
            fchmod(fileno(fh), mode);
            if (buf) {
              fwrite(buf, len, 1, fh);
            }
            fclose(fh);
          }
        }
        if (buf) {
          free(buf);
          buf = NULL;
        }
        ResetNextData(&work_name_free, &nextname, &nextlink, &extension);
        break;
      // @TODO handle other types...
    }
    if (buf) {
      free(buf);
      buf = NULL;
    }
    if (tar_name) free(tar_name);
  }
  close(fd);
  if (subdir) free(subdir);
  int status;
  waitpid(childpid, &status, 0);
  if (status == 0)
    HandleSemaphoreText(_("Files have been extracted"), list, 1);
  else
    exit(10);
  return 0;
}
