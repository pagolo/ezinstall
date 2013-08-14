#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__

int init (int action);
int read_ini_file(int action);
int is_upload(void);
void setMainConfig(char *section,char *name,char *value);
char *cloneaddress(char *origin,char *dest);
void SetPhpVar(char *varname, char *varvalue);

#endif
