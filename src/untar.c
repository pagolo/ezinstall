/* ========================================================================== */
/*                                                                            */
/*   Filename.c                                                               */
/*   (c) 2001 Author                                                          */
/*                                                                            */
/*   Description                                                              */
/*                                                                            */
/* ========================================================================== */

#include "main.h"
#include "untar.h"

/* bzip follows */
char inbuff[BZ_MAX_UNUSED];
char outbuff[BZ_MAX_UNUSED*2];

static Bool myfeof ( FILE* f )
{
   Int32 c = fgetc ( f );
   if (c == EOF) return True;
   ungetc ( c, f );
   return False;
}

int my_read 
           ( bz_stream *stream, 
             FILE* infile,
             int len,
             int *progress,
             int *bzerror
              )
{
   Int32   n, ret;

   BZ_SETERR(BZ_OK);
   stream->avail_out = BZ_MAX_UNUSED*2;
   stream->next_out = outbuff;

   while (True) {

      if (ferror(infile)) 
         { BZ_SETERR(BZ_IO_ERROR); return 0; };

      if (stream->avail_in == 0 && !myfeof(infile)) {
         n = fread ( inbuff, sizeof(unsigned char), 
                     BZ_MAX_UNUSED, infile );
         if (ferror(infile))
            { BZ_SETERR(BZ_IO_ERROR); return 0; };
         stream->avail_in = n;
         *progress += n;
         stream->next_in = inbuff;
      }

      ret = BZ2_bzDecompress ( stream );

      if (ret != BZ_OK && ret != BZ_STREAM_END)
         { BZ_SETERR(ret); return 0; };

      if (ret == BZ_OK && myfeof(infile) && 
          stream->avail_in == 0 && stream->avail_out > 0)
         {  BZ_SETERR(BZ_UNEXPECTED_EOF); return 0; };

      if (ret == BZ_STREAM_END)
         { BZ_SETERR(BZ_STREAM_END);
           return len - stream->avail_out; };
      if (stream->avail_out == 0)
         { BZ_SETERR(BZ_OK); return len; };
      
   }

   return 0; /*not reached*/
}

int inf_bz(FILE *source, int dest) {
  static bz_stream stream;
  int  len = 1600, totalsize, progress = 0;
  int  bzerror = 0;
  struct stat filestat;
      
  if (source == NULL || dest < 0) return -1;
  if (fstat(fileno(source), &filestat) < 0) return -1; // *** stat() file
  totalsize = filestat.st_size; // *** get size of file

   stream.bzalloc  = NULL;
   stream.bzfree   = NULL;
   stream.opaque   = NULL;
   BZ2_bzDecompressInit(&stream, 0, 0);

  while (len = my_read(&stream, source, len, &progress, &bzerror)) {
            printf("\rExtracting (%d%%)...", (progress * 100) / totalsize); //*** show progression 
    write(dest,outbuff,(BZ_MAX_UNUSED*2)-stream.avail_out);
  }
  BZ2_bzDecompressEnd (&stream);
  fclose(source);
  return bzerror;  
}
/* bzip ends */

/* gzip follows... */
// Mark Adler function follows... (lines marked with *** have been added)

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, int dest)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    int totalsize; // ***size of source file
    int progress = 0; // ***progress of inflation
    struct stat filestat; // ***stat structure
    
    if (source == NULL || dest < 0) {   //** added file opened control
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
    ret = inflateInit2(&strm, 16+MAX_WBITS); // gzip file
    if (ret != Z_OK)
        return ret;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            fclose(source);
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;
        progress += strm.avail_in; // *** increment progress
        printf("\rExtracting (%d%%)...", (progress * 100) / totalsize); //*** show progression
        fflush(stdout);

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
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                fclose(source);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (write(dest, out, have) != have) {
                (void)inflateEnd(&strm);
                fclose(source);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    fclose(source);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
/* gzip ends */

int     _fd[2];

void Expand(char *filename) {
  int (*tar_inflate) (FILE *source, int dest);
  int ret;
  if (strstr(filename,".bz2"))  // da correggere
    tar_inflate = &inf_bz;
  else
    tar_inflate = &inf;
  ret = (*tar_inflate)(fopen(filename, "rb"), _fd[1]);
  close(_fd[1]);
  puts("");
  if (ret) puts("error");
  exit(0); 
}

/* tar follows */
  struct raw_tar tar;
  pid_t   childpid;

void ReadBuffer(int fd, char *buffer, int size) {
  static int check_zero;
  int charsno = read(fd, buffer, size);
  if (charsno == size) return; // all ok
  if (charsno == 0) {
    if (check_zero++ > 500) return; // unexpected end of file?
  }
  else check_zero = 0; // reset counter
  if (charsno < 0) {exit(100);} // error
  if (charsno < size) { // incomplete buffer, recurse...
    nanosleep((struct timespec[]){{0, 500000}}, NULL);
    ReadBuffer(fd, &buffer[charsno], size - charsno);
  }
}

void Untar(char *filename) {
  int fd;
  long int len = 0, work, mod;
  char *buf;

  pipe(_fd);
  
  if ((childpid = fork()) == -1) {
    perror("fork");
    exit(1);
  }
  if (childpid == 0) {
    /* Child process closes up input side of pipe */
    close(_fd[0]);
    Expand(filename);
  }
  
  close(_fd[1]);
  fd = _fd[0];
  
  for (;;) {
    ReadBuffer(fd, (char *) &tar, sizeof(tar));
    if (!tar.name[0]) break;
    len = strtol(tar.size, NULL, 8);
    //sscanf(tar.size, "%o", &len);
    work = len;
    mod = work % 512;
    if (work && mod) work += 512 - mod;
    if (work) {
      buf = calloc(1, work + 1);
      ReadBuffer(fd, buf, work);
      free(buf);
    }
    //printf("%s\t%c\n", tar.name, tar.type[0]);
  }
  close(fd);
  waitpid(childpid, NULL, 0);
}
/* tar ends */
/*
int main(int argc, char **argv) {

  if (argc < 2) exit(5);
  
  Untar(argv[1]);
  
  return 0;
}
*/