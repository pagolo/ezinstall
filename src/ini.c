/*
** This file is placed into the public domain on February 22, 1996,  by
** its author: Carey Bloodworth
**
** Modifications:
**
** 07-Dec-1999 by
** Bob Stout & Jon Guthrie
**  General cleanup, use NUL (in SNIPTYPE.H) instead of NULL where appropriate.
**  Allow spaces in tag names.
**  Allow strings in quotes.
**  Allow trailing comments.
**  Allow embedded comment separator(s) in quoted strings.
**  Add comments.
**  ReadCfg() now returns the number of variables found if no error occurred.
**  Changed integer type to short.
**  Use cant() calls in lieu of fopen() calls,
**    include ERRORS.H for prototype.
**  Fix parsing error with null string data.
**
**  09-Jan-2006 ported to unix, by P. Bozzo
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "ini.h"

#define BUFFERSIZE (INI_LINESIZE + 2)
#define cant fopen
#define NUL '\0'
#define True_ 1
#define False_ 0
#define Boolean_T int
#define LAST_CHAR(a) a[strlen(a)-1]

enum LineTypes {
      EmptyLine = 0, CommentLine = 1, InSection = 2, NotInSection = 3,
      NewSection = 4, FoundSection = 5, LeavingSection = 6
};

/*
**  StripLeadingSpaces() - Strips leading spaces from a string.
**
**  Paramters: 1 - String.
**
**  Returns: Pointer to first non-whitespace character in the string.
**
**  Note: This does not modify the original string.
*/

static char *StripLeadingSpaces(char *string)
{
      if (!string || (0 == strlen(string)))
            return NULL;

      while ((*string) && (isspace(*string)))
            string++;
      return string;
}

/*
**  StripTrailingSpaces() - Strips traling spaces from a string.
**
**  Paramters: 1 - String.
**
**  Returns: Pointer to the string.
**
**  Note: This does modify the original string.
*/

static char *StripTrailingSpaces(char *string)
{
      if (!string || (0 == strlen(string)))
            return NULL;

      while (isspace(LAST_CHAR(string)))
            LAST_CHAR(string) = NUL;

      return string;
}

/*
**  ReadLine() - Read a line from the active file
**
**  Paramters: 1 - File pointer of the active file.
**             2 - Pointer to the line's storage.
**
**  Returns: Length of line or EOF.
**
**  Side effects: Strips newline characters (DOS, Unix, or Mac).
*/

static int ReadLine(FILE *fp, char *line)
{
      char *cp;

      cp = fgets(line, INI_LINESIZE, fp);

      if (NULL == cp)
      {
            *line = 0;
            return EOF;
      }
      if (feof(fp))
      {
            *line = 0;
            return EOF;
      }
      if (0 != ferror(fp))
      {
            *line = 0;
            return EOF;
      }

      /*
      **  Allow both DOS and Unix style newlines.
      */

      while (strlen(line) && strchr("\n\r", LAST_CHAR(line)))
            LAST_CHAR(line) = NUL;

      return strlen(line);
}

/*
**  StrEq() - Case-insensitive string compare.
**
**  Paramters: 1 - First string.
**             2 - second string.
**
**  Returns: True_ if strings match, else False_.
*/

static Boolean_T StrEq(char *s1, char *s2)
{
      while (tolower(*s1) == tolower(*s2))
      {
            if (NUL == *s1)
                  return True_;
            s1++;
            s2++;
      }
      return False_;
}

/*
**  ParseLine() - This routine divides the line into two parts. The
**                variable, and the 'string'.
**
**  Paramters: 1 - The line to parse:
**             2 - Pointer to variable name storage.
**             3 = Pointer to textual data storage.
**
**  Returns: Nothing.
*/

static void IniParseLine(char *line, char *var, char *data)
{
      int len = 0;

      line = StripLeadingSpaces(line);
      strcpy(var, line);
      strcpy(data, "");

      while (*line)
      {
            if (/*isspace(*line) || */ ('=' == *line))
            {
                  var[len] = 0;
                  var  = StripTrailingSpaces(var);
                  line = StripLeadingSpaces(line+1);

                  /* Remove a possible '=' */
                  /* This could allow var==string in some cases. No big deal*/

                  while (line && '=' == *line)
                        line = StripLeadingSpaces(line+1);
                  if (line)
                  {
                        strcpy(data, line);
                        StripTrailingSpaces(data);
                  }
                  return;
            }
            else
            {
                  line++;
                  len++;
            }
      }
}

/*
**  SectionLine() - This routine checks each line of the file and identifies
**                  only those lines that are within the right section.
**
**  Paramters: 1 - The line to scan.
**             2 - The specific section wanted.
**             3 - The current section name.
**
**  Returns: Line type.
*/

static enum LineTypes SectionLine(char *line,
                                  char *SectionWanted,
                                  char *CurrentSection)
{
      enum LineTypes linetype;

      line = StripLeadingSpaces(line);
      if (!line || NUL == *line)
            return EmptyLine;

      /*
      ** Comments are started with a "%", ";" or "#"
      */

      if (';' == *line)
            return CommentLine;
      if ('%' == *line)
            return CommentLine;
      if ('#' == *line)
            return CommentLine;

      if ('[' == line[0])  /* Section Header */
      {
            linetype = NewSection;
            if (StrEq(CurrentSection, SectionWanted))
                  linetype = LeavingSection;

            strcpy(CurrentSection, line);
            if (StrEq(line, SectionWanted))
                  linetype = FoundSection;
      }
      else
      {            /* Just a regular line */
            linetype = NotInSection;
            if (StrEq(CurrentSection, SectionWanted))
                  linetype = InSection;
      }

      return linetype;
}

#ifndef EZINSTALL

/*
**  ReadCfg() - Reads a .ini / .cfg file. May read multiple lines within the
**              specified section.
**
**  Paramters: 1 - File name
**             2 - Section name
**             3 - Array of CfgStruct pointers
**
**  Returns: Number of variables located
**           -1 if any type spec failed
**           -2 for any type of file error
*/

int ReadCfg(const char *FileName, char *SectionName, struct CfgStruct *MyVars)
{
      FILE *CfgFile;
      static char line[BUFFERSIZE];
      static char SectionWanted[BUFFERSIZE];
      static char CurrentSection[BUFFERSIZE];
      enum LineTypes linetype;
      static char var[BUFFERSIZE];
      static char data[BUFFERSIZE], *dp;
      int retval = 0;
      struct CfgStruct *mv;

      CfgFile = cant((char *)FileName, "r");
      if (CfgFile==NULL) return 0;

      strcpy(CurrentSection, "[]");
      sprintf(SectionWanted, "[%s]", SectionName);

      while (EOF != ReadLine(CfgFile, line))
      {
            linetype = SectionLine(line, SectionWanted, CurrentSection);
            switch (linetype)
            {
            case EmptyLine:
                  break;  /* Nothing to parse */

            case CommentLine:
                  break;  /* Nothing to parse */

            case InSection:              /* In our section, parse it. */
            {
                  IniParseLine(line, var, data);

                  for (mv = MyVars; mv->Name; ++mv)
                  {
                        if (StrEq(mv->Name, var))
                        {
                              switch (mv->VarType)
                              {
                              case Cfg_String:
                                    if ('\"' == *data)
                                    {
                                          dp = data + 1;
                                          data[strlen(data)-1] = NUL;
                                    }
                                    else  dp = data;
                                    /*
                                    ** Use sprintf to assure embedded
                                    ** escape sequences are handled.
                                    ** DISABILITATO CREA PROBLEMI.
                                    */
                                    //sprintf(mv->DataPtr, dp);
                                    //INVECE...
                                    strcpy(mv->DataPtr,dp);
                                    ++retval;
                                    break;

                              case Cfg_Byte:
                                    *((unsigned char*)mv->DataPtr) =
                                          (unsigned char)atoi(data);
                                    ++retval;
                                    break;

                              case Cfg_Ushort:
                                    *((unsigned int*)mv->DataPtr) =
                                          (unsigned int)atoi(data);
                                    ++retval;
                                    break;

                              case Cfg_Short:
                                    *((int*)mv->DataPtr) = atoi(data);
                                    ++retval;
                                    break;

                              case Cfg_Ulong:
                                    *((unsigned long*)mv->DataPtr) =
                                          (unsigned long)atol(data);
                                    ++retval;
                                    break;

                              case Cfg_Long:
                                    *((long*)mv->DataPtr) = atol(data);
                                    ++retval;
                                    break;

                              case Cfg_Double:
                                    *((double*)mv->DataPtr) = atof(data);
                                    ++retval;
                                    break;

                              case Cfg_Boolean:
                                    *((int*)mv->DataPtr) = 0;
                                    data[0] = tolower(data[0]);
                                    if (('y' == data[0]) || ('t' == data[0]))
                                          *((int*)mv->DataPtr) = 1;
                                    ++retval;
                                    break;

                              case Cfg_I_Array:
                              {
                                    int *ip;
                                    char *str;

                                    ip = ((int*)mv->DataPtr);
                                    str = strtok(data, " ,\t");
                                    while (NULL != str)
                                    {
                                          *ip = atoi(str);
                                          ip++;
                                          str = strtok(NULL, " ,\t");
                                    }
                                    ++retval;
                                    break;
                              }

                              default:
#ifdef TEST
                                    printf("Unknown conversion type\n");
#endif
                                    retval = -1;
                                    break;
                              }
                        }
                        if (-1 == retval)
                              break;
                  };

                  /*
                  ** Variable wasn't found.  If we don't want it,
                  ** then ignore it
                  */
            }
            case NotInSection:
                  break;  /* Not interested in this line */

            case NewSection:
                  break;  /* Who cares? It's not our section */

            case FoundSection:
                  break;  /* We found our section! */

            case LeavingSection:
                  break;  /* We finished our section! */
            }

            if (-1 == retval)
                  break;
      }

      if (ferror(CfgFile))
      {
            fclose(CfgFile);
            return -2;
      }
      else
      {
            fclose(CfgFile);
            return retval;
      }
}

/*
**  SearchCfg() - Search an .ini / .cfg file for a specific single datum.
**
**  Parameters: 1 - File name.
**              2 - Section name.
**              3 - Variable to find.
**              4 - Pointer to variable's storage.
**              5 - Type of vatiable.
**
**  Returns:  1 if succesful
**            0 if section/variable not found
**           -1 if invalid type spec
**           -2 for file error
*/

int SearchCfg(const char *FileName,
              char *SectionName,
              char *VarName,
              void *DataPtr,
              enum CfgTypes VarType
             )
{
      static struct CfgStruct MyVars[2];

      MyVars[0].Name    = VarName;
      MyVars[0].DataPtr = DataPtr;
      MyVars[0].VarType = VarType;

      MyVars[1].Name = MyVars[1].DataPtr = NULL;

      return ReadCfg(FileName, SectionName, MyVars);
}

#endif

// NEW functions 15-01-2006 P.Bozzo
/*
** GetSectionData() grabs an entire section data as strings
** Fills all structure members
*/
int GetSectionData(const char *FileName, char *SectionName, struct CfgStruct *mv)
{
      FILE *CfgFile;
      static char line[BUFFERSIZE];
      static char SectionWanted[BUFFERSIZE];
      static char CurrentSection[BUFFERSIZE];
      enum LineTypes linetype;
      static char var[BUFFERSIZE];
      static char data[BUFFERSIZE], *dp;
      int retval = 0;
      
      CfgFile = cant((char *)FileName, "r");
      if (CfgFile==NULL) return 0;

      strcpy(CurrentSection, "[]");
      sprintf(SectionWanted, "[%s]", SectionName);

      while (EOF != ReadLine(CfgFile, line))
      {
            linetype = SectionLine(line, SectionWanted, CurrentSection);
            switch (linetype)
            {
            case EmptyLine:
                  break;  /* Nothing to parse */

            case CommentLine:
                  break;  /* Nothing to parse */

            case InSection:              /* In our section, parse it. */
                  if (mv) {
                     IniParseLine(line, var, data);
                     if ('\"' == *data)
                     {
                        dp = data + 1;
                        data[strlen(data)-1] = NUL;
                     }
                     else  dp = data;
                     mv->DataPtr=(void *)strdup(dp);
                     mv->Name=strdup(var);
                     mv->VarType=Cfg_String;
                     ++mv;
                  }
                  ++retval;
                  break;

            case NotInSection:
                  break;  /* Not interested in this line */

            case NewSection:
                  break;  /* Who cares? It's not our section */

            case FoundSection:
                  break;  /* We found our section! */

            case LeavingSection:
                  break;  /* We finished our section! */
            }

            if (-1 == retval)
                  break;
      }

      if (ferror(CfgFile))
      {
            fclose(CfgFile);
            return -2;
      }
      else
      {
            fclose(CfgFile);
            return retval;
      }
}

//
// Dinamically creates array of data
// from an entire section
//
struct CfgStruct *GetSection(const char *FileName, char *SectionName)
{
   struct CfgStruct *mv=NULL;
   int count;
   
   count=GetSectionData(FileName, SectionName, NULL);
   if (!count) return NULL;
   mv=calloc(sizeof(struct CfgStruct),count+1);
   if (!mv) return NULL;
   count=GetSectionData(FileName, SectionName, mv);
   
   return mv;
}

#ifdef EZINSTALL

STRING *GetSectionNames(const char *FileName) {
      FILE *CfgFile;
      STRING *stringlist=NULL;
      static char line[BUFFERSIZE];
      
      CfgFile = cant((char *)FileName, "r");
      if (CfgFile==NULL) return NULL;

      while (EOF != ReadLine(CfgFile, line))
      {
         char *ptr=line;
         if (line[0]=='[') {
            ptr=strchr(line,']');
            if (ptr) {
               *ptr='\0';
               appendstring(&stringlist,&line[1]);
            }
         }
      }

      fclose(CfgFile);
      return stringlist;
}
#endif
