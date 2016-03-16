#ifndef __UNTAR_H
#define __UNTAR_H

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#define BZ_IMPORT
#include "bzlib.h"
#include "zlib.h"

/* bzip follows */
typedef char            Char;
typedef unsigned char   Bool;
typedef unsigned char   UChar;
typedef int             Int32;
typedef unsigned int    UInt32;
typedef short           Int16;
typedef unsigned short  UInt16;
                                       
#define True  ((Bool)1)
#define False ((Bool)0)
#define BZ_SETERR(eee)                    \
{                                         \
   if (bzerror != NULL) *bzerror = eee;   \
}
/* bzip ends */

/* tar follows */
#define CHUNK 0x1000 // 1048576 // 16384

struct raw_tar {
  char  name[100];
  char  mode[8];
  char  owner[8];
  char  group[8];
  char  size[12];
  char  date[12];
  char  sum[8];
  char  type[1];
  char  linked[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char padding[12];
};

typedef struct tar_extension {
  char *path;
  char *linkpath;
  char *atime;
  char *ctime;
} TAR_EXT;

/* tar ends */

int Untar(const char *filename, STRING **list);

#endif
