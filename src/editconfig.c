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

STRING *ParsePhp(char *line) {
  STRING *result = NULL;
  static enum PhpState state;
  char *ptr, *start, *prev, z, quote = '\'';
  PHPCONF *var = NULL;

  if (state != IN_COMMENT_STD) state = IN_CODE;

  for (ptr = start = prev = line; *ptr != '\0'; ptr++) {
    switch (*ptr) {
      case '\'':
      case '\"':
        if (state == IN_CODE) {
          state = IN_STRING;
          quote = *ptr;
        } else if (state == IN_STRING && *ptr == quote) {
          state = IN_CODE;
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
      case '$':
        if (state != IN_CODE) break; // comment, string, ...
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

  appendstring(&result, start);

  return result;
}

void OutputConfigFile(void) {
  char buffer[512];
  FILE *fh = fopen(globaldata.gd_inidata->php_conf_name, "r");

  if (!(fh)) Error(getstr(S_NO_CONF, "can't open php configuration file"));

  printf("<TextArea cols=90 rows=20 name=configfile>\n");

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
  printf(getstr(S_CHECK_CONF, "<br>Please check php configuration file<br>\n"));

  ChDirRoot();
  rc = chdir(getfieldbyname("folder"));
  if (rc == -1) Error(getstr(S_NOCHDIR, "Can't chdir to project folder"));

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

  printf("<tr><td colspan=2><br>\n<input type=submit value=\"%s\" name=B1><input type=reset value=\"%s\" name=B2></td></tr>\n",
          getstr(S_CONTINUE, "Continue"),
          getstr(S_CLEAR, "Clear"));
  printf("</form>\n");

}

void SaveConfigFile(void) {
  char *filebuf = getfieldbyname("configfile");
  FILE *fh;
  int rc;

  ChDirRoot();
  rc = chdir(getfieldbyname("folder"));
  if (rc == -1) Error(getstr(S_NOCHDIR, "Can't chdir to project folder"));

  fh = fopen(globaldata.gd_inidata->php_conf_save, "w");
  if (!(fh)) Error(getstr(S_NO_SAVE_CONF, "can't save configuration file"));
  fwrite(filebuf, strlen(filebuf), 1, fh);
  fclose(fh);
  printf(getstr(S_SAVE_CONF_OK, "<br>Configuration file has been saved..."));
}
