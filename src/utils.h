#ifndef __UTILS_H__
#define __UTILS_H__

//------------------ STRING LISTS

typedef struct String {
	char *string;
	struct String *next;
	} STRING;

STRING *newstring(char *s);
int appendstring(STRING **head, char *s);
int freestringlist(STRING *list);

//------------------ VARIOUS UTILITIES
char * append_cstring (char *rc, const char *appendable);
char *mysprintf(const char *format, ...);
int execute(char *s);

//------------------ LOG FILE
void WriteLog (char *msg);

//------------------ UPLOAD/DOWNLOAD UTILITIES
char *get_ini_upload(void);
int get_zip_upload(void);
char *get_ini_name(int argc, char **argv);
int graburl(char *url,int permissions, int expand, int tempname);
int graburl_list(char *url,int permissions, int expand, int tempname, STRING **list);

//------------------ PERMISSIONS
void ChangePermissionsRecurse(void);
void ChangePermissions(void);

//------------------ popenRWE
int popenRWE(int *, char *, char **);
int pcloseRWE(int, int *);

//------------------ start/end semaphore
void StartSemaphore(void);
void EndSemaphore(void);
void AddSemaphoreText(char *s);
void EndSemaphoreText();
void HandleSemaphoreText(char *text, STRING **list, int append);
#endif
