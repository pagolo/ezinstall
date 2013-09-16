/***************************************************************************
                         mysql.c  -  description
                            -------------------
   begin                : lun lug 7 2003
   copyright            : (C) 2003 by Paolo Bozzo
   email                : bozzo@mclink.it
 ***************************************************************************/

#include "main.h"
#include <mysql/mysql.h>

int MySqlTest(void) {
  MYSQL *conn;

  conn = mysql_init(NULL);
  if (!(conn)) return 0;

  if (mysql_real_connect(conn, globaldata.gd_mysql->host, globaldata.gd_mysql->username, globaldata.gd_mysql->password, NULL, 0, NULL, 0) == NULL) {
    //printf("connect: %s %s %s",globaldata.gd_mysql->host,globaldata.gd_mysql->username,globaldata.gd_mysql->password);
    mysql_close(conn); // verificare se richiesto (anche sotto)
    return 0;
  }

  mysql_close(conn);
  return 1;
}

void MySqlForm(void) {
  MYSQL *conn;
  MYSQL_RES *res_set = NULL;
  MYSQL_ROW row;
  char *query;
  unsigned int numrows = 0;

  printf("\n<form method='POST' action=\"%s?%d\">\n<table>\n",
          getenv("SCRIPT_NAME"),
          CREATE_DB);

  printf("<input type='hidden' name='inifile' value=\"%s\">\n"
          "<input type='hidden' name='folder' value=\"%s\">\n"
          "<input type='hidden' name='webarchive' value=\"%s\">\n",
          globaldata.gd_inifile,
          getfieldbyname("folder"),
          globaldata.gd_inidata->web_archive);

  // sezione per menu drop-down
  conn = mysql_init(NULL);
  if (!(conn)) Error(_("error initiating MySQL"));

  if (mysql_real_connect(conn, globaldata.gd_mysql->host, globaldata.gd_mysql->username, globaldata.gd_mysql->password, NULL, 0, NULL, 0) == NULL) {
    mysql_close(conn);
    Error(_("error connecting to MySQL"));
  }
  query = "SELECT SCHEMA_NAME FROM information_schema.SCHEMATA"; // "SHOW databases;";
  if (mysql_query(conn, query) == 0) {
    res_set = mysql_store_result(conn);
    numrows = mysql_num_rows(res_set);
  }
  if (numrows > 0) {
    printf("<tr><td colspan='2'>%s<hr>&nbsp;</td></tr>\n", _("Please insert a new name or choose an existing database..."));
    printf("<tr><td style='text-align:right; width:33%%;'>%s</td>\n<td><select name='dbases' onChange='if (this.value!=\"\") database.value=this.value'>\n<option value=''>%s</option>\n", _("select existing db"), _("database"));
    while ((row = mysql_fetch_row(res_set)) != NULL) {
      if (strcmp(row[0], "information_schema") == 0 || strcmp(row[0], "performance_schema") == 0)
        continue;
      printf("<option value='%s'>%s</option>\n", row[0], row[0]);
    }
    printf("</select>\n</td></tr>");
  }
  if (res_set) mysql_free_result(res_set);
  mysql_close(conn);
  // fine sezione per menu

  printf("<tr><td style='text-align:right; width:33%%;'>%s</td><td><input type='text' name='database' value=%s></td></tr>\n", _("database name"), globaldata.gd_mysql->db_name);

  printf("<tr><td colspan=2><br />\n<input type=submit value=\"%s\" name=B1><input type=reset value=\"%s\" name=B2></td></tr>\n",
          _("Continue"),
          _("Clear"));

  printf("</table>\n</form>");
}

void ExecuteSqlFile(STRING **list) {
  char buffer[256];
  int rc;
  FILE *fh;
  int pipe[3];
  int pid;
  char *args[] = {
    "mysql",
    mysprintf("-h%s", globaldata.gd_mysql->host),
    mysprintf("-u%s", globaldata.gd_mysql->username),
    mysprintf("-p%s", globaldata.gd_mysql->password),
    mysprintf("%s", globaldata.gd_mysql->db_name),
    NULL
  };

  ChDirRoot();
  rc = chdir(getfieldbyname("folder"));
  if (rc == -1) DaemonError(_("Can't chdir to project folder"), list);

  {
    STRING *script = globaldata.gd_mysql->db_files;
    for (; script != NULL; script = script->next) {
      fh = fopen(script->string, "r");
      if (!(fh)) DaemonError(_("can't find sql file"), list);

      pid = popenRWE(pipe, args[0], args);
      if (pid == -1) DaemonError(_("can't execute mysql client..."), list);
      
      FILE *in = fdopen(pipe[0], "w");
      FILE *err = fdopen(pipe[2], "r");
      while (fgets(buffer, 256, fh)) fputs(buffer, in);
      fclose(fh);
      fclose(in);
      if (fgets(buffer, 256, err)) {
        fclose(err);
        pcloseRWE(pid, pipe);
        DaemonError(buffer, list);
      }
      fclose(err);
      pcloseRWE(pid, pipe);
    }
  }
  if (args[1]) free(args[1]);
  if (args[2]) free(args[2]);
  if (args[3]) free(args[3]);
  if (args[4]) free(args[4]);
}

void CreateDbTables(STRING **list) {
  MYSQL *conn;
  char *query;

  // cercare/creare il database
  conn = mysql_init(NULL);
  if (!(conn)) DaemonError(_("error initiating MySQL"), list);

  if (mysql_real_connect(conn, globaldata.gd_mysql->host, globaldata.gd_mysql->username, globaldata.gd_mysql->password, NULL, 0, NULL, 0) == NULL) {
    mysql_close(conn);
    DaemonError(_("error connecting to MySQL"), list);
  }
  if (mysql_select_db(conn, globaldata.gd_mysql->db_name) != 0) {
    // database doesn't exist? create it
    query = mysprintf("CREATE DATABASE `%s`", globaldata.gd_mysql->db_name);
    if (mysql_query(conn, query) != 0) {
      mysql_close(conn);
      DaemonError(_("can't create new database, please go back and select an existing db"), list);
    } else {
      char *s = mysprintf(_("Database '%s' has been created<br />"), globaldata.gd_mysql->db_name);
      HandleSemaphoreText(s, list, 1);
      if (s) free(s);
    }
    free(query);
  } else {
    char *s = mysprintf(_("Database '%s' was found<br />"), globaldata.gd_mysql->db_name);
    HandleSemaphoreText(s, list, 1);
    if (s) free(s);
  }
  mysql_close(conn);

  if (globaldata.gd_mysql->db_files) {
    HandleSemaphoreText(_("creating database tables..."), list, 1);
    ExecuteSqlFile(list);
    HandleSemaphoreText(_("database tables have been created"), list, 0);
  }
}
