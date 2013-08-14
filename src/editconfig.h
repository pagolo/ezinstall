#ifndef __EDITCONFIG_H__
#define __EDITCONFIG_H__

enum PhpState {
   IN_CODE,
   IN_STRING,
   IN_COMMENT_STD,
   IN_COMMENT_PLUS
};

void EditConfigForm(void);
void SaveConfigFile(void);

#endif
