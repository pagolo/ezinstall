/* 7zMain.c - Test application for 7z Decoder
2015-08-02 : Igor Pavlov : Public domain */

#include "precomp.h"

#include "7zip/7z.h"
#include "7zip/7zAlloc.h"
#include "7zip/7zBuf.h"
#include "7zip/7zCrc.h"
#include "7zip/7zFile.h"
#include "7zip/7zVersion.h"

#ifndef USE_WINDOWS_FILE
/* for mkdir */
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <errno.h>
#endif
#endif

static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static int Buf_EnsureSize(CBuf *dest, size_t size)
{
  if (dest->size >= size)
    return 1;
  Buf_Free(dest, &g_Alloc);
  return Buf_Create(dest, size, &g_Alloc);
}

#ifndef _WIN32
#define _USE_UTF8
#endif

/* #define _USE_UTF8 */

#ifdef _USE_UTF8

#define _UTF8_START(n) (0x100 - (1 << (7 - (n))))

#define _UTF8_RANGE(n) (((UInt32)1) << ((n) * 5 + 6))

#define _UTF8_HEAD(n, val) ((Byte)(_UTF8_START(n) + (val >> (6 * (n)))))
#define _UTF8_CHAR(n, val) ((Byte)(0x80 + (((val) >> (6 * (n))) & 0x3F)))

static size_t Utf16_To_Utf8_Calc(const UInt16 *src, const UInt16 *srcLim)
{
  size_t size = 0;
  for (;;)
  {
    UInt32 val;
    if (src == srcLim)
      return size;
    
    size++;
    val = *src++;
   
    if (val < 0x80)
      continue;

    if (val < _UTF8_RANGE(1))
    {
      size++;
      continue;
    }

    if (val >= 0xD800 && val < 0xDC00 && src != srcLim)
    {
      UInt32 c2 = *src;
      if (c2 >= 0xDC00 && c2 < 0xE000)
      {
        src++;
        size += 3;
        continue;
      }
    }

    size += 2;
  }
}

static Byte *Utf16_To_Utf8(Byte *dest, const UInt16 *src, const UInt16 *srcLim)
{
  for (;;)
  {
    UInt32 val;
    if (src == srcLim)
      return dest;
    
    val = *src++;
    
    if (val < 0x80)
    {
      *dest++ = (char)val;
      continue;
    }

    if (val < _UTF8_RANGE(1))
    {
      dest[0] = _UTF8_HEAD(1, val);
      dest[1] = _UTF8_CHAR(0, val);
      dest += 2;
      continue;
    }

    if (val >= 0xD800 && val < 0xDC00 && src != srcLim)
    {
      UInt32 c2 = *src;
      if (c2 >= 0xDC00 && c2 < 0xE000)
      {
        src++;
        val = (((val - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
        dest[0] = _UTF8_HEAD(3, val);
        dest[1] = _UTF8_CHAR(2, val);
        dest[2] = _UTF8_CHAR(1, val);
        dest[3] = _UTF8_CHAR(0, val);
        dest += 4;
        continue;
      }
    }
    
    dest[0] = _UTF8_HEAD(2, val);
    dest[1] = _UTF8_CHAR(1, val);
    dest[2] = _UTF8_CHAR(0, val);
    dest += 3;
  }
}

static SRes Utf16_To_Utf8Buf(CBuf *dest, const UInt16 *src, size_t srcLen)
{
  size_t destLen = Utf16_To_Utf8_Calc(src, src + srcLen);
  destLen += 1;
  if (!Buf_EnsureSize(dest, destLen))
    return SZ_ERROR_MEM;
  *Utf16_To_Utf8(dest->data, src, src + srcLen) = 0;
  return SZ_OK;
}

#endif

static SRes Utf16_To_Char(CBuf *buf, const UInt16 *s
    #ifndef _USE_UTF8
    , UINT codePage
    #endif
    )
{
  unsigned len = 0;
  for (len = 0; s[len] != 0; len++);

  #ifndef _USE_UTF8
  {
    unsigned size = len * 3 + 100;
    if (!Buf_EnsureSize(buf, size))
      return SZ_ERROR_MEM;
    {
      buf->data[0] = 0;
      if (len != 0)
      {
        char defaultChar = '_';
        BOOL defUsed;
        unsigned numChars = 0;
        numChars = WideCharToMultiByte(codePage, 0, s, len, (char *)buf->data, size, &defaultChar, &defUsed);
        if (numChars == 0 || numChars >= size)
          return SZ_ERROR_FAIL;
        buf->data[numChars] = 0;
      }
      return SZ_OK;
    }
  }
  #else
  return Utf16_To_Utf8Buf(buf, s, len);
  #endif
}

#ifdef _WIN32
  #ifndef USE_WINDOWS_FILE
    static UINT g_FileCodePage = CP_ACP;
  #endif
  #define MY_FILE_CODE_PAGE_PARAM ,g_FileCodePage
#else
  #define MY_FILE_CODE_PAGE_PARAM
#endif

static WRes MyCreateDir(const UInt16 *name, char **subdir, Bool *do_save)
{
  #ifdef USE_WINDOWS_FILE
  
  return CreateDirectoryW(name, NULL) ? 0 : GetLastError();
  
  #else

  CBuf buf;
  WRes res;
  char *work_name =NULL;
  Buf_Init(&buf);
  RINOK(Utf16_To_Char(&buf, name MY_FILE_CODE_PAGE_PARAM));
  work_name = (char *)buf.data;

  if (*do_save) {
    if (*subdir && strstr(work_name, *subdir) == work_name)
      work_name = &work_name[strlen(*subdir) + 1];
  } else {
    if (strcmp(basename(work_name), globaldata.gd_inidata->archive_dir) == 0) {
      *subdir = strdup(work_name);
      *do_save = True;
    }
    Buf_Free(&buf, &g_Alloc);
    return 0;
  }
  
  res =
  #ifdef _WIN32
  _mkdir((const char *)work_name)
  #else
  mkdir((const char *)work_name, 0755) // orig 0777
  #endif
  == 0 ? 0 : errno;
  Buf_Free(&buf, &g_Alloc);
  return res;
  
  #endif
}

static WRes OutFile_OpenUtf16(CSzFile *p, const UInt16 *name, char **subdir, Bool *do_save)
{
  #ifdef USE_WINDOWS_FILE
  return OutFile_OpenW(p, name);
  #else
  CBuf buf;
  WRes res;
  char *work_name;
  
  Buf_Init(&buf);
  RINOK(Utf16_To_Char(&buf, name MY_FILE_CODE_PAGE_PARAM));
  work_name = (char *)buf.data;

  int is_normal = (*do_save && (*subdir == NULL || strncmp(work_name, *subdir, strlen(*subdir)) == 0));
  if (is_normal || FileIsSQL(work_name)) {
    if (is_normal) {
      if (*subdir) work_name = &work_name[strlen(*subdir) + 1];
    } else {
      work_name = basename(work_name);
    }
  }

  res = OutFile_Open(p, (const char *)work_name);
  Buf_Free(&buf, &g_Alloc);
  return res;
  #endif
}

void PrintError(char *sz)
{
  printf("\nERROR: %s\n", sz);
}

// #define NUM_PARENTS_MAX 128

int Unseven(const char *filename, STRING **list)
{
  CFileInStream archiveStream;
  CLookToRead lookStream;
  CSzArEx db;
  SRes res;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  UInt16 *temp = NULL;
  size_t tempSize = 0;
  Bool do_save = (globaldata.gd_inidata->archive_dir && *(globaldata.gd_inidata->archive_dir))? False : True;
  char *subdir = NULL;
  // UInt32 parents[NUM_PARENTS_MAX];

//  printf("\n7z ANSI-C Decoder " MY_VERSION_COPYRIGHT_DATE "\n\n");

  if (!filename || !*filename) return -1; // error

  #if defined(_WIN32) && !defined(USE_WINDOWS_FILE) && !defined(UNDER_CE)
  g_FileCodePage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
  #endif

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;

  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  #ifdef UNDER_CE
  if (InFile_OpenW(&archiveStream.file, L"\test.7z"))
  #else
  if (InFile_Open(&archiveStream.file, filename))
  #endif
  {
    PrintError("can not open input file");
    return -1;
  }

  FileInStream_CreateVTable(&archiveStream);
  LookToRead_CreateVTable(&lookStream, False);
  
  lookStream.realStream = &archiveStream.s;
  LookToRead_Init(&lookStream);

  CrcGenerateTable();

  SzArEx_Init(&db);
  
  res = SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp);
  
    if (res == SZ_OK)
    {
      UInt32 i;

      /*
      if you need cache, use these 3 variables.
      if you use external function, you can make these variable as static.
      */
      UInt32 blockIndex = 0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */
      Byte *outBuffer = 0; /* it must be 0 before first call for each new archive. */
      size_t outBufferSize = 0;  /* it can have any value before first call (if outBuffer = 0) */

      for (i = 0; i < db.NumFiles; i++)
      {
        size_t offset = 0;
        size_t outSizeProcessed = 0;
        // const CSzFileItem *f = db.Files + i;
        size_t len;
        unsigned isDir = SzArEx_IsDir(&db, i);
        len = SzArEx_GetFileNameUtf16(&db, i, NULL);
        // len = SzArEx_GetFullNameLen(&db, i);

        if (len > tempSize)
        {
          SzFree(NULL, temp);
          tempSize = len;
          temp = (UInt16 *)SzAlloc(NULL, tempSize * sizeof(temp[0]));
          if (!temp)
          {
            res = SZ_ERROR_MEM;
            break;
          }
        }

        SzArEx_GetFileNameUtf16(&db, i, temp);

    { // progression output
      char *s = mysprintf(_("Extracting (%d%%)"), ((i + 1) * 100) / db.NumFiles);
      HandleSemaphoreText(s, list, !i ? 1 : 0);
      if (s) free(s);
    }
        if (isDir)
          ;//printf("/");
        else
        {
          res = SzArEx_Extract(&db, &lookStream.s, i,
              &blockIndex, &outBuffer, &outBufferSize,
              &offset, &outSizeProcessed,
              &allocImp, &allocTempImp);
          if (res != SZ_OK)
            break;
        }

        // write data to disk        
        {
          CSzFile outFile;
          size_t processedSize;
          size_t j;
          UInt16 *name = (UInt16 *)temp;
          const UInt16 *destPath = (const UInt16 *)name;
 
          for (j = 0; name[j] != 0; j++)
            if (name[j] == '/')
            {
              name[j] = 0;
              MyCreateDir(name, &subdir, &do_save);
              name[j] = CHAR_PATH_SEPARATOR;
            }
    
          if (isDir)
          {
            MyCreateDir(destPath, &subdir, &do_save);
            //printf("\n");
            continue;
          }
          if (do_save == False) continue;
          if (OutFile_OpenUtf16(&outFile, destPath, &subdir, &do_save))
          {
            PrintError("can not open output file");
            res = SZ_ERROR_FAIL;
            break;
          }

          processedSize = outSizeProcessed;
          
          if (File_Write(&outFile, outBuffer + offset, &processedSize) != 0 || processedSize != outSizeProcessed)
          {
            PrintError(_("can not write output file"));
            res = SZ_ERROR_FAIL;
            break;
          }
          
          if (File_Close(&outFile))
          {
            PrintError(_("can not close output file"));
            res = SZ_ERROR_FAIL;
            break;
          }
          
          #ifdef USE_WINDOWS_FILE
          if (SzBitWithVals_Check(&db.Attribs, i))
            SetFileAttributesW(destPath, db.Attribs.Vals[i]);
          #endif
        }
        //printf("\n");
      }
      IAlloc_Free(&allocImp, outBuffer);
    }

  SzArEx_Free(&db, &allocImp);
  SzFree(NULL, temp);

  File_Close(&archiveStream.file);
  
  if (res == SZ_OK)
  {
    //printf("\nEverything is Ok\n");
    return 0;
  }
  
  if (res == SZ_ERROR_UNSUPPORTED)
    PrintError(_("decoder doesn't support this archive"));
  else if (res == SZ_ERROR_MEM)
    PrintError(_("can not allocate memory"));
  else if (res == SZ_ERROR_CRC)
    PrintError(_("CRC error"));
  else
    printf("<br />ERROR #%d\n", res);
  
  return -1;
}

