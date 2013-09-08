#include "main.h"

PHPCONF *GetVar(char *s) {
  PHPCONF *var;
  char *z = &s[1], back;
  while (!(isspace(*z)) && *z != '=') ++z;
  back = *z;
  *z = '\0';
  for (var = globaldata.gd_inidata->php_conf_list; var != NULL; var = var->next) {
    //      printf("%s==%s\n",s,var->varname);
    if (strcmp(s, var->varname) == 0) break;
  }
  *z = back;

  return var;
}

PHPCONF *GetDefine(char *s) {
  PHPCONF *var;
  if (strncmp(s, "define", 6) != 0)
    return NULL;
  char *z = &s[6], q, back;
  while (isspace(*z)) ++z;
  if (*z == '(') ++z;
  else return NULL; // ?
  q = *z;
  if (q != '\'' && q != '\"')
    return NULL;
  s = ++z;
  z = strchr(s, q);
  if (z == NULL)
    return NULL;
  back = *z;
  *z = '\0';
  for (var = globaldata.gd_inidata->php_conf_list; var != NULL; var = var->next) {
    if (strcmp(s, var->varname) == 0) break;
  }
  *z = back;
  return var;
}

STRING *ParsePhp(char *line) {
  STRING *result = NULL;
  static enum PhpState state;
  char *ptr, *start, *prev, *string = NULL, z, quote = '\'';
  PHPCONF *var = NULL;

  if (state != IN_COMMENT_STD) state = IN_CODE;

  for (ptr = start = prev = line; *ptr != '\0'; ptr++) {
    switch (*ptr) {
      case '\'':
      case '\"':
        if (state == IN_CODE) {
          state = IN_STRING;
          quote = *ptr;
          string = &ptr[1];
        } else if (state == IN_STRING && *ptr == quote) {
          state = IN_CODE;
          if (globaldata.gd_inidata->data_mode != _ARRAY)
            break;
          *ptr = '\0';
          string = strdup(string);
          *ptr = quote;
          for (var = globaldata.gd_inidata->php_conf_list; var != NULL; var = var->next) {
            if (strcmp(string, var->varname) == 0) break;
          }
          if (string) {
            free(string);
            string = NULL;
          }
          if (var) {
            ++ptr;
            while (isspace(*ptr)) ++ptr; // skip spaces
            if (ptr[0] != '=' || ptr[1] != '>') { // not an assign
              --ptr;
              break;
            }
            ++ptr;
            ++ptr;
            while (isspace(*ptr)) ++ptr; // skip spaces
            if (*ptr != '\'' && *ptr != '\"') break;
            quote = *ptr;
            z = ptr[1];
            ptr[1] = '\0'; // backup next char  terminate string
            appendstring(&result, start);
            ++ptr;
            *ptr = z; // restore previous value
            appendstring(&result, var->varvalue); // insert correct value!
            //printf("*3*");
            while (*ptr != quote) ++ptr; // skip default value;
            start = ptr;
            break;
          }
        }
        break;
      case '#':
        if (state == IN_CODE)
          state = IN_COMMENT_PLUS;
        break;
      case '/':
        if (prev == ptr) break;
        if (*prev == '/' && state == IN_CODE)
          state = IN_COMMENT_PLUS;
        if (*prev == '*' && state == IN_COMMENT_STD)
          state = IN_CODE;
        break;
      case '*':
        if (prev == ptr) break;
        if (*prev == '/' && state == IN_CODE)
          state = IN_COMMENT_STD;
        break;
      case 'd': // search for define
        if (state != IN_CODE) break; // comment, string, ...
        if (globaldata.gd_inidata->data_mode != _DEFINES) break;
        if (!(var = GetDefine(ptr))) break; // not my var
        *ptr = '\0';
        appendstring(&result, start);
        char *def = mysprintf("define('%s', '%s')", var->varname, var->varvalue);
        appendstring(&result, def);
        start = strchr(&ptr[1], ';');
        break;
      case '$':
        if (state != IN_CODE) break; // comment, string, ...
        if (globaldata.gd_inidata->data_mode != _VARIABLES) break;
        //printf("*1*");
        if (!(var = GetVar(ptr))) break; // not my var
        ++ptr; // skip initial $
        //printf("*1-1*");
        while (!(isspace(*ptr)) && *ptr != '=') ++ptr; // skip variable name
        while (isspace(*ptr)) ++ptr; // skip spaces
        if (*ptr != '=') { // not an assign
          --ptr;
          break;
        }
        ++ptr;
        //printf("*2*");
        while (isspace(*ptr)) ++ptr; // skip spaces
        if (*ptr != '\'' && *ptr != '\"') break;
        quote = *ptr;
        z = ptr[1];
        ptr[1] = '\0'; // backup next char  terminate string
        appendstring(&result, start);
        ++ptr;
        *ptr = z; // restore previous value
        appendstring(&result, var->varvalue); // insert correct value!
        //printf("*3*");
        while (*ptr != quote) ++ptr; // skip default value;
        start = ptr;
        break;
    }
    prev = ptr;
  }

  if (start) appendstring(&result, start);

  return result;
}

void OutputConfigFile(void) {
  char buffer[512];
  FILE *fh = fopen(globaldata.gd_inidata->php_conf_name, "r");

  if (!(fh)) Error(_("can't open php configuration file"));

  printf("<TextArea cols='90' rows='20' name=configfile>\n");

  while (fgets(buffer, 512, fh)) {
    STRING *s = ParsePhp(buffer);
    STRING *t = s;
    while (t) {
      printf("%s", t->string);
      t = t->next;
    }
    freestringlist(s);
  }

  printf("</TextArea>\n");

  fclose(fh);
}

void EditConfigForm(void) {
  int rc;
  printf(_("<br />Please check php configuration file<br />\n"));

  ChDirRoot();
  rc = chdir(getfieldbyname("folder"));
  if (rc == -1) Error(_( "Can't chdir to project folder"));

  printf("\n<form method=POST action=\"%s?%d\">\n",
          getenv("SCRIPT_NAME"),
          SAVE_CONFIG);

  printf("<input type=hidden name=inifile value=\"%s\">\n"
          "<input type=hidden name=folder value=\"%s\">\n"
          "<input type=hidden name=database value=\"%s\">\n"
          "<input type=hidden name=webarchive value=\"%s\">\n",
          globaldata.gd_inifile,
          getfieldbyname("folder"),
          getfieldbyname("database"),
          globaldata.gd_inidata->web_archive);

  OutputConfigFile();

  printf("<tr><td colspan=2><br />\n<input type=submit value=\"%s\" name=B1><input type=reset value=\"%s\" name=B2></td></tr>\n",
          _( "Continue"),
          _( "Clear"));
  printf("</form>\n");

}

void SaveConfigFile(void) {
  char *filebuf = getfieldbyname_sanitize("configfile", 0);
  FILE *fh;
  int rc;

  ChDirRoot();
  rc = chdir(getfieldbyname("folder"));
  if (rc == -1) Error(_( "Can't chdir to project folder"));

  fh = fopen(globaldata.gd_inidata->php_conf_save, "w");
  if (!(fh)) Error(_("can't save configuration file"));
  fwrite(filebuf, strlen(filebuf), 1, fh);
  fclose(fh);
  printf(_("<br />Configuration file has been saved..."));
}
