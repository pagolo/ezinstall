#ifndef __MAIN_H__
#define __MAIN_H__

// editable defines
#define COOKIE_NAME     "ezinstall"
#define CONFIG_NAME     "ezinstall.xml"
#define CONFIG_NAME_ROOT "../../ezinstall.xml"
#define LOG_NAME        "ezinstall.log"
#define TEMP_MARK       "<ezinstall_temporary_file />\r\n"

#include "../config.h"
#include "conditions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>
#include <memory.h>
#include <unistd.h>
#include <ctype.h>
#include <libintl.h>
#include <langinfo.h>
#include <dirent.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <libgen.h>

#ifndef SOCKET
#define SOCKET int
#endif
#define closesocket    close

// Actions
enum {
   ACTION_START=-1,
   DOWNLOAD_CONFIG,
   UPLOAD_CONFIG,
   CREATE_FOLDER,
   RENAME_FOLDER,
   MYSQL_FORM,
   CREATE_DB,
   EDIT_CONFIG,
   SAVE_CONFIG,
   CALL_SCRIPT,
   ACTION_TEST=100,
   ACTION_UPLOAD,
   ACTION_DOWNLOAD,
   ACTION_DELTEMP,
   ACTION_EDITCONF,
   ACTION_SAVECONF,
   ACTION_EXIT
};

typedef struct UserData {
   char *username;
   char *password;
} USER;

#include "utils.h"

typedef struct MySqlData {
   char *username;
   char *password;
   char *host;
   char *db_name;
   STRING *db_files;
} MYSQLDATA;

typedef struct FileSysObject {
   char *file;
   int   isfolder;
   struct FileSysObject *next;
} FSOBJ;

typedef struct ChMod {
   char *file;
   int  permissions;
   int  createfolder;
   struct ChMod *next;
} CHMOD;

typedef struct PhpConf {
   char *varname;
   char *varvalue;
   int  isset;
   char *message;
   struct PhpConf *next;
} PHPCONF;

enum {
   _CREATEDIR = 1,
   _SKIP_MYSQL = 2,
   _SKIP_CONFIGFILE = 4
};

enum {
  _VARIABLES,
  _DEFINES,
  _ARRAY
};

typedef struct IniData {
   int  flags;          // flags, vedi sopra
   int  data_mode;      // config data mode (_VARIABLES,_DEFINES,_ARRAY)
   char *directory;     // nome cartella di default
   char *web_archive;   // archivio da scaricare via http
   char *unzip;         // formato archivio
   FSOBJ *filesys_list; // lista di cartelle o file da creare
   CHMOD *perm_list;    // lista delle cartelle su cui cambiare i permessi
   CHMOD *perm_list_rec;// lista delle cartelle su cui cambiare i permessi ricorsivamente
   char *php_conf_name; // nome del file di conf. php ORIGINE
   char *php_conf_save; // nome del file di conf. php DA SALVARE
   PHPCONF *php_conf_list; // lista delle variabili, con rispettivi valori
   char *script2start;  // script da lanciare una volta finito
   char *finalmessage;  // messaggio finale per l'utente
} INIDATA;

// Log levels
enum {
   LOG_NONE,
   LOG_1
};

typedef struct GlobalData {
   USER *gd_userdata;  // nome utente loggato
   MYSQLDATA *gd_mysql;// dati per uso mysql
   char *gd_iniaddress;// indirizzo web del file da scaricare
   char *gd_inifile;   // nome file ini scaricato
   INIDATA *gd_inidata;// configurazione da file ini scaricato
   char *gd_error;     // stringa di errore
   char *gd_locale_code;//codice tipo it_IT.UTF8
   char *gd_locale_path;//codice tipo it_IT.UTF8
   int  gd_header_sent;// intestazione inviata?
   int  gd_config_root;// configurazione nella cartella superiore della superiore (../../)
   int  gd_loglevel;   // livello di log da utilizzare
   char *gd_logpath;   // nome completo di path del file di log
   char *gd_session;   // nome del file di sessione
   time_t gd_session_timeout; // timeout di sessione (20 minuti)
} GLOBAL;

extern GLOBAL globaldata;

void Error (char *msg);
void ChDirRoot(void);

#include "socket.h"
#include "login.h"
#include "session.h"
#include "configure.h"
#include "mylocale.h"
#include "getinput.h"
#include "mysql.h"
#include "editconfig.h"
#include "test.h"
#include "xml.h"
#include "md5.h"

#endif
