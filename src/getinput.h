/***************************************************************************
                          getinput.h  -  description
                             -------------------
    begin                : Thu Jan 4 2001
    copyright            : (C) 2001 by Paolo Bozzo
    email                : bozzo@mclink.it
 ***************************************************************************/

#ifndef H_GETINPUT
#define H_GETINPUT

typedef struct StringField {
	char *name;
	char *value;
	char	*type;
	char	*buffer;
	int	bufferlen;
	struct StringField *next;
	} FIELD;

char *getfieldbyname(char *name);
char *getbinarybyname(char *name, int *len);

#endif
