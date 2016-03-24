#include "main.h"
#include <dirent.h>
#include <pwd.h>
#include "errno.h"
#include "string.h"

int CheckTemp(char *file) {
  // TO DO: verificare che sia un xml con root "easinstaller"...
  return 1;
  /*
 int len=sizeof(TEMP_MARK);
 static char buf[sizeof(TEMP_MARK)];
 int fd=open(file,O_RDONLY);
 if (fd==NO_FILE) return 0;
 read(fd,buf,len);
 close(fd);
 if (strncmp(buf,TEMP_MARK,len-2)==0) return 1;
 return 0;
   */
}

int TempCount(void) {
  struct dirent *de;
  DIR *mydir;
  int result = 0;

  mydir = opendir(".");
  if (mydir) {
    while ((de = readdir(mydir))) {
      if (strncmp(de->d_name, "ezin", 4) == 0 && strlen(de->d_name) == 10 && CheckTemp(de->d_name))
        ++result;
    }
  }
  return result;
}

void DeleteTemp(void) {
  STRING *list = NULL, *ptr;
  struct dirent *de;
  DIR *mydir;

  mydir = opendir(".");
  if (mydir) {
    while ((de = readdir(mydir))) {
      if (strncmp(de->d_name, "ezin", 4) == 0 && strlen(de->d_name) == 10 && CheckTemp(de->d_name))
        appendstring(&list, de->d_name);
    }
  }
  for (ptr = list; ptr; ptr = ptr->next) unlink(ptr->string);
  freestringlist(list);
}
void DoTest(void) {
  struct passwd *user;
  char *user_string;
  int euid, tno;
  char dot1[] = "\n<li><small>";
  char dot2[] = "</small></li>";
  
  printf("\n<ul>");

  printf("%s%s: %s%s", dot1, _("Software version"), VERSION, dot2);
  euid = geteuid();
  user = getpwuid(euid);
  //   printf("%s%s: %s%s",dot1,_("Application user"),user?user->pw_name:(_("Can't get user name")),dot2);
  if (user)
    user_string = user->pw_name;
  else
    user_string = mysprintf("<span style='color:red'>%s</span>", strerror(errno));
  printf("%s%s: %s%s", dot1, _("Application user"), user_string, dot2);
  if (!user) free(user_string);
  printf("%s%s: %s%s", dot1, _("Localization setup"), globaldata.gd_locale_code, dot2);
  if (MySqlTest())
    printf("%s%s%s", dot1, _("MySQL connection is OK"), dot2);
  else
    printf("%s<span style='color:red'>%s</span>%s", dot1, _("Error with MySQL connection"), dot2);
  if (globaldata.gd_php_sapi)  // php handler
    printf("%s%s: %s%s", dot1, _("PHP Handler"), globaldata.gd_php_sapi, dot2);
  tno = TempCount();
  printf("%s%s: %d%s", dot1, _("Temporary files found"), tno, dot2);
  if (tno > 0) {
    printf("%s<a onClick=\"if (confirm('%s')) return true; return false;\" href=\"%s?deltemp\">%s</a>%s\n",
            dot1,
            _("Are you sure\\nyou want to delete temporary files?"),
            getenv("SCRIPT_NAME"),
            _("Delete temporary files"),
            dot2);
  }

  printf("\n</ul>\n<br /><a href=\"%s?editconf\">%s</a>", getenv("SCRIPT_NAME"), _("Edit configuration"));
}
