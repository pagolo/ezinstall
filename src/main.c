#include "main.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#define HTM_HEADER "Content-type: text/html\r\n\r\n<html>\n<head>\n<title>Easy Installer Tool</title>\n<link rel=\"stylesheet\" href=\"/ezinstall.css\" type=\"text/css\"/>\n</head>\n<body>\n<h1>Easy Installer Tool</h1>\n<div class='rule'><hr /></div>&nbsp;<br />\n"
#define HTM_FOOTER "</body></html>\n"

GLOBAL globaldata;

void Error(char *msg) {
  if (!(globaldata.gd_header_sent)) printf(HTM_HEADER);
  printf("<br /><div class='error_title'>%s</div>: ", _("ERROR"));
  printf("%s<br />", msg);
  if (globaldata.gd_loglevel > LOG_NONE) WriteLog(msg);
  if (globaldata.gd_error) printf(globaldata.gd_error);
  printf("<br /><a href='javascript:history.back()'>%s</a><br />", _("BACK"));
  printf(HTM_FOOTER);
  xmlCleanupParser();
  EndSemaphore();
  exit(5);
}

void CreateFileSystemObject(void) {
  int rc;
  FSOBJ *object;
  for (object = globaldata.gd_inidata->filesys_list; object != NULL; object = object->next) {
    if (object->isfolder) {
      printf(_("Creating folder &quot;%s&quot;...<br />"), object->file);
      rc = mkdir(object->file, 0755);
      if (rc == -1) Error(_("Can't create folder"));
    } else {
      printf(_("Creating file &quot;%s&quot;...<br />"), object->file);
      rc = creat(object->file, 0644);
      if (rc == -1) Error(_("Can't create file"));
      else close(rc);
    }
  }
}

enum {
  __CREATE, __RENAME
};

void CreateChangeDir(char *dirname, int dir_rename) {
  int rc;
  char *root = getenv("DOCUMENT_ROOT");

  if (root == NULL) rc = -1;
  else rc = chdir(root);
  if (rc == -1) Error(_("Can't chdir to document_root"));

  if (dir_rename == __RENAME) {
    if (dirname == NULL || strcmp(globaldata.gd_inidata->directory, dirname) == 0) {
      printf(_("No need to rename folder...<br />"));
    } else {
      printf(_("Renaming folder &quot;%s&quot; to &quot;%s&quot;...<br />"), globaldata.gd_inidata->directory, dirname);
      rc = rename(globaldata.gd_inidata->directory, dirname);
      if (rc == -1) Error(_("Can't rename project folder"));
    }
  } else {
    printf(_("<br />Creating folder &quot;%s&quot;...<br />"), dirname);
    rc = mkdir(dirname, 0755);
    if (rc == -1) Error(_("Can't create project folder"));
  }

  rc = chdir(dirname);
  if (rc == -1) Error(_("Can't chdir to project folder"));
}

int DownloadExtractArchiveFile(void) {
  int rc;
  char *command;
  char *filename;

  if (is_upload()) {
    filename = globaldata.gd_inidata->web_archive;
  } else {
    printf(_("Downloading archive...<br />"));
    rc = graburl(globaldata.gd_inidata->web_archive, 0644, 0, 0);
    if (rc == 0) Error(_("Can't download the script archive"));
    filename = basename(globaldata.gd_inidata->web_archive);
  }

  command = mysprintf("%s '%s'", globaldata.gd_inidata->unzip, filename);

  printf(_("Uncompressing archive...<br />"));

  if (execute(command)) {
    if (globaldata.gd_loglevel > LOG_NONE)
      WriteLog(_("archive files extracted"));
    unlink(filename);
  }
  free(command);

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
        "<td colspan='2' class='submit_row'><input type='submit' value=\"%s\" name='B1'><input type='reset' value=\"%s\" name='B2'></td>\n"
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
        "<td class='submit_row_onecol'><input type='submit' value='%s' name='B1'><input type='reset' value='%s' name='B2'></td>\n"
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
          _("Please edit the destination web folder name"),
          ":",
          "",
          globaldata.gd_inidata->directory,
          _("Continue"),
          _("Clear"));
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
          _("Continue"),
          _("Clear"));
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
        "<input type='submit' value=\"%s\" name='B1'>\n"
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
  printf("<script language='javascript'>window.location=\"/%s/%s\";</script>\n",
          getfieldbyname("folder"),
          globaldata.gd_inidata->script2start);
}

char *upload_string =
        "<form method='POST' action=\"%s?%d\" enctype=\"multipart/form-data\">\n"
        "<input type='hidden' name='upload' value=\"1\">\n"
        "<table id='upload_table'>\n<tr><td class='upload_cell'>\n"
        "%s</td>\n"
        "<td><input type='file' name='ini' size='40'></td></tr>\n"
        "<tr><td class='upload_cell'>%s</td>\n"
        "<td><input type='file' name='zip' size='40'></td></tr>\n"
        "<tr><td class='upload_cell'>%s</td>\n"
        "<td><input type='checkbox' name='overwrite' checked='checked'></td></tr>\n"
        "<tr><td colspan='2' class='submit_row'><hr />\n"
        "<input type='submit' value=\"%s\" name='B1'><input type='reset' value=\"%s\" name='B2'></td></tr>\n"
        "</table></form>\n";

void UploadForm(void) {
  printf(upload_string, getenv("SCRIPT_NAME"),
          UPLOAD_CONFIG,
          _("configuration file"),
          _("archive file"),
          _("overwrite file"),
          _("Submit"),
          _("Clear"));
}

char *download_string =
        "<form method=\"POST\" action=\"%s?%d\">\n"
        "<input type='hidden' name='upload' value=\"0\">\n"
        "<table id='download_table'>\n"
        "<tr>\n"
        "<td>%s:</td>\n"
        "</tr>\n"
        "<tr>\n"
        "<td><input type='text' name='url' value=\"http://\" size='64'></td>\n"
        "</tr>\n"
        "<tr><td><input type='checkbox' name='overwrite' checked='checked'>%s</td></tr>\n"
        "<tr>\n"
        "<td class='submit_row_onecol'><br /><input type='submit' value=\"%s\" name='B1'><input type='reset' value=\"%s\" name='B2'></td>\n"
        "</tr>\n"
        "</table>\n"
        "</form>";

void DownloadForm(void) {
  printf(download_string, getenv("SCRIPT_NAME"),
          DOWNLOAD_CONFIG,
          _("Please insert the url of the configuration file"),
          _("overwrite file"),
          _("Submit"),
          _("Clear"));
}

char *script_string =
        "\n<script language=\"JavaScript\" type=\"text/javascript\">\n"
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
        "&nbsp;<br />%s %s<br />&nbsp;\n"
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

  result = append_cstring(result, "</select>\n");
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
          _("Localization & Log"),
          InsertLanguages(),
          InsertLogLevels(),
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
  return ACTION_START;
}

void GetStartPath(void) {
  char *path = malloc(PATH_SIZE);
  if (!path) return;
  if (!(getcwd(path, PATH_SIZE))) return;
  globaldata.gd_start_path = path;
}

int main(int argc, char **argv) {
  int action, logged, rc;
  char *ld;
  char *error_read = _("Error reading xml file");
  static char absolute_path[512];

  LIBXML_TEST_VERSION
  
  GetStartPath();

  action = ReadAction(argc, argv);

  logged = init(action);
  ld = globaldata.gd_locale_path;
  if (ld && *ld)
    realpath(ld, absolute_path);
  setlocale(LC_ALL, globaldata.gd_locale_code ? globaldata.gd_locale_code : "");
  bindtextdomain(PACKAGE, ld && *ld ? absolute_path : MYLOCALEDIR);
  textdomain(PACKAGE);

  printf(HTM_HEADER);
  globaldata.gd_header_sent = 1;

  if (!(logged)) {
    ShowLoginPage(action);
    printf(HTM_FOOTER);
    return 0;
  }
  StartSemaphore();
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
      if (!(globaldata.gd_inifile = get_ini_upload()))
        Error(_("can't access configuration file"));
      rc = read_xml_file(action);
      if (rc == 0) Error(error_read);
      get_zip_upload();
    case DOWNLOAD_CONFIG:
      if (action == DOWNLOAD_CONFIG) {
        if (!(globaldata.gd_iniaddress = get_ini_name(argc, argv)))
          Error(_("ini file name not specified"));
        rc = graburl(globaldata.gd_iniaddress, 0644, 0, 1);
        if (rc == 0) Error(_("Can't download ini file"));
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
        DownloadExtractArchiveFile();
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
      if (action == CREATE_FOLDER) {
        CreateChangeDir(getfieldbyname("folder"), __CREATE);
        DownloadExtractArchiveFile();
      } else {
        CreateChangeDir(getfieldbyname("folder"), __RENAME);
      }
      CreateFileSystemObject();
      ChangePermissionsRecurse();
      ChangePermissions();
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
      CreateDbTables();
      if (globaldata.gd_loglevel > LOG_NONE)
        WriteLog(_("MySQL setup"));
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

  printf(HTM_FOOTER);

  xmlCleanupParser();
  
  EndSemaphore();

  return 0;
}
