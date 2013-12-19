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

void Expand(char *filename, STRING **list, int fd, int zip_format) {
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

void ResetNextData(char **nextname, long int *nextlink) {
  free(*nextname);
  *nextname = NULL;
  *nextlink = 0;
}

int Untar(char *filename, STRING **list) {
  int _fd[2];
  int fd;
  struct raw_tar tar;
  long int len = 0, work, mod, nextlink = 0;
  pid_t childpid;
  char *buf = NULL;
  char *nextname = NULL;

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
    ReadBuffer(fd, (char *) &tar, sizeof (tar));
    if (!tar.name[0]) break;
    len = strtol(tar.size, NULL, 8);
    //sscanf(tar.size, "%o", &len);
    work = len;
    mod = work % 512;
    if (work && mod) work += 512 - mod;
    if (work) {
      buf = calloc(1, work + 1);
      ReadBuffer(fd, buf, work);
    }
    mode_t mode = (mode_t)strtol(tar.mode, NULL, 8);
    switch (tar.type[0]) {
      case '5': // directory @TODO: check for error
        if (mkdir((nextname && !nextlink) ? nextname : tar.name, mode) < 0) {
          exit(100);
        }
        if (nextname) ResetNextData(&nextname, &nextlink);
        break;
      case '1':
        link(tar.linked, (nextname && nextlink) ? nextname : tar.name);
        if (nextname) ResetNextData(&nextname, &nextlink);
        break;
      case '2':
        symlink(tar.linked, (nextname && nextlink) ? nextname : tar.name);
        if (nextname) ResetNextData(&nextname, &nextlink);
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
        {
          FILE *fh = fopen((nextname && !nextlink) ? nextname : tar.name, "w");
          if (fh) {
            fchmod(fileno(fh), mode);
            if (buf) {
              fwrite(buf, len, 1, fh);
              free(buf);
              buf = NULL;
            }
            fclose(fh);
          }
        }
        if (nextname) ResetNextData(&nextname, &nextlink);
        break;
      // @TODO handle other types...
    }
    if (buf) {
      free(buf);
      buf = NULL;
    }
  }
  close(fd);
  int status;
  waitpid(childpid, &status, 0);
  if (status == 0)
    HandleSemaphoreText(_("Files have been extracted"), list, 1);
  else
    exit(10);
  return 0;
}
