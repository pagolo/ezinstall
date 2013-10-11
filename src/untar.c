#include "main.h"
#include "untar.h"

Bool myfeof(FILE* f) {
  Int32 c = fgetc(f);
  if (c == EOF) return True;
  ungetc(c, f);
  return False;
}

int myread
(bz_stream *stream,
        char *inbuff,
        char *outbuff,
        FILE* infile,
        int len,
        int *progress,
        int *bzerror
        ) {
  Int32 n, ret;
  BZ_SETERR(BZ_OK);
  stream->avail_out = BZ_MAX_UNUSED * 2;
  stream->next_out = outbuff;

  while (True) {

    if (ferror(infile)) {
      BZ_SETERR(BZ_IO_ERROR);
      return 0;
    };

    if (stream->avail_in == 0 && !myfeof(infile)) {
      n = fread(inbuff, sizeof (unsigned char),
              BZ_MAX_UNUSED, infile);
      if (ferror(infile)) {
        BZ_SETERR(BZ_IO_ERROR);
        return 0;
      };
      stream->avail_in = n;
      *progress += n;
      stream->next_in = inbuff;
    }

    ret = BZ2_bzDecompress(stream);

    if (ret != BZ_OK && ret != BZ_STREAM_END) {
      BZ_SETERR(ret);
      return 0;
    };

    if (ret == BZ_OK && myfeof(infile) &&
            stream->avail_in == 0 && stream->avail_out > 0) {
      BZ_SETERR(BZ_UNEXPECTED_EOF);
      return 0;
    };

    if (ret == BZ_STREAM_END) {
      BZ_SETERR(BZ_STREAM_END);
      return len - stream->avail_out;
    };
    if (stream->avail_out == 0) {
      BZ_SETERR(BZ_OK);
      return len;
    };

  }
  return 0; /*not reached*/
}

int inf_bz(FILE *source, int dest, STRING **list) {
  char inbuff[BZ_MAX_UNUSED];
  char outbuff[BZ_MAX_UNUSED * 2];
  bz_stream stream;
  int len = 1600, totalsize, progress = 0;
  int bzerror = 0, i = 0;
  struct stat filestat;

  if (source == NULL || dest < 0) return -1;
  if (fstat(fileno(source), &filestat) < 0) return -1; // *** stat() file
  totalsize = filestat.st_size; // *** get size of file

  stream.bzalloc = NULL;
  stream.bzfree = NULL;
  stream.opaque = NULL;
  BZ2_bzDecompressInit(&stream, 0, 0);

  while ((len = myread(&stream, inbuff, outbuff, source, len, &progress, &bzerror)) > 0) {
    char *s = mysprintf(_("Extracting (%d%%)"), (progress * 100) / totalsize);
    HandleSemaphoreText(s, list, !i++ ? 1 : 0);
    if (s) free(s);
    write(dest, outbuff, (BZ_MAX_UNUSED * 2) - stream.avail_out);
  }
  BZ2_bzDecompressEnd(&stream);
  fclose(source);
  return bzerror;
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
    if (strm.avail_in == 0)
      break;
    strm.next_in = in;
    progress += strm.avail_in; // *** increment progress
    {
      static int i = 0;
      char *s = mysprintf(_("Extracting (%d%%)"), (progress * 100) / totalsize);
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
          fclose(source);
          abort();
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
  close(fd);
  //puts("");
  if (ret && !(zip_format == BZ2_TAR && ret == BZ_STREAM_END))
    DaemonError("error", list);
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

int Untar(char *filename, STRING **list) {
  int _fd[2];
  int fd;
  struct raw_tar tar;
  long int len = 0, work, mod;
  pid_t childpid;
  char *buf = NULL;

  _fd[0] = _fd[1] = -1;
  pipe(_fd);

  if ((childpid = fork()) == -1) {
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
    switch (tar.type[0]) {
      case '5': // directory
        mkdir(tar.name, 0755);
        break;
      case '0': // real file
        {
          FILE *fh = fopen(tar.name, "w");
          if (fh) {
            if (buf) {
              fwrite(buf, len, 1, fh);
              free(buf);
              buf = NULL;
            }
            fclose(fh);
          }
        }
        break;
      // @TODO handle other types...
    }
    if (buf) {
      free(buf);
      buf = NULL;
    }
  }
  close(fd);
  waitpid(childpid, NULL, 0);
  return 0;
}
