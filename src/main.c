#include "main.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <errno.h>

#define HTM_HEADER "Content-type: text/html\r\n\r\n<html>\n<head>\n<title>Easy Installer Tool</title>\n<link rel=\"stylesheet\" href=\"%s/ezinstall.css\" type=\"text/css\"/>\n<script type=\"text/javascript\" src=\"%s/ezinstall.js\"></script>\n</head>\n<body>\n<h1>Easy Installer Tool</h1>\n<div class='rule'><hr /></div>&nbsp;<br />\n"
#define HTM_HEADER_CLIENT "Content-type: text/html\r\n\r\n"
#define HTM_FOOTER "</body></html>\n"

//#define DEBUG

GLOBAL globaldata;

void Error(char *msg) {
  if (!(globaldata.gd_header_sent)) printf(HTM_HEADER, globaldata.gd_static_path, globaldata.gd_static_path);
  printf("<br /><div class='error_title'>%s</div>: ", _("ERROR"));
  printf("%s<br />", msg);
  if (globaldata.gd_loglevel > LOG_NONE) WriteLog(msg);
  if (globaldata.gd_error) printf(globaldata.gd_error);
  printf("<br /><a href='javascript:history.back()'>%s</a><br />", _("BACK"));
  printf(HTM_FOOTER);
  xmlCleanupParser();
  if (globaldata.gd_semaphore && !globaldata.gd_semaphore->sem_keep)
    EndSemaphore();
  exit(5);
}

void DaemonError(char *msg, STRING **list) {
  char *s1 = mysprintf("<em>%s</em>: %s", _("ERROR"), msg);
  HandleSemaphoreText(s1, list, 1);
  EndSemaphoreTextError();
  if (globaldata.gd_loglevel > LOG_NONE) WriteLog(msg);
  while (globaldata.gd_semaphore->sem_buffer[0] != '*') sleep(1);
  xmlCleanupParser();
  EndSemaphore();
  exit(5);
}

void CreateFileSystemObject(STRING **list) {
  int rc;
  FSOBJ *object;
  for (object = globaldata.gd_inidata->filesys_list; object != NULL; object = object->next) {
    if (object->isfolder) {
      char *s = mysprintf(_("Creating folder &quot;%s&quot;..."), object->file);
      HandleSemaphoreText(s, list, 1);
      if (s) free(s);
      rc = mkdir(object->file, 0755);
      if (rc == -1) DaemonError(_("Can't create folder"), list);
    } else {
      char *s = mysprintf(_("Creating file &quot;%s&quot;...<br />"), object->file);
      HandleSemaphoreText(s, list, 1);
      if (s) free(s);
      rc = creat(object->file, 0644);
      if (rc == -1) DaemonError(_("Can't create file"), list);
      else close(rc);
    }
  }
}

enum {
  __CREATE, __RENAME
};

void CreateChangeDir(char *dirname, STRING **list, int dir_rename) {
  int rc;
  char *root = getenv("DOCUMENT_ROOT");

  if (root == NULL) rc = -1;
  else rc = chdir(root);
  if (rc == -1) DaemonError(_("Can't chdir to document_root"), list);

  if (dir_rename == __RENAME) {
    if (dirname == NULL || strcmp(globaldata.gd_inidata->directory, dirname) == 0) {
      HandleSemaphoreText(_("No need to rename folder...<br />"), list, 1);
    } else {
      char *s = mysprintf(_("Renaming folder &quot;%s&quot; to &quot;%s&quot;...<br />"), globaldata.gd_inidata->directory, dirname);
      HandleSemaphoreText(s, list, 1);
      if (s) free(s);
      rc = rename(globaldata.gd_inidata->directory, dirname);
      if (rc == -1) DaemonError(_("Can't rename project folder"), list);
    }
  } else {
    char *s = mysprintf(_("Creating folder &quot;%s&quot;..."), dirname);
    HandleSemaphoreText(s, list, 1);
    if (s) free(s);
    rc = mkdir(dirname, 0755);
    if (rc == -1) DaemonError(_("Can't create project folder"), list);
  }

  rc = chdir(dirname);
  if (rc == -1) DaemonError(_("Can't chdir to project folder"), list);
}

int DownloadExtractArchiveFile(STRING **list) {
  int rc, free_filename = 0;
  char *filename;
  int (*Uncompress) (const char *filename, STRING **list);

  if (is_upload()) {
    filename = globaldata.gd_inidata->web_archive;
  } else {
    HandleSemaphoreText(_("Downloading archive...<br />"), list, 1);
    rc = graburl_list(globaldata.gd_inidata->web_archive, 0644, 0, 0, list);
    if (rc == 0) DaemonError(_("Can't download the script archive"), list);
    filename = basename(globaldata.gd_inidata->web_archive);
  }

  HandleSemaphoreText(_("Uncompressing archive...<br />"), list, 1);
  
  // read file in document_root
  if (!(strchr(filename, SLASH))) {
    char *path = getenv("DOCUMENT_ROOT");
    if (path && *path) {
      path = append_cstring(NULL, path);
      if (path[strlen(path) - 1] != '/') path = append_cstring(path, "/");
      filename = append_cstring(path, filename);
      free_filename = 1;
    }
  }

#ifdef WITH_7ZIP
  extern int Unseven(const char *filename, STRING **list);
#endif

  switch (globaldata.gd_inidata->zip_format) {
    case PKZIP:
      Uncompress = &Unzip;
      break;
#ifdef WITH_7ZIP
    case SEVENZIP:
      Uncompress = &Unseven;
      break;
#endif
    case GZ_TAR:
    case Z_TAR:
    case BZ2_TAR:
      Uncompress = &Untar;
      break;
    default:
      globaldata.gd_inidata->zip_format = UNKNOWN_COMP_FORMAT;
      break;
  }
  
  if (globaldata.gd_inidata->zip_format != UNKNOWN_COMP_FORMAT) {
    if ((*Uncompress)(filename, list) == 0) {
      if (globaldata.gd_loglevel > LOG_NONE)
        WriteLog(_("archive files extracted"));
      unlink(filename);
    } else {
      DaemonError(_("Error extracting archive files"), list);
    }
  } else {
    DaemonError(_("Unknown archive format"), list);
  }
  
  if (free_filename && filename) free(filename);
          
  return 1;
}

char *login_string = "<form name='myform' method=\"POST\" action=\"%s?%s\">\n"
        "<table id=\"login_table\">\n"
        "<tr>\n"
        "<td class='login_table_label'>%s:</td>\n"
        "<td><input type='text' name='_main_username' size='29'></td>\n"
        "</tr>\n"
        "<tr>\n"
        "<td class='login_table_label'>%s:</td>\n"
        "<td><input type='password' name='_main_password' size='29'></td>\n"
        "</tr>\n"
        "<tr>\n"
        "<td colspan='2' class='submit_row'><input id='continue' type='submit' value=\"%s\" name='B1'><input type='reset' value=\"%s\" name='B2'></td>\n"
        "</tr>\n"
        "</table>\n"
        "</form>\n"
        "<script language='javascript'>document.myform._main_username.focus();</script>\n";

void ShowLoginPage(int action) {
  printf(login_string, getenv("SCRIPT_NAME"), action == ACTION_EXIT ? "" : getenv("QUERY_STRING"),
          _("Username"),
          _("Password"),
          _("Submit"),
          _("Clear"));
}

char *createdir_string = "<form method=\"POST\" action=\"%s?%d\">\n"
        "<input type='hidden' name='inifile' value=\"%s\">\n"
        "<input type='hidden' name='webarchive' value=\"%s\">\n"
        "<input type='hidden' name='overwrite' value=\"%s\">\n"
        "<input type='hidden' name='upload' value=\"%s\">\n"
        "<table id='createdir_table'>\n"
        "<tr>\n"
        "<td>%s%s</td>\n"
        "</tr>\n"
        "<tr>\n"
        "<td><input type='text' name='folder' %s value='%s' size='32'></td>\n"
        "</tr>\n"
        "<tr>\n"
        "<td class='submit_row_onecol'><input id='continue' type='submit' value='%s' name='B1'></td>\n"
        "</tr>\n"
        "</table>\n"
        "</form>";

void ShowCreateDirPage(void) {
  printf(createdir_string, getenv("SCRIPT_NAME"),
          CREATE_FOLDER,
          globaldata.gd_inifile,
          globaldata.gd_inidata->web_archive,
          getfieldbyname("overwrite"),
          getfieldbyname("upload"),
          globaldata.gd_inidata->dir_msg? globaldata.gd_inidata->dir_msg : _("Please edit the destination web folder name"),
          ":",
          "",
          globaldata.gd_inidata->directory,
          _("Continue")
          );
}

void ShowRenameDirPage(void) {
  char *dir = globaldata.gd_inidata->directory;
  int dont_show_edit = dir[0] == '.' && (dir[1] == '\0' || (dir[1] == '/' && dir[2] == '\0'));
  printf(createdir_string, getenv("SCRIPT_NAME"),
          RENAME_FOLDER,
          globaldata.gd_inifile,
          globaldata.gd_inidata->web_archive,
          getfieldbyname("overwrite"),
          getfieldbyname("upload"),
          dont_show_edit ? "" : _("Please edit the destination web folder name"),
          dont_show_edit ? "" : ":",
          dont_show_edit ? "style='display: none'" : "",
          dir,
          _("Continue")
          );
}

void ShowArchive(void) {
  if (globaldata.gd_inidata && globaldata.gd_inidata->web_archive)
    printf(_("<strong>Installing <em>%s</em></strong><br />&nbsp;<br />"), basename(globaldata.gd_inidata->web_archive));
}

void ChDirRoot(void) {
  int rc;
  char *root = getenv("DOCUMENT_ROOT");

  rc = chdir(root);
  if (rc == -1) Error(_("Can't chdir to document_root"));
}

char *nextstep_string = "<br /><form method='POST' action='%s?%d'>\n"
        "<input type='hidden' name='inifile' value=\"%s\">\n"
        "<input type='hidden' name='folder' value=\"%s\">\n"
        "<input type='hidden' name='database' value=\"%s\">\n"
        "<input type='hidden' name='webarchive' value=\"%s\">\n\n"
        "<input id='continue' type='submit' value=\"%s\" name='B1'>\n"
        "</form>";

void NextStep(int step) {
  char *database = getfieldbyname("database");
  if ((globaldata.gd_inidata->flags & _SKIP_MYSQL) && step <= EDIT_CONFIG)
    step = EDIT_CONFIG;
  if ((globaldata.gd_inidata->flags & _SKIP_CONFIGFILE) && step <= CALL_SCRIPT && step >= EDIT_CONFIG)
    step = CALL_SCRIPT;
  if (step == CALL_SCRIPT && globaldata.gd_inidata->finalmessage)
    printf("<br />%s<br />\n", globaldata.gd_inidata->finalmessage);
  printf(nextstep_string, getenv("SCRIPT_NAME"),
          step,
          globaldata.gd_inifile,
          getfieldbyname("folder"),
          database ? database : "",
          globaldata.gd_inidata->web_archive,
          _("Continue")
          );
}

void LaunchScript(void) {
  char *folder = getfieldbyname("folder");
  if (folder) {
    int len = strlen(folder);
    if (len > 0 && folder[len - 1] == '/')
      folder[len - 1] = '\0';
  }
  printf(_("Please wait..."));
  printf("<div class='throbber'>&nbsp;</div><script language='javascript'>window.location=\"/%s/%s\";</script>\n",
          folder,
          globaldata.gd_inidata->script2start);
}

char *upload_string =
        "<form id=\"upload_form\" method='POST' action=\"%s?%d\" enctype=\"multipart/form-data\">\n"
        "<input type='hidden' name='upload' value=\"1\">\n"
        "<input type='hidden' name='inifile' value=\"\">\n"
        "<input type='hidden' name='zipfile' value=\"\">\n"
        "<table id='upload_table'>\n<tr><td class='upload_cell'>\n"
        "%s</td>\n"
        "<td><input type='file' id='ini' name='ini' size='40' /></td>\n"
        "<td id='ini_sent'></td></tr>\n"
        "<tr><td class='upload_cell'>%s</td>\n"
        "<td><input type='file' id='zip' name='zip' disabled='disabled' size='40'></td>\n"
        "<td id='zip_sent'></td></tr>\n"
        "<tr><td class='upload_cell'>%s</td>\n"
        "<td><input type='checkbox' name='overwrite' checked='checked'></td><td></td></tr>\n"
        "<tr><td colspan='3'><div id=\"progress_bar\"><div id=\"percent\" class=\"percent\">0%</div></div></td></tr>\n"
        "<tr><td colspan='3' class='submit_row'><hr />\n"
        "<input id='submit' type='submit' value=\"%s\" style='display:none' name='B1'>"
        "<input id='reset' type='reset' value=\"%s\" style='display:none' name='B2'>"
        "<input id='continue' type='submit' value='%s' disabled='disabled' name='B3'>"
        "</td></tr>\n"
        "</table></form>\n"
        "<script type=\"text/javascript\">InitAjax('%s','%s',%s);</script>\n";

void UploadForm(void) {
  printf(upload_string, getenv("SCRIPT_NAME"),
          UPLOAD_CONFIG,
          _("configuration file"),
          _("archive file"),
          _("overwrite file"),
          _("Submit"),
          _("Clear"),
          _("Continue"),
          getenv("SCRIPT_NAME"),
          _("File sent..."),
#ifdef WITH_7ZIP
          "true"
#else
          "false"
#endif
          );
}

char *download_string =
        "<form method=\"POST\" action=\"%s?%d\" enctype=\"multipart/form-data\">\n"
        "<input type='hidden' name='upload' value=\"0\">\n"
        "<table id='download_table'>\n"
        "<tr id='down_row'>\n"
        "<td><label for='ini_download'>%s:</label><br /><input id='ini_download' type='text' name='url' value=\"http://\" size='64'></td>\n"
        "</tr>\n"
        "<tr><td><input id='overwrite' type='checkbox' name='overwrite' checked='checked' /><label for='overwrite'>%s</label></td></tr>\n"
        "<tr><td><input id='toggle' type='checkbox' name='toggle' onchange='toggle_upload(this)' /><label for='toggle'>%s</label></td></tr>\n"
        "<tr id='up_row' style='display:none'><td><input type='file' id='ini_upload' name='ini' disabled='disabled' accept='text/xml' size='40' /></td></tr>\n"
        "<tr>\n"
        "<td class='submit_row_onecol'><br /><input type='submit' value=\"%s\" name='B1'><input type='reset' value=\"%s\" name='B2'></td>\n"
        "</tr>\n"
        "</table>\n"
        "</form>";

void DownloadForm(void) {
  printf(download_string, getenv("SCRIPT_NAME"),
          DOWNLOAD_CONFIG,
          _("Please insert url of xml configuration file"),
          _("overwrite file"),
          _("upload xml configuration file"),
          _("Submit"),
          _("Clear"));
}

char *script_string =
        "\n<script type=\"text/javascript\">\n"
        "function CheckUp(theform) {\n"
        "if (theform.admin_name.value==\"\") {\n"
        "  alert(\"%s\");\n"
        "  theform.admin_name.focus();\n"
        "  return false;\n"
        "  }\n"
        "if (theform.admin_pass1.value!=\"\" && theform.admin_pass1.value.length < 6) {\n"
        "  alert(\"%s\");\n"
        "  theform.admin_pass1.focus();\n"
        "  return false;\n"
        "  }\n"
        "if (theform.admin_pass1.value!=theform.admin_pass2.value) {\n"
        "  alert(\"%s\");\n"
        "  theform.admin_pass1.focus();\n"
        "  return false;\n"
        "  }\n"
        "if (theform.mysql_user.value==\"\") {\n"
        "  alert(\"%s\");\n"
        "  theform.mysql_user.focus();\n"
        "  return false;\n"
        "  }\n"
        "if (theform.mysql_pass1.value==\"\") {\n"
        "  alert(\"%s\");\n"
        "  theform.mysql_pass1.focus();\n"
        "  return false;\n"
        "  }\n"
        "if (theform.mysql_pass1.value!=theform.mysql_pass2.value) {\n"
        "  alert(\"%s\");\n"
        "  theform.mysql_pass1.focus();\n"
        "  return false;\n"
        "  }\n"
        "return true;\n"
        "}\n"
        "</script>\n";

char *config_string =
        "<form name=\"Config\" method=\"POST\" action=\"%s?saveconf\" onSubmit=\"return CheckUp(this)\">\n"
        "<fieldset>\n"
        "<legend>%s</legend>\n"
        "<table>\n"
        "<tr><td>\n"
        "<small>%s</small></td>\n"
        "<td>\n"
        "<input name=\"admin_name\" type=\"text\" id=\"admin_name\" value=\"%s\" />\n"
        "</td><td colspan=\"2\">&nbsp;</td>\n</tr>\n<tr><td>\n"
        "<small>%s</small></td>\n"
        "<td><input name=\"admin_pass1\" type=\"password\" id=\"admin_pass1\" value=\"%s\" /></td>\n"
        "<td><small>%s</small></td>\n"
        "<td><input name=\"admin_pass2\" type=\"password\" id=\"admin_pass2\" value=\"%s\" /></td></tr>\n"
        "</table>\n"
        "</fieldset><br />&nbsp;<br />\n"
        "<fieldset>\n"
        "<legend>%s</legend>\n"
        "&nbsp;<br />%s %s %s<br />&nbsp;\n"
        "</fieldset><br />&nbsp;<br />\n"
        "<fieldset>\n"
        "<legend>%s</legend>\n"
        "<table>\n"
        "<tr><td>\n"
        "<small>%s</small></td>\n"
        "<td>\n"
        "<input name=\"mysql_user\" type=\"text\" value=\"%s\" />\n"
        "</td><td colspan=\"2\">&nbsp;</td>\n</tr>\n<tr><td>\n"
        "<small>%s</small></td>\n"
        "<td><input name=\"mysql_pass1\" type=\"password\" value=\"%s\" /></td>\n"
        "<td><small>%s</small></td>\n"
        "<td><input name=\"mysql_pass2\" type=\"password\" value=\"%s\" /></td></tr>\n"
        "<tr><td><small>%s</small></td>\n"
        "<td><input name=\"mysql_host\" type=\"text\" value=\"%s\" /></td>\n"
        "<td><small>%s</small></td>\n"
        "<td><input name=\"mysql_db\" type=\"text\" value=\"%s\" /></td></tr>\n"
        "</table>\n"
        "</fieldset>\n"
        "<p>\n"
        "<input type=\"submit\" name=\"button\" value=\"%s\" />\n"
        "</p>\n"
        "</form>\n";

char *InsertLanguages(void) {
  char *result, *old;
  LOCALELIST *list = get_locales_all();

  result = mysprintf("<small>%s</small>\n", _("Language"));
  result = append_cstring(result, "<select name=\"Language\">\n");
  for (; list; list = list->next) {
    old = result;
    result = mysprintf("%s<option %s value='%s'>%s</option>\n", old, (strcmp(list->locale_code, globaldata.gd_locale_code) == 0) ? "selected=\"selected\"" : "", list->locale_code, list->language_name);
    free(old);
  }
  result = append_cstring(result, "</select>\n");
  return result;
}

char *InsertLogLevels(void) {
  char *result, *old;

  result = mysprintf("<small>%s</small>\n<select name=\"LogLevel\">\n", _("LogLevel"));
  old = result;
  result = mysprintf("%s<option %s value=\"0\">%s</option>\n", old, (globaldata.gd_loglevel == 0) ? "selected=\"selected\"" : "", _("None"));
  free(old);
  old = result;
  result = mysprintf("%s<option %s value=\"1\">%s</option>\n", old, (globaldata.gd_loglevel == 1) ? "selected=\"selected\"" : "", _("Level 1"));
  free(old);
  old = result;
  result = mysprintf("%s<option %s value=\"2\">%s</option>\n", old, (globaldata.gd_loglevel == 2) ? "selected=\"selected\"" : "", _("Level 2"));
  free(old);

  result = append_cstring(result, "</select>\n");
  return result;
}

char *InsertStaticDataPath(void) {
  char *result = mysprintf("<small>%s</small>\n<input type='text' name=\"StaticData\" value='%s' />\n", _("Static data path"), globaldata.gd_static_path);
  return result;
}
void ConfigForm(void) {

  printf(
          script_string,
          _("Please insert a username for the administrator"),
          _("The administrator password must be at least six chars long"),
          _("Administrator password and confirmation don't match. Please try again."),
          _("Please insert a username for the mysql user"),
          _("Please insert a password for the mysql user"),
          _("Mysql user password and confirmation don't match. Please try again.")
          );

  printf(
          config_string,
          getenv("SCRIPT_NAME"),
          _("Administrator"),
          _("username"),
          globaldata.gd_userdata->username,
          _("password"),
          "",
          _("password (repeat)"),
          "",
          _("Localization & Log & Static Data Path"),
          InsertLanguages(),
          InsertLogLevels(),
          InsertStaticDataPath(),
          _("MySQL"),
          _("username"),
          globaldata.gd_mysql->username,
          _("password"),
          globaldata.gd_mysql->password,
          _("password (repeat)"),
          globaldata.gd_mysql->password,
          _("hostname"),
          globaldata.gd_mysql->host,
          _("default database"),
          globaldata.gd_mysql->db_name,
          _("Save")
          );
}

int ReadAction(int argc, char **argv) {
  if (argc <= 1) return ACTION_START;
  if (isdigit(argv[1][0])) return atoi(argv[1]);
  if (strcasecmp("test", argv[1]) == 0) return ACTION_TEST;
  if (strcasecmp("upload", argv[1]) == 0) return ACTION_UPLOAD;
  if (strcasecmp("download", argv[1]) == 0) return ACTION_DOWNLOAD;
  if (strcasecmp("deltemp", argv[1]) == 0) return ACTION_DELTEMP;
  if (strcasecmp("editconf", argv[1]) == 0) return ACTION_EDITCONF;
  if (strcasecmp("saveconf", argv[1]) == 0) return ACTION_SAVECONF;
  if (strcasecmp("exit", argv[1]) == 0) return ACTION_EXIT;
  if (strstr(argv[1], "client") == argv[1]) {
    // TODO: semaphore initialization
    return SEMAPHORE_CLIENT;
  } 
  return ACTION_START;
}

void GetStartPath(void) {
  char *path = malloc(PATH_SIZE);
  if (!path) return;
  if (!(getcwd(path, PATH_SIZE))) return;
  globaldata.gd_start_path = path;
}

int semaphore_client_main(int argc, char **argv) {
//  union semun arg;
  int semid;
  key_t key = (key_t)atoi(argv[2]);
  int shmid;
  char *text;

  printf(HTM_HEADER_CLIENT);

  semid = semget(key, 1, 0);
#ifdef DEBUG
  if (globaldata.gd_loglevel > LOG_1) WriteLog(mysprintf("semget=%d",semid));
#endif
  if(semid == -1)
    {
      //printf(_("client:unable to execute semaphore"));
    //printf("%s<br />", strerror(errno));
    printf("*");
    if (globaldata.gd_loglevel > LOG_1) WriteLog(_("- client: unable to execute semaphore"));
    else sleep(1);
      exit(-1);
    }
  shmid = shmget(key,SHARED_MEM_SIZE,0666);
  if(shmid<0)
    {
      printf(_("client:failure in shmget"));
      //semctl(semid, 0, IPC_RMID, arg);
       exit(-1);
    }
  text = shmat(shmid,NULL,0);
  globaldata.gd_header_sent = 1;
  semv_wait(semid);
  printf(text);
  if (strstr(text, SEMAPHORE_END)) {
    strcpy(text, "*");
#ifdef DEBUG
    if (globaldata.gd_loglevel > LOG_1) WriteLog("- SEMAPHORE_END");
    else sleep(1);
#endif
  }
  semv_post(semid);
  //    semctl(semid, 0, IPC_RMID, arg);
  //shmctl(shmid, IPC_RMID, 0);
  exit(0);
}

void SemaphorePrepare() {
  StartSemaphore();
  globaldata.gd_semaphore->sem_keep = 1;
  printf("<div id='from_semaphore'></div>&nbsp;<br />\n");  
}

void Daemonize(void) {
  STRING *list = NULL;
  int i;
  if (getppid() == 1) return; /* already a daemon */
  i = fork();
  if (i < 0) exit(1); /* fork error */
  if (i > 0) return; /* parent exits */
  /* child (daemon) continues */
  setsid(); /* obtain a new process group */
  for (i = getdtablesize(); i >= 0; --i) close(i); /* close all descriptors */
  i = open("/dev/null", O_RDWR);
  dup(i);
  dup(i); /* handle standart I/O */
  //WriteLog(mysprintf("daemon stdin file descriptor = %d", i));
  umask(0022); /* set newly created file permissions */
  
  switch (globaldata.gd_action) {
    case UPLOAD_CONFIG:
    case DOWNLOAD_CONFIG:
      DownloadExtractArchiveFile(&list);
      break;
    case CREATE_FOLDER:
    case RENAME_FOLDER:
      {
        char *mainfolder = getfieldbyname("folder");
        if (globaldata.gd_action == CREATE_FOLDER) {
          if (strcmp("./", mainfolder) && strcmp(".", mainfolder))
            CreateChangeDir(mainfolder, &list, __CREATE);
          else
            ChDirRoot();
          DownloadExtractArchiveFile(&list);
        } else {
          CreateChangeDir(mainfolder, &list, __RENAME);
        }
      }
      CreateFileSystemObject(&list);
      ChangePermissionsRecurse(&list);
      ChangePermissions(&list);
      break;
    case CREATE_DB:
      CreateDbTables(&list);
      if (globaldata.gd_loglevel > LOG_NONE)
        WriteLog(_("MySQL setup"));
      break;
  }
  
#ifdef DEBUG
union semun arg;
int sem_id = semget(globaldata.gd_semaphore->sem_key, 1, 0666 | IPC_CREAT);
WriteLog(mysprintf("semid_test=%d"));
semctl(sem_id, 0, IPC_RMID, arg);
sem_id = semget(globaldata.gd_semaphore->sem_key, 1, 0666 | IPC_CREAT);
WriteLog(mysprintf("semid_test2=%d"));
semctl(sem_id, 0, IPC_RMID, arg);
#endif
  EndSemaphoreText();
  while (globaldata.gd_semaphore->sem_buffer[0] != '*') sleep(1);
  //semv_wait(globaldata.gd_semaphore->sem_id);
  EndSemaphore();
  freestringlist(list);
  exit(0);
}

int ajax_config_upload_main(int argc, char **argv) {
  printf("%s%s", HTM_HEADER_CLIENT, get_ini_upload(1));
  exit(0);  
}

int ajax_archive_upload_main(int argc, char **argv) {
  globaldata.gd_inifile = getfieldbyname("inifile");
  read_xml_file(UPLOAD_CONFIG); // @TODO: gestire gli errori in modo ajax compatibile
  chdir(getenv("DOCUMENT_ROOT"));
  printf("%s%s", HTM_HEADER_CLIENT, get_zip_upload(1));
  exit(0);  
}

int main(int argc, char **argv) {
  int action, logged, rc;
  char *ld;
  char *error_read = _("Error reading xml file");
  static char absolute_path[512];

  LIBXML_TEST_VERSION
  
  GetStartPath();

  globaldata.gd_action = action = ReadAction(argc, argv);

  logged = init(action);
  ld = globaldata.gd_locale_path;
  if (ld && *ld)
    realpath(ld, absolute_path);
  setlocale(LC_ALL, globaldata.gd_locale_code ? globaldata.gd_locale_code : "");
  bindtextdomain(PACKAGE, ld && *ld ? absolute_path : MYLOCALEDIR);
  textdomain(PACKAGE);

  if (action >= SEMAPHORE_CLIENT) {
    if (!(logged)) {
      printf("%s%s", HTM_HEADER_CLIENT, _("Access forbidden"));
      exit(5);
    }
    if (action == SEMAPHORE_CLIENT)
      semaphore_client_main(argc, argv);
    if (action == AJAX_CONFIG_UPLOAD)
      ajax_config_upload_main(argc, argv);
    if (action == AJAX_ARCHIVE_UPLOAD)
      ajax_archive_upload_main(argc, argv);
  }

  if (globaldata.gd_static_path && *globaldata.gd_static_path) {
    if (globaldata.gd_static_path[strlen(globaldata.gd_static_path)-1] == '/')
      globaldata.gd_static_path[strlen(globaldata.gd_static_path)-1] = '\0';
  }
  printf(HTM_HEADER, globaldata.gd_static_path, globaldata.gd_static_path);
  globaldata.gd_header_sent = 1;

  if (!(logged)) {
    ShowLoginPage(action);
    printf(HTM_FOOTER);
    return 0;
  }

  switch (action) {
    case ACTION_START:
    {
      char *script = getenv("SCRIPT_NAME");
      printf("| <a href=%s?test>%s</a> | <a href=%s?download>%s</a> | <a href=%s?upload>%s</a> | <a onClick=\"if (confirm('%s')) return true; return false;\" href=%s?exit>%s</a> |",
              script, _("Configuration test"),
              script, _("Download & install"),
              script, _("Upload & install"),
              _("Are you sure you want to exit?"),
              script, _("Exit"));
    }
      break;
    case UPLOAD_CONFIG:
      globaldata.gd_inifile = getfieldbyname("inifile");
      if (!(globaldata.gd_inifile) || !(globaldata.gd_inifile[0]))
        if (!(globaldata.gd_inifile = get_ini_upload(0)))
          Error(_("can't access configuration file"));
      rc = read_xml_file(action);
      if (rc == 0) Error(error_read);
      {
        char *zipfile = getfieldbyname("zipfile");
        if (!(zipfile && *zipfile)) {
          char *result = get_zip_upload(0);
          if (strcmp("ok", result) != 0)
            Error(result);
        } else if (globaldata.gd_inidata && globaldata.gd_inidata->web_archive) {
          free(globaldata.gd_inidata->web_archive);
          globaldata.gd_inidata->web_archive = strdup(zipfile);
        }
      }
    case DOWNLOAD_CONFIG:
      if (action == DOWNLOAD_CONFIG) {
        char *toggle = getfieldbyname("toggle");
        int uploaded_ini = (toggle && strcasecmp(toggle, "on") == 0) ? 1 : 0;
        if (uploaded_ini) {
          if (!(globaldata.gd_inifile = get_ini_upload(0)))
            Error(_("can't access configuration file"));
        }
        else {
          if (!(globaldata.gd_iniaddress = get_ini_name(argc, argv)))
            Error(_("ini file name not specified"));
          rc = graburl(globaldata.gd_iniaddress, 0644, 0, 1);
          if (rc == 0) Error(_("Can't download ini file"));
        }
        rc = read_xml_file(action);
        if (rc == 0) Error(error_read);
      }
      ShowArchive();
      // se c'è da creare la cartella...
      if (globaldata.gd_inidata->flags & _CREATEDIR) {
        ShowCreateDirPage();
      }        // altrimenti scaricare il file...
      else {
        ChDirRoot();
        //exit(100);
        SemaphorePrepare();
        Daemonize();
        ShowRenameDirPage();
      }
      break;
    case CREATE_FOLDER:
    case RENAME_FOLDER:
      globaldata.gd_inifile = getfieldbyname("inifile");
      rc = read_xml_file(action);
      if (rc == 0) Error(error_read);
      globaldata.gd_inidata->web_archive = getfieldbyname("webarchive");
      ShowArchive();
      SemaphorePrepare();
      Daemonize();
      NextStep(MYSQL_FORM);
      break;
    case MYSQL_FORM:
      globaldata.gd_inifile = getfieldbyname("inifile");
      rc = read_xml_file(action);
      if (rc == 0) Error(error_read);
      globaldata.gd_inidata->web_archive = getfieldbyname("webarchive");
      ShowArchive();
      MySqlForm();
      break;
    case CREATE_DB:
      globaldata.gd_inifile = getfieldbyname("inifile");
      rc = read_xml_file(action);
      if (rc == 0) Error(error_read);
      globaldata.gd_inidata->web_archive = getfieldbyname("webarchive");
      globaldata.gd_mysql->db_name = getfieldbyname("database");
      ShowArchive();
      SemaphorePrepare();
      Daemonize();
      NextStep(EDIT_CONFIG);
      break;
    case EDIT_CONFIG:
    case SAVE_CONFIG:
      globaldata.gd_inifile = getfieldbyname("inifile");
      rc = read_xml_file(action);
      if (rc == 0) Error(error_read);
      globaldata.gd_inidata->web_archive = getfieldbyname("webarchive");
      globaldata.gd_mysql->db_name = getfieldbyname("database");
      ShowArchive();
      if (action == EDIT_CONFIG)
        EditConfigForm();
      else {
        SaveConfigFile();
        if (globaldata.gd_loglevel > LOG_NONE)
          WriteLog(_("Config file saved"));
        NextStep(CALL_SCRIPT);
      }
      break;
    case CALL_SCRIPT:
      globaldata.gd_inifile = getfieldbyname("inifile");
      rc = read_xml_file(action);
      if (rc == 0) Error(error_read);
      unlink(globaldata.gd_inifile);
      LaunchScript();
      break;
    case ACTION_DELTEMP:
      DeleteTemp();
    case ACTION_SAVECONF:
    case ACTION_TEST:
      printf("<em>%s</em>", _("Configuration test"));
      printf(" | <a href=\"%s\">%s</a><br /><br />", getenv("SCRIPT_NAME"), _("Home page"));
      DoTest();
      break;
    case ACTION_DOWNLOAD:
      printf("<em>%s</em>", _("Download & install"));
      printf(" | <a href=\"%s\">%s</a><br /><br />", getenv("SCRIPT_NAME"), _("Home page"));
      DownloadForm();
      break;
    case ACTION_UPLOAD:
      printf("<em>%s</em>", _("Upload & install"));
      printf(" | <a href=\"%s\">%s</a><br /><br />", getenv("SCRIPT_NAME"), _("Home page"));
      UploadForm();
      break;
    case ACTION_EDITCONF:
      printf("<em>%s</em>", _("Edit configuration"));
      printf(" | <a href=\"%s\">%s</a><br /><br />", getenv("SCRIPT_NAME"), _("Home page"));
      ConfigForm();
      break;
    default:
      break;
  }

  // do ajax script
  if (globaldata.gd_semaphore && globaldata.gd_semaphore->sem_keep)
    printf("<script type=\"text/javascript\">do_ajax('client',%d,'%s',new Date().getTime())</script>\n",
          (int)globaldata.gd_semaphore->sem_key,
          getenv("SCRIPT_NAME"));

  printf(HTM_FOOTER);

  xmlCleanupParser();
  
  if (globaldata.gd_semaphore && !globaldata.gd_semaphore->sem_keep)
    EndSemaphore();

  return 0;
}
