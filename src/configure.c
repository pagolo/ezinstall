#include "main.h"

/**** GLOBAL CONFIGURATION ****/

void setMainConfig(char *section, char *name, char *value) {
  static USER *user = NULL;
  static MYSQLDATA *mysql = NULL;
    
  if (strcasecmp(section, "main") == 0) {
    if (strcasecmp(name, "language") == 0 && value && *value) {
      globaldata.gd_locale_code = strdup(value);
    } else if (strcasecmp(name, "locale_path") == 0 && value && *value) {
      globaldata.gd_locale_path = strdup(value);
    } else if (strcasecmp(name, "static_path") == 0 && value && *value) {
      globaldata.gd_static_path = strdup(value);
    } else if (strcasecmp(name, "loglevel") == 0 && value && *value) {
      globaldata.gd_loglevel = atoi(value);
    } else if (strcasecmp(name, "php_sapi") == 0 && value && *value) {
      globaldata.gd_php_sapi = strdup(value);
      if (strstr(value, "cgi") == NULL)
        globaldata.gd_php_is_cgi = 0;
      else
        globaldata.gd_php_is_cgi = 1;
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

  parseMainConfig();

  // ...check if important fields are filled
  if (!(globaldata.gd_userdata) ||
          !(globaldata.gd_userdata->username) ||
          !(globaldata.gd_userdata->password)) {
    Error(_("Configuration error, admin data..."));
  }
  if (!(globaldata.gd_mysql) ||
          !(globaldata.gd_mysql->username) ||
          !(globaldata.gd_mysql->host)) {
    Error(_("Configuration error, mysql data..."));
  }
  if (globaldata.gd_mysql->password == NULL)
    globaldata.gd_mysql->password = "";
  
  // per ora hard coded, poi si vedrà
  globaldata.gd_session_timeout = (time_t)1200;

  return 1;
}

int GetInputUserData(char **id, char **pass) {
  *id = getfieldbyname("_main_username");
  *pass = getfieldbyname("_main_password");
  if (*id && *pass) return 1;
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
  char *username = NULL, *pass;
  int logged = 0;
  char *session_name_ip = getcookie(COOKIE_NAME);

  CreateLogPath();
  ReadGlobalConfig();

  if (session_name_ip && *session_name_ip) {
    username = checksession(session_name_ip, (action == ACTION_EXIT));
  }

  if (action == ACTION_EXIT) {
    putcookie(COOKIE_NAME, "", -1L, NULL, NULL, 0);
    return 0;
  }

  if (username && strcmp(globaldata.gd_userdata->username, username) == 0) {
    logged = 1;
  }
  else {
    //logged = 1;
    logged = GetInputUserData(&username, &pass);
    if (logged) logged = AreDataOk(username, pass, globaldata.gd_userdata->username, globaldata.gd_userdata->password);
  }
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
    if (globaldata.gd_session == NULL)
      createsession(globaldata.gd_userdata->username);
    mycode = mysprintf("%s %s", globaldata.gd_session, getenv("REMOTE_ADDR"));
    putcookie(COOKIE_NAME, mycode, time(NULL) + 1200, getenv("SCRIPT_NAME"), getenv("SERVER_NAME"), 0);
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
