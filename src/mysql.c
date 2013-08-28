 /***************************************************************************
                          mysql.c  -  description
                             -------------------
    begin                : lun lug 7 2003
    copyright            : (C) 2003 by Paolo Bozzo
    email                : bozzo@mclink.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "main.h"
#include <mysql/mysql.h>

int MySqlTest(void) {
   MYSQL *conn;
   
   conn = mysql_init(NULL);
   if (!(conn)) return 0;
   
   if (mysql_real_connect(conn,globaldata.gd_mysql->host,globaldata.gd_mysql->username,globaldata.gd_mysql->password,NULL,0,NULL,0)==NULL) {
   //printf("connect: %s %s %s",globaldata.gd_mysql->host,globaldata.gd_mysql->username,globaldata.gd_mysql->password);
	mysql_close(conn); // verificare se richiesto (anche sotto)
	return 0;
	}

   mysql_close(conn);
   return 1;
}

void MySqlForm(void) {
   MYSQL *conn;
   MYSQL_RES *res_set=NULL;
   MYSQL_ROW row;
   char *query;
   unsigned int numrows=0;
   
   printf ("\n<form method=POST action=\"%s?%d\">\n<table>\n",
           getenv("SCRIPT_NAME"),
           CREATE_DB);

   printf ("<input type=hidden name=inifile value=\"%s\">\n"
           "<input type=hidden name=folder value=\"%s\">\n"
           "<input type=hidden name=webarchive value=\"%s\">\n",
           globaldata.gd_inifile,
           getfieldbyname("folder"),
           globaldata.gd_inidata->web_archive);

   // sezione per men� drop-down
   conn = mysql_init(NULL);
   if (!(conn)) Error(_("error initiating MySQL"));

   if (mysql_real_connect(conn,globaldata.gd_mysql->host,globaldata.gd_mysql->username,globaldata.gd_mysql->password,NULL,0,NULL,0)==NULL) {
	mysql_close(conn);
	Error(_("error connecting to MySQL"));
	}
   query="SHOW databases;";
   if (mysql_query(conn,query)==0) {      
      res_set = mysql_store_result(conn);
      numrows = mysql_num_rows(res_set);
   }
   if (numrows>0) {
      printf("<tr><td colspan=2>%s<hr>&nbsp;</td></tr>\n",_("Please insert a new name or choose an existing database..."));
      printf("<tr><td align=right width=33%%>%s</td>\n<td><select name=dbases onChange='if (this.value!=\"\") database.value=this.value'>\n<option value=''>%s</option>\n",_("select existing db"),_("database"));
      while ((row = mysql_fetch_row(res_set)) != NULL) {
         printf("<option value=%s>%s</option>\n",row[0],row[0]);
      }
      printf("</select>\n</td></tr>");
   }
   if (res_set) mysql_free_result(res_set);
   mysql_close(conn);
   // fine sezione per menu
   
   printf("<tr><td align=right width=33%%>%s</td><td><input type=text name=database value=%s></td></tr>\n",_("database name"),globaldata.gd_mysql->db_name);

   printf("<tr><td colspan=2><br />\n<input type=submit value=\"%s\" name=B1><input type=reset value=\"%s\" name=B2></td></tr>\n",
          _("Continue"),
          _("Clear"));
   
   printf("</table>\n</form>");
}

void ExecuteSqlFile(void) {
   char buffer[256], *command;
   int rc;
   FILE *fh;
   FILE *ph;

   command=mysprintf("mysql -h'%s' -u'%s' -p'%s' '%s'",
                     globaldata.gd_mysql->host,
                     globaldata.gd_mysql->username,
                     globaldata.gd_mysql->password,
                     globaldata.gd_mysql->db_name);

   ChDirRoot();
   rc=chdir(getfieldbyname("folder"));
   if (rc==-1) Error(_("Can't chdir to project folder"));
   
   {
     STRING *script = globaldata.gd_mysql->db_files;
     for ( ; script != NULL; script = script->next )
     {
       fh=fopen(script->string, "r");
       if (!(fh)) Error(_("can't find sql file"));
   
       ph=popen(command,"w");
       if (!(ph)) Error(_("can't execute mysql client..."));

       while (fgets(buffer,256,fh)) fputs(buffer,ph);

       fclose(fh);
       pclose(ph);
     }
   }
   
   free(command);
}

void CreateDbTables(void) {
   MYSQL *conn;
   char *query;

   printf("<br />");
   
   // cercare/creare il database
   conn = mysql_init(NULL);
   if (!(conn)) Error(_("error initiating MySQL"));

   if (mysql_real_connect(conn,globaldata.gd_mysql->host,globaldata.gd_mysql->username,globaldata.gd_mysql->password,NULL,0,NULL,0)==NULL) {
	mysql_close(conn);
	Error(_("error connecting to MySQL"));
	}
   if (mysql_select_db(conn,globaldata.gd_mysql->db_name)!=0) {
      // database doesn't exist? create it
      query=mysprintf("CREATE DATABASE `%s`",globaldata.gd_mysql->db_name);
      if (mysql_query(conn,query)!=0) {
	mysql_close(conn);
	Error(_("can't create new database, please go back and select an existing db"));
      } else {
        printf(_("Database '%s' has been created<br />"),globaldata.gd_mysql->db_name);
      }
      free(query);
    } else {
      printf(_("Database '%s' was found<br />"),globaldata.gd_mysql->db_name);
    }
   mysql_close(conn);

   if (globaldata.gd_mysql->db_files) {
      ExecuteSqlFile();
      printf(_("database tables have been created<br />"));
   }
}
