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

char *search(char *command) {
    FILE *ph;
    static char buffer[64];
    char *cmd = mysprintf("which %s", command);
    ph = popen(cmd, "r");
    free(cmd);
    if (!ph) return NULL;
    fread(buffer, 64, 1, ph);
    pclose(ph);
    if (strlen(buffer) == 0) return (_("<span style='color:red'>not found</span>"));
    return buffer;
}

void DoTest(void) {
    struct passwd *user;
    char *user_string;
    int euid, tno;
    char punct1[] = "<li><small>";
    char punct2[] = "</small></li>";

    printf("%s%s: %s%s", punct1, _( "Software version"), VERSION, punct2);
    euid = geteuid();
    user = getpwuid(euid);
    //   printf("%s%s: %s%s",punct1,_("Application user"),user?user->pw_name:(_("Can't get user name")),punct2);
    if (user)
        user_string = user->pw_name;
    else
        user_string = mysprintf("<span style='color:red'>%s</span>", strerror(errno));
    printf("%s%s: %s%s", punct1, _("Application user"), user_string, punct2);
    if (!user) free(user_string);
    printf("%s%s: %s%s", punct1, _( "Localization setup"), globaldata.gd_language, punct2);
    if (MySqlTest())
        printf("%s%s%s", punct1, _("MySQL connection is OK"), punct2);
    else
        printf("%s<font color=red>%s</font>%s", punct1, _("Error with MySQL connection"), punct2);
    tno = TempCount();
    printf("%s%s: %d%s", punct1, _( "Temporary files found"), tno, punct2);
    if (tno > 0) {
        printf("%s<a onClick=\"if (confirm('%s')) return true; return false;\" href=\"%s?deltemp\">%s</a>%s\n",
                punct1,
                _("Are you sure\\nyou want to delete temporary files?"),
                getenv("SCRIPT_NAME"),
                _( "Delete temporary files"),
                punct2);
    }
    printf("%s%s: %s%s\n", punct1, _("Unzip path"), search("unzip"), punct2);
    printf("%s%s: %s%s\n", punct1, _("MySql client path"), search("mysql"), punct2);

    printf("<br /><br /><a href=\"%s?editconf\">%s</a>", getenv("SCRIPT_NAME"), _("Edit configuration"));
}
