#include "main.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>

#define TRUE 1
#define FALSE 0

void
parseSection(xmlDocPtr doc, xmlNodePtr cur, STRING *string, const xmlChar *sectionName) {
  STRING *s;
  xmlChar *key;
  cur = cur->xmlChildrenNode;

  while (cur != NULL) {
    for (s = string; s; s = s->next) {
      if ((!xmlStrcmp(cur->name, (const xmlChar *) s->string))) {
        key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
        setMainConfig((char *) sectionName, (char *) s->string, (char *) key);
        xmlFree(key);
      }
    }
    cur = cur->next;
  }
  return;
}

void
parseMainConfig(void) {

  xmlDocPtr doc;
  xmlNodePtr cur, nodeptr;
  STRING *stringlist = NULL;

  doc = xmlParseFile(CONFIG_NAME_ROOT);
  if (doc == NULL)
    doc = xmlParseFile(CONFIG_NAME);
  else
    globaldata.gd_config_root = 1;

  if (doc == NULL) {
    Error(_("Configuration xml document not parsed successfully. \n"));
  }

  cur = nodeptr = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    xmlFreeDoc(doc);
    Error(_("Configuration: empty document\n"));
  }

  if (xmlStrcmp(cur->name, (const xmlChar *) "ezinstall") != 0) {
    xmlFreeDoc(doc);
    Error(_("Configuration: document of the wrong type, root node != ezinstall"));
  }

  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "main"))) {
      appendstring(&stringlist, "language");
      appendstring(&stringlist, "locale_path");
      appendstring(&stringlist, "static_path");
      appendstring(&stringlist, "loglevel");
      parseSection(doc, cur, stringlist, cur->name);
      freestringlist(stringlist);
      stringlist = NULL;
    }
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "user"))) {
      appendstring(&stringlist, "username");
      appendstring(&stringlist, "password");
      parseSection(doc, cur, stringlist, cur->name);
      freestringlist(stringlist);
      stringlist = NULL;
    }
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "mysql"))) {
      appendstring(&stringlist, "username");
      appendstring(&stringlist, "password");
      appendstring(&stringlist, "host");
      appendstring(&stringlist, "database");
      parseSection(doc, cur, stringlist, cur->name);
      freestringlist(stringlist);
      stringlist = NULL;
    }
    cur = cur->next;
  }

  xmlFreeDoc(doc);
  return;
}

int setUnzip(char *filename, char *mode) {
  if (strcasecmp(mode, "auto") == 0) {
    char *ptr = strrchr(filename, '.');
    if (ptr && strcasecmp(ptr, ".zip") == 0)
      return PKZIP;
    else if (ptr && (strcasecmp(ptr, ".tgz") == 0 || strcasecmp(ptr, ".gz") == 0))
      return GZ_TAR;
    else if (ptr && (strcasecmp(ptr, ".bz2") == 0))
      return BZ2_TAR;
//    else if (ptr && (strcasecmp(ptr, ".Z") == 0))
//      return Z_TAR;
    else Error(_("Unknown format of archive file"));
  } else {
    // accepted values: auto, zip, gzip, bzip
    if (strcasecmp(mode, "zip") == 0)
      return PKZIP;
    else if (strcasecmp(mode, "gzip") == 0)
      return GZ_TAR;
    else if (strcasecmp(mode, "bzip") == 0)
      return BZ2_TAR;
    else Error(_("Unknown format of archive file"));
  }
  // never
  return -1;
}

void addChmod(char *file, char *mode, INIDATA *inidata, int recourse, int createfolder, char *extensions_string) {
  char *z;
  CHMOD *tmp;
  CHMOD *chmd = recourse ? inidata->perm_list_rec : inidata->perm_list;
  CHMOD *ptr = calloc(sizeof (CHMOD), 1);
  if (!ptr) return; // no memory
  ptr->file = strdup(file);
  ptr->permissions = strtol((const char *) mode, &z, 8);
  ptr->createfolder = createfolder;
  if (extensions_string && *extensions_string) {
    char *s = extensions_string;
    while (1) {
      char *sep = strchr(s, '|');
      if (sep) {
        *sep = '\0';
        ++sep;
      }
      appendstring(&(ptr->extensions), s);
      if (sep) s = sep;
      else break;
    }
  }

  if (chmd == NULL) {
    if (recourse) inidata->perm_list_rec = ptr;
    else inidata->perm_list = ptr;
  } else {
    tmp = chmd;
    while (tmp->next) tmp = tmp->next;
    tmp->next = ptr;
  }
}

int parsePermissionsNode(xmlDocPtr doc, xmlNodePtr cur) {
  xmlChar *file, *mode, *createfolder, *extensions;
  cur = cur->xmlChildrenNode;
  INIDATA *inidata = globaldata.gd_inidata;

  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (xmlChar *) "chmod")) || (!xmlStrcmp(cur->name, (xmlChar *) "chmod-recourse"))) {
      file = xmlGetProp(cur, (xmlChar *) "fsobject");
      mode = xmlGetProp(cur, (xmlChar *) "mode");
      createfolder = xmlGetProp(cur, (xmlChar *) "createfolder");
      extensions = xmlGetProp(cur, (xmlChar *) "extensions");
      addChmod((char *) file, (char *) mode, inidata, (!xmlStrcmp(cur->name, (xmlChar *) "chmod")) ? FALSE : TRUE, (xmlStrcmp(createfolder, (xmlChar *)"yes") == 0) ? 1 : 0, (char *)extensions);
      if (file) xmlFree(file);
      if (mode) xmlFree(mode);
      if (createfolder) xmlFree(createfolder);
      if (extensions) xmlFree(extensions);
    }
    cur = cur->next;
  }

  return 1;
}

int parseConfigNode(xmlDocPtr doc, xmlNodePtr cur) {
  xmlChar *key;
  char *mydb;
  INIDATA *inidata = globaldata.gd_inidata;

  if (inidata == NULL) return 0;
  inidata->flags &= ~_SKIP_CONFIGFILE;

  for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
    if ((!xmlStrcmp(cur->name, (xmlChar *) "file"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      inidata->php_conf_name = strdup((char *) key);
      xmlFree(key);
      continue;
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "saveas"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      inidata->php_conf_save = strdup((char *) key);
      xmlFree(key);
      continue;
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "datamode"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      if ((!xmlStrcmp(key, (xmlChar *) "variables")))
        inidata->data_mode = _VARIABLES;
      else if ((!xmlStrcmp(key, (xmlChar *) "defines")))
        inidata->data_mode = _DEFINES;
      else if ((!xmlStrcmp(key, (xmlChar *) "array")))
        inidata->data_mode = _ARRAY;
      xmlFree(key);
      continue;
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "myuser"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      SetPhpVar((char *) key, globaldata.gd_mysql->username);
      xmlFree(key);
      continue;
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "mypass"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      SetPhpVar((char *) key, globaldata.gd_mysql->password);
      xmlFree(key);
      continue;
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "myhost"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      SetPhpVar((char *) key, globaldata.gd_mysql->host);
      xmlFree(key);
      continue;
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "mydb"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      mydb = getfieldbyname("database");
      if (!(mydb && *mydb)) mydb = globaldata.gd_mysql->db_name;
      if (!(mydb)) mydb = "";
      SetPhpVar((char *) key, mydb);
      xmlFree(key);
      continue;
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "generic"))) {
      xmlChar *value = xmlGetProp(cur, (xmlChar *) "value");
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      SetPhpVar((char *) key, (char *)value);
      xmlFree(key);
    }
    //  printf("AAAAAAAAA");
    //  exit(1000);
  }
  return 1;
}

int parseMysqlNode(xmlDocPtr doc, xmlNodePtr cur) {
  xmlChar *key;
  cur = cur->xmlChildrenNode;
  INIDATA *inidata = globaldata.gd_inidata;

  if (inidata == NULL) return 0;
  inidata->flags &= ~_SKIP_MYSQL;

  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (xmlChar *) "db_name"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      if (globaldata.gd_mysql->db_name == NULL)
        globaldata.gd_mysql->db_name = strdup((char *) key);
      xmlFree(key);
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "db_file"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      appendstring(&(globaldata.gd_mysql->db_files), strdup((char *) key));
      xmlFree(key);
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "keep_sql"))) {
      inidata->flags &= _KEEP_SQL;
    }
    cur = cur->next;
  }

  return 1;
}

int parseFinishNode(xmlDocPtr doc, xmlNodePtr cur) {
  xmlChar *key;
  cur = cur->xmlChildrenNode;
  INIDATA *inidata = globaldata.gd_inidata;

  if (inidata == NULL) return 0;

  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (xmlChar *) "script"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      inidata->script2start = strdup((char *) key);
      xmlFree(key);
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "message"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      inidata->finalmessage = strdup((char *) key);
      xmlFree(key);
    }
    cur = cur->next;
  }

  return 1;
}

int parseMainNode(xmlDocPtr doc, xmlNodePtr cur, int action) {
  xmlChar *key, *attrib, *message, *subdir, *force;
  cur = cur->xmlChildrenNode;
  INIDATA *inidata;

  globaldata.gd_inidata = inidata = calloc(sizeof (INIDATA), 1);
  if (inidata == NULL) return 0;
  inidata->flags |= (_SKIP_MYSQL | _SKIP_CONFIGFILE);

  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (xmlChar *) "directory"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      attrib = xmlGetProp(cur, (xmlChar *) "create");
      message = xmlGetProp(cur, (xmlChar *) "message");
      subdir = xmlGetProp(cur, (xmlChar *) "subdir");
      inidata->directory = strdup((char *) key);
      inidata->dir_msg = message ? strdup((char *)message) : NULL;
      inidata->archive_dir = subdir ? strdup((char *)subdir) : NULL;
      if (xmlStrcmp(attrib, (xmlChar *) "yes") == 0) {
        inidata->flags |= _CREATEDIR;
      }
      xmlFree(key);
      if (attrib) xmlFree(attrib);
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "url")) && action == DOWNLOAD_CONFIG) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      if (inidata->web_archive) free(inidata->web_archive);
      inidata->web_archive = strdup((char *)key);
      xmlFree(key);
    }
    if ((!xmlStrcmp(cur->name, (xmlChar *) "file"))) {
      key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
      attrib = xmlGetProp(cur, (xmlChar *) "unzip");
      force = xmlGetProp(cur, (xmlChar *) "force");
      if (force && xmlStrcmp(force, (xmlChar *) "yes") == 0) {
        inidata->flags |= _FORCE_ARCHIVE;
      }
      if (globaldata.gd_iniaddress != NULL) {
        if (inidata->web_archive == NULL)
          inidata->web_archive = cloneaddress(globaldata.gd_iniaddress, (char *) key);
      }
      else if (action == UPLOAD_CONFIG) { // upload, not download
        char *path = getenv("DOCUMENT_ROOT");
        if (path && *path) {
          char *userfile = getfieldbyname("zip");
          if (userfile == NULL) userfile = getfieldbyname("zipfile");
          path = append_cstring(NULL, path);
          if (path[strlen(path) - 1] != '/') path = append_cstring(path, "/");
          inidata->web_archive = append_cstring(path, (inidata->flags & _FORCE_ARCHIVE) && userfile ? userfile : (char *) key);
        }
      }
      inidata->zip_format = setUnzip(inidata->web_archive? inidata->web_archive : (char *)key, attrib ? (char *) attrib : "auto");
      xmlFree(key);
      if (attrib) xmlFree(attrib);
      if (force) xmlFree(force);
    }
    cur = cur->next;
  }
  return 1;

}

int read_xml_file(int action) {
  xmlDocPtr doc;
  xmlNodePtr cur, nodeptr;

  doc = xmlParseFile(globaldata.gd_inifile);

  if (doc == NULL) {
    Error(_("Configuration xml document not parsed successfully. \n"));
  }

  cur = nodeptr = xmlDocGetRootElement(doc);

  if (cur == NULL) {
    xmlFreeDoc(doc);
    Error(_("Configuration: empty document\n"));
  }

  if (xmlStrcmp(cur->name, (const xmlChar *) "ezinstaller") != 0) {
    xmlFreeDoc(doc);
    Error(_("Configuration: document of the wrong type, root node != ezinstaller"));
  }

  cur = cur->xmlChildrenNode;
  while (cur != NULL) {
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "main"))) {
      parseMainNode(doc, cur, action);
    }
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "permissions"))) {
      parsePermissionsNode(doc, cur);
    }
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "config"))) {
      parseConfigNode(doc, cur);
    }
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "mysql"))) {
      parseMysqlNode(doc, cur);
    }
    if ((!xmlStrcmp(cur->name, (const xmlChar *) "finish"))) {
      parseFinishNode(doc, cur);
    }
    cur = cur->next;
  }

  return 1;
}

int WriteGlobalConfig(void) {
  int rc, result = 0;
  char *username, *password;
  char *config_name = globaldata.gd_config_root ? CONFIG_NAME_ROOT : CONFIG_NAME;
  xmlTextWriterPtr writer;
#ifdef MEMORY
  int fd;
  xmlBufferPtr buf;

  /* Create a new XML buffer, to which the XML document will be
   * written */
  buf = xmlBufferCreate();
  if (buf == NULL) {
    //printf("testXmlwriterMemory: Error creating the xml buffer\n");
    return 0;
  }

  /* Create a new XmlWriter for memory, with no compression.
   * Remark: there is no compression for this kind of xmlTextWriter */
  writer = xmlNewTextWriterMemory(buf, 0);
  if (writer == NULL) {
    xmlBufferFree(buf);
    return 0;
  }
#else
  // create the file
  writer = xmlNewTextWriterFilename(globaldata.gd_config_root ? CONFIG_NAME_ROOT : CONFIG_NAME, 0);
  if (writer == NULL) return 0;
#endif

  // start the document
  rc = xmlTextWriterStartDocument(writer, NULL, NULL, NULL);
  if (rc < 0) goto finish;
  // root element
  rc = xmlTextWriterStartElement(writer, BAD_CAST "ezinstall");
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n  ");
  if (rc < 0) goto finish;
  /////////////////////////////////////
  // start the main section
  rc = xmlTextWriterStartElement(writer, BAD_CAST "main");
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the language
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "language", BAD_CAST getfieldbyname("Language"));
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the locale path
  if (globaldata.gd_locale_path && *(globaldata.gd_locale_path)) {
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "locale_path", BAD_CAST globaldata.gd_locale_path);
    if (rc < 0) goto finish;
    // newline
    rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
    if (rc < 0) goto finish;
  }
  // the static files path
//  if (globaldata.gd_static_path && *(globaldata.gd_static_path)) {
    rc = xmlTextWriterWriteElement(writer, BAD_CAST "static_path", BAD_CAST getfieldbyname("StaticData"));
    if (rc < 0) goto finish;
    // newline
    rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
    if (rc < 0) goto finish;
  //}
  // the loglevel
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "loglevel", BAD_CAST getfieldbyname("LogLevel"));
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n  ");
  if (rc < 0) goto finish;
  /* Close the element named main. */
  rc = xmlTextWriterEndElement(writer);
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n  ");
  if (rc < 0) goto finish;
  /////////////////////////////////////
  // user section
  rc = xmlTextWriterStartElement(writer, BAD_CAST "user");
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the name
  username = getfieldbyname("admin_name");
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "username", BAD_CAST username);
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the password
  char *input_passwd = getfieldbyname("admin_pass1");
  if (input_passwd && *input_passwd)
    password = do_hash(input_passwd);
  else
    password = globaldata.gd_userdata->password;
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "password", BAD_CAST password);
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n  ");
  if (rc < 0) goto finish;
  /* Close the element named user. */
  rc = xmlTextWriterEndElement(writer);
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n  ");
  if (rc < 0) goto finish;
  /////////////////////////////////////
  // mysql section
  rc = xmlTextWriterStartElement(writer, BAD_CAST "mysql");
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the name
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "username", BAD_CAST getfieldbyname("mysql_user"));
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the password
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "password", BAD_CAST getfieldbyname("mysql_pass1"));
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the hostname
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "host", BAD_CAST getfieldbyname("mysql_host"));
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n    ");
  if (rc < 0) goto finish;
  // the database name
  rc = xmlTextWriterWriteElement(writer, BAD_CAST "database", BAD_CAST getfieldbyname("mysql_db"));
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n  ");
  if (rc < 0) goto finish;
  /* Close the element named mysql. */
  rc = xmlTextWriterEndElement(writer);
  if (rc < 0) goto finish;
  // newline
  rc = xmlTextWriterWriteString(writer, BAD_CAST "\n");
  if (rc < 0) goto finish;
  /////////////////////////////////////
  /* Close all. */
  rc = xmlTextWriterEndDocument(writer);
  if (rc < 0) goto finish;

#ifdef MEMORY
  xmlFreeTextWriter(writer);
  writer = NULL;

  remove(config_name);
  fd = open(config_name, O_CREAT | O_RDWR | O_TRUNC, 0600);
  if (fd == NO_FILE) goto finish;
  //write(fd, (const void *) buf->content, buf->size);
  write(fd, (const void *) buf->content, xmlStrlen(buf->content));
  close(fd);
#endif

  result = 1;

finish:
  if (writer) xmlFreeTextWriter(writer);
#ifdef MEMORY
  xmlBufferFree(buf);
#endif
  return result;
}

/*
int
main(int argc, char **argv) {

        char *docname;
		
        if (argc <= 1) {
                printf("Usage: %s docname\n", argv[0]);
                return(0);
        }

        docname = argv[1];
        parseDoc (docname);

        return (1);
}
 */
