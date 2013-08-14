#include "main.h"

/**** GLOBAL CONFIGURATION ****/

int read_locale_strings(char *language) {
  struct CfgStruct *mv = NULL, *mvptr;
  char *file, **locale;
  int count = 0, i;

  count = GetSectionData(LANG_NAME, language, NULL);
  if (count) file = LANG_NAME;

  if (!count) {
    globaldata.gd_string_no = 0;
    if ((locale = globaldata.gd_locale) != NULL) {
      for (i = 0; locale[i] != NULL; i++)
        free(locale[i]);
      free(locale);
    }
    globaldata.gd_locale = NULL;
    globaldata.gd_language = strdup("English (default)");
    return 0; // no locale strings
  }

  mv = calloc(sizeof (struct CfgStruct), count + 1);
  locale = calloc(sizeof (char *), count + 1);
  if (!mv || !locale) return 0; // no memory?
  count = GetSectionData(file, language, mv);

  for (mvptr = mv; mvptr && mvptr->Name; ++mvptr) {
    i = atoi(mvptr->Name);
    free(mvptr->Name);
    if (i < count && i >= 0) locale[i] = (char *) mvptr->DataPtr;
  }
  free(mv);
  mv = NULL;

  globaldata.gd_string_no = count;
  globaldata.gd_locale = locale;
  globaldata.gd_language = strdup(language);

  return count;
}

void setMainConfig(char *section, char *name, char *value) {
  static USER *user = NULL;
  static MYSQLDATA *mysql = NULL;

  if (strcasecmp(section, "main") == 0) {
    if (strcasecmp(name, "language") == 0 && value && *value) {
      read_locale_strings(value);
    } else if (strcasecmp(name, "loglevel") == 0 && value && *value) {
      globaldata.gd_loglevel = atoi(value);
    }
  } else if (strcasecmp(section, "user") == 0) {
    if (user == NULL) user = globaldata.gd_userdata = calloc(sizeof (USER), 1);
    if (user == NULL) return; // NO MEMORY?
    if (strcasecmp(name, "username") == 0 && value && *value) {
      user->username = strdup(value);
    } else if (strcasecmp(name, "password") == 0 && value && *value) {
      user->password = strdup(value);
    }
  } else if (strcasecmp(section, "mysql") == 0) {
    if (mysql == NULL) mysql = globaldata.gd_mysql = calloc(sizeof (MYSQLDATA), 1);
    if (mysql == NULL) return; // NO MEMORY?
    if (strcasecmp(name, "username") == 0 && value && *value) {
      mysql->username = strdup(value);
    } else if (strcasecmp(name, "password") == 0 && value && *value) {
      mysql->password = strdup(value);
    } else if (strcasecmp(name, "host") == 0 && value && *value) {
      mysql->host = strdup(value);
    } else if (strcasecmp(name, "database") == 0 && value && *value) {
      mysql->db_name = strdup(value);
    }
  }
}

int ReadGlobalConfig(void) {

  parseMainConfig(CONFIG_NAME);

  // ...check if important fields are filled
  if (!(globaldata.gd_userdata) ||
          !(globaldata.gd_userdata->username) ||
          !(globaldata.gd_userdata->password)) {
    Error("Configuration error, admin data...");
  }
  if (!(globaldata.gd_mysql) ||
          !(globaldata.gd_mysql->username) ||
          !(globaldata.gd_mysql->host)) {
    Error("Configuration error, mysql data...");
  }
  if (globaldata.gd_mysql->password == NULL)
    globaldata.gd_mysql->password = "";

  return 1;
}

int GetInputUserData(char **id, char **pass) {
  char *cook = getcookie(COOKIE_NAME);
  char *n = NULL;
  char *p = NULL;

  *id = getfieldbyname("_main_username");
  *pass = getfieldbyname("_main_password");
  if (*id && *pass) return 1;

  if (cook && *cook) n = strdup(cook);
  if (n) p = strchr(n, '|');
  if (p) {
    *p = '\0';
    ++p;
    *id = strdup(n);
    *pass = strdup(p);
    free(n);
    if (*id && *pass) return 1;
  }

  return 0;
}

void CreateLogPath(void) {
  extern char *get_current_dir(void);
  char *path;
  char *slash;
  char *file = LOG_NAME;

  path = get_current_dir();
  if (path[strlen(path) - 1] == '/') slash = "";
  else slash = "/";

  globaldata.gd_logpath = mysprintf("%s%s%s", path, slash, file);
}

int init(int action) {
  char *ip, *pass;
  int logged = 0;

  CreateLogPath();
  ReadGlobalConfig();

  if (action == ACTION_EXIT) {
    putcookie(COOKIE_NAME, "", -1L, NULL, NULL, 0);
    return 0;
  }

  logged = GetInputUserData(&ip, &pass);
  if (logged) logged = AreDataOk(ip, pass, globaldata.gd_userdata->username, globaldata.gd_userdata->password);
  if (!(logged)) {
    // header_out(); implementare???
    // logout
    putcookie(COOKIE_NAME, "", -1L, NULL, NULL, 0);
  } else {
    char *mycode;
    if (action == ACTION_SAVECONF) {
      WriteGlobalConfig();
      ReadGlobalConfig();
    }
    mycode = mysprintf("%s|%s", globaldata.gd_userdata->username, globaldata.gd_userdata->password);
    // header_out(); come sopra.........
    //putcookie(COOKIE_NAME,mycode,time(NULL)+360000,NULL,NULL,0);
    putcookie(COOKIE_NAME, mycode, -1L, NULL, NULL, 0);
    free(mycode);
  }
  return logged;
}

/**** LOCAL CONFIGURATION (downloaded xml file) ****/

// prende un indirizzo web (origin) e cambia il nome di file finale
// (prendendolo da dest)

char *cloneaddress(char *origin, char *dest) {
  char *rc, *ptr, back;

  rc = calloc(strlen(origin) + strlen(dest) + 1, 1);
  if (!(rc)) return NULL;

  ptr = strrchr(origin, '/');
  if (!(ptr)) {
    free(rc);
    return NULL;
  }
  back = ptr[1];
  ptr[1] = '\0';

  sprintf(rc, "%s%s", origin, dest);
  ptr[1] = back;

  return rc;
}

int is_upload(void) {
  char *is = getfieldbyname("upload");
  if (is && *is == '1') return 1;
  return 0;
}

FSOBJ *get_filesys_data(struct CfgStruct *mv) {
  struct CfgStruct *mvptr;
  FSOBJ *object = NULL;
  for (mvptr = mv; mvptr && mvptr->Name; ++mvptr) {
    FSOBJ *tmp;
    FSOBJ *ptr = calloc(sizeof (FSOBJ), 1);
    if (!ptr) break; // no memory
    // fill values
    if (strcasecmp("folder", mvptr->Name) == 0 || strncasecmp("dir", mvptr->Name, 3) == 0)
      ptr->isfolder = 1;
    ptr->file = strdup((const char *) mvptr->DataPtr);
    free(mvptr->DataPtr);
    if (object == NULL) object = ptr;
    else {
      tmp = object;
      while (tmp->next) tmp = tmp->next;
      tmp->next = ptr;
    }
  }
  return object;
}

CHMOD *get_chmod_data(struct CfgStruct *mv) {
  struct CfgStruct *mvptr;
  CHMOD *chmd = NULL;
  for (mvptr = mv; mvptr && mvptr->Name; ++mvptr) {
    char *z;
    CHMOD *tmp;
    CHMOD *ptr = calloc(sizeof (CHMOD), 1);
    if (!ptr) break; // no memory
    ptr->file = mvptr->Name;
    ptr->permissions = strtol((const char *) mvptr->DataPtr, &z, 8);
    free(mvptr->DataPtr);
    if (chmd == NULL) chmd = ptr;
    else {
      tmp = chmd;
      while (tmp->next) tmp = tmp->next;
      tmp->next = ptr;
    }
  }
  return chmd;
}

void SetPhpVar(char *varname, char *varvalue) {
  PHPCONF *var, *ptr;

  if (varname == NULL || *varname == '\0' || varvalue == NULL || *varvalue == '\0')
    return; // do nothing

  var = calloc(sizeof (PHPCONF), 1);
  if (var == NULL) return; // no memory
  var->varname = strdup(varname);
  var->varvalue = strdup(varvalue);

  if (globaldata.gd_inidata->php_conf_list == NULL)
    globaldata.gd_inidata->php_conf_list = var;
  else {
    ptr = globaldata.gd_inidata->php_conf_list;
    while (ptr->next) ptr = ptr->next;
    ptr->next = var;
  }
}

