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
      int retval = 0;
      if (strcmp(row[0], "information_schema") == 0 || strcmp(row[0], "performance_schema") == 0)
        continue;
      char *freeme = mysprintf("SELECT COUNT(*) FROM information_schema.TABLES WHERE table_schema = '%s';", row[0]);
      if (freeme) {
        if (!(mysql_query(conn, freeme))) {
          MYSQL_RES *result = mysql_store_result(conn);
          if (result) {
            MYSQL_ROW rowdata = mysql_fetch_row(result);
            if (rowdata)
              retval = atoi(rowdata[0]);
          }
          mysql_free_result(result);
        }
        free(freeme);
      }
      printf("<option value='%s'>%s (%d)</option>\n", row[0], row[0], retval);
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

void ExecuteSqlFile(MYSQL *mysql, char *filename, STRING **list) {
  FILE *fh;
  char *buf, *ptr;
  struct stat filestat;
  int totalsize, status;
  MYSQL_RES *result;

  fh = fopen(filename, "r");
  if (!fh) {
    mysql_close(mysql);
    DaemonError(_("Can't open .sql file"), list);
  }
  if (fstat(fileno(fh), &filestat) < 0) {
    mysql_close(mysql);
    fclose(fh);
    DaemonError(_("Can't stat .sql file"), list); // *** stat() file
  }
  totalsize = filestat.st_size; // *** get size of file
  buf = calloc(1, totalsize + 1);
  if (!buf) {
    mysql_close(mysql);
    fclose(fh);
    DaemonError(_("Can't allocate memory"), list);
  }
  fread(buf, 1, totalsize, fh);
  fclose(fh);

  // clear -- comments  
  for (ptr = buf; *ptr != '\0'; ptr++) {
    if (ptr > buf) {
      if (ptr[-1] == '\n' && ptr[0] == '-') {
        while (*ptr != '\r' && *ptr != '\n')
          *ptr++ = ' ';
      } 
    }
  }

  status = mysql_query(mysql, buf);
  if (status) {
    mysql_close(mysql);
    DaemonError(_("Can't execute statements"), list);
  }
  /* process each statement result */
  do {
    /* did current statement return data? */
    result = mysql_store_result(mysql);
    if (result) {
      /* yes; process rows and free the result set */
      //process_result_set(mysql, result);
      //printf("row count = %d\n", result->row_count);
      mysql_free_result(result);
    } else /* no result set or error */ {
      if (mysql_field_count(mysql) == 0) {
      } else /* some error occurred */ {
        mysql_close(mysql);
        DaemonError(_("Can't retrieve result set"), list);
      }
    }
    /* more results? -1 = no, >0 = error, 0 = yes (keep looping) */
    if ((status = mysql_next_result(mysql)) > 0) {
      mysql_close(mysql);
      DaemonError(_("Can't execute statement"), list);
    }
  } while (status == 0);
}

void CreateDbTables(STRING **list) {
  MYSQL *conn;
  char *query;
  STRING *ptr;

  // cercare/creare il database
  conn = mysql_init(NULL);
  if (!(conn)) DaemonError(_("error initiating MySQL"), list);

  if (mysql_real_connect(conn, globaldata.gd_mysql->host, globaldata.gd_mysql->username, globaldata.gd_mysql->password, NULL, 0, NULL, CLIENT_MULTI_STATEMENTS) == NULL) {
    mysql_close(conn);
    DaemonError(_("error connecting to MySQL"), list);
  }
  if (mysql_select_db(conn, globaldata.gd_mysql->db_name) != 0) {
    // database doesn't exist? create it
    query = mysprintf("CREATE DATABASE `%s`;", globaldata.gd_mysql->db_name);
    if (mysql_query(conn, query) != 0) {
      mysql_close(conn);
      DaemonError(_("can't create new database, please go back and select an existing db"), list);
    } else {
      char *s = mysprintf(_("Database '%s' has been created<br />"), globaldata.gd_mysql->db_name);
      HandleSemaphoreText(s, list, 1);
      if (s) free(s);
    }
    free(query);
    if (mysql_select_db(conn, globaldata.gd_mysql->db_name) != 0) {
      mysql_close(conn);
      DaemonError(_("Can't select newly created database"), list);
    }
  } else {
    char *s = mysprintf(_("Database '%s' was found<br />"), globaldata.gd_mysql->db_name);
    HandleSemaphoreText(s, list, 1);
    if (s) free(s);
  }

  ChDirRoot();
  char *folder = getfieldbyname("folder");
  if (!folder) {
    mysql_close(conn);
    DaemonError(_("No project folder name"), list);
  }
  int rc = chdir(folder);
  if (rc == -1) {
    mysql_close(conn);
    DaemonError(_("Can't chdir to project folder"), list);
  }

  if (globaldata.gd_mysql->db_files) {
    HandleSemaphoreText(_("creating database tables, please wait...<div class=\"throbber\">&nbsp;</div>"), list, 1);
    for (ptr = globaldata.gd_mysql->db_files; ptr; ptr = ptr->next) {
      ExecuteSqlFile(conn, ptr->string, list);
      if (!(globaldata.gd_inidata->flags & _KEEP_SQL))
        unlink(ptr->string);
    }
    HandleSemaphoreText(_("Database tables have been successfully created..."), list, 0);
  }
  mysql_close(conn);
}

int FileIsSQL(char *path) {
  STRING *ptr;
  if (globaldata.gd_mysql->db_files) {
    for (ptr = globaldata.gd_mysql->db_files; ptr; ptr = ptr->next)
      if (strcmp(basename(path), ptr->string) == 0)
        return 1;
  }
  return 0;
}
