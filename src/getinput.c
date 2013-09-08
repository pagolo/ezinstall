
/***************************************************************************
                          getinput.c  -  description
                             -------------------
    begin                : Sat Feb 16 2002
    copyright            : (C) 2002 by Paolo Bozzo
    email                : paolo@casabozzo.net
 ***************************************************************************/
#define _GNU_SOURCE
#include "main.h"

char *ReadInputGet(void) {
  char *buf = getenv("QUERY_STRING");

  return (buf ? strdup(buf) : NULL);
}

char *ReadInput(int *len, const char *method) {
  char *buf;
  char *sbuflen = getenv("CONTENT_LENGTH");
  int buflen;

  if (strcasecmp(method, "GET") == 0) return (ReadInputGet());

  if ((!(sbuflen)) || (!(*sbuflen))) return NULL;
  *len = buflen = atoi(sbuflen);

  if (!(buf = calloc(1, buflen + 2))) {
    return NULL;
  }
  fread(buf, buflen, 1, stdin);
  return buf;
}

FIELD *GetMultipart(const char *enctype, char *buffer, int len) {
  FIELD *start = NULL;
  FIELD *work = NULL, *current = NULL;
  char *ptr, *boundary, *fn, *z, *zz;
  char *startpoint, *header, *data, back;

  startpoint = buffer;

  // prima di tutto leggiamo il boundary
  ptr = strstr(enctype, "boundary"); //strcasestr(enctype,"boundary");
  if (ptr == NULL) return NULL;
  ptr = strchr(ptr, '=');
  ++ptr;
  while (isspace(*ptr)) ++ptr;
  boundary = strdup(ptr);
  ptr = strchr(boundary, '\r');
  if (ptr) *ptr = '\0';
  ptr = strchr(boundary, '\n');
  if (ptr) *ptr = '\0';

  // poi i dati mime
  if (*buffer++ != '-') return NULL; // doppio trattino
  if (*buffer++ != '-') return NULL;

  ptr = strstr(buffer, boundary);

  while (ptr != NULL) { // un nuovo campo
    size_t residual_len;
    header = ptr;
    ptr = &header[strlen(boundary)];
    if (ptr[0] == '-' && ptr[1] == '-') break; // fine ciclo
    data = strstr(ptr, "\r\n\r\n");
    if (data == NULL) data = strstr(ptr, "\n\n");
    if (data == NULL) return NULL;
    *data++ = '\0';
    while (*data == '\r' || *data == '\n') ++data;
    residual_len = (size_t) startpoint[len]-(size_t) data;
    ptr = (char *) memmem(data, residual_len, boundary, strlen(boundary));
    if (ptr == NULL) break;
    back = *ptr;
    *ptr = '\0';

    // get field name
    fn = strstr(header, "name=");
    if (fn == NULL) break;
    fn = &fn[5];
    while (isspace(*fn)) ++fn;
    if (*fn == '\"') ++fn;
    if ((z = strchr(fn, '\"'))) *z = '\0';
    work = calloc(sizeof (FIELD), 1);
    if (!(work)) break;
    work->name = strdup(fn);
    if (z) *z = '\"';

    // get file name
    fn = strstr(header, "filename");
    if (fn != NULL) {
      fn = &fn[8];
      while (isspace(*fn)) ++fn;
      if (*fn == '=') ++fn;
      if (*fn == '\"') ++fn;
      if ((z = strchr(fn, '\"'))) *z = '\0';
      work->value = strdup(fn);
      if (z) *z = '\"';
    }

    // get content-type
    fn = strstr(header, "Content-Type:");
    if (fn != NULL) {
      fn = strchr(fn, ':');
      ++fn;
      while (isspace(*fn)) ++fn;
      if (*fn == '\"') ++fn;
      if ((zz = strchr(fn, ' '))) *zz = '\0';
      else if ((z = strchr(fn, '\"'))) *z = '\0';
      work->type = strdup(fn);
      if (zz) *zz = ' ';
      else if (z) *z = '\"';
    }

    if (work->value == NULL) {// se campo normale...
      fn = strdup(data);
      z = strchr(fn, '\r');
      if (z == NULL) z = strchr(fn, '\n');
      if (z) *z = '\0';
      zz = strstr(fn, "--");
      if (zz && zz[2] == '\0') *zz = '\0';
      work->value = fn;
    } else {
      size_t filelen;
      // si tratta di un file, gestirlo come si deve
      // spostare z alla fine del buffer del file
      z = ptr;
      while (z[-1] == '-') --z;
      if (z[-1] == '\n') --z;
      if (z[-1] == '\r') --z;
      // attenzione con i 64 BIT !!!!!
      filelen = (size_t) z - (size_t) data;
      fn = calloc(filelen, 1);
      if (fn) {
        memcpy(fn, data, filelen);
        work->buffer = fn;
        work->bufferlen = filelen;
      }
    }

    *ptr = back;
    // accodare i dati
    if (start == NULL) current = start = work;
    else {
      current->next = work;
      current = work;
    }
  }

  return start;
}

FIELD *ParseInput(void) {
  static FIELD *start = NULL;
  FIELD *work, *current = NULL;
  char *ptr, *parsed, *str, *eq;
  char *endofbuffer, *buffer;
  const char *m = getenv("REQUEST_METHOD");
  const char *enctype = getenv("CONTENT_TYPE");
  int buflen;

  if (start) return start;

  if (m == NULL) return NULL;

  buffer = ReadInput(&buflen, m);
  if (!(buffer)) return NULL;
  endofbuffer = &buffer[strlen(buffer)];

  if (strcasecmp(m, "POST") == 0 && enctype && strcasestr(enctype, "multipart/form-data")) {
    start = GetMultipart(enctype, buffer, buflen);
  } else {
    for (parsed = ptr = buffer; ptr < endofbuffer; ptr++) {
      if (*ptr == '+') *ptr = ' ';
      if (*ptr == '&' || ptr[1] == '\0') {
        if (*ptr == '&') *ptr = '\0';
        if ((work = calloc(1, sizeof (FIELD))) != NULL) {
          str = strdup(parsed);
          if (!(str)) continue;
          unescape(str);
          eq = strchr(str, '=');
          if (!(eq)) {
            free(str);
            continue;
          }
          *eq = '\0';
          work->name = strdup(str);
          work->value = strdup(&eq[1]);
          free(str);
          if (start == NULL) start = work;
          else if (current) current->next = work;
          current = work;
        }
        parsed = &ptr[1];
      }
    }
  }
  free(buffer);
  return start;
}

void sanitize_xss(char *string) {
  char *ptr, quote = '\0';
  int tag = 0, instring = 0;
  for (ptr = string; *ptr != '\0'; ptr++) {
    if (*ptr == '<') tag = 1;
    if (*ptr == '>' && tag && !instring) tag = 0;
    if ((*ptr == '\'' || *ptr == '\"') && tag) {
      if (!instring) {
        quote = *ptr;
        instring = 1;
      }
      else if (*ptr == quote) {
        instring = 0;
      }
    }
    if (strncasecmp(ptr,"script",6) == 0 && tag && !instring) {
      memcpy(ptr, " span ", 6);
    }
  }
}

char *getfieldbyname_sanitize(char *name, int do_sanitize) {
  FIELD *inp;
  char *rc = NULL;

  for (inp = ParseInput(); inp; inp = inp->next) {
    if (strcasecmp(inp->name, name) == 0) {
      rc = strdup(inp->value);
      break;
    }
  }
  if (do_sanitize && rc) sanitize_xss(rc);
  return rc;
}

char *getfieldbyname(char *name) {
  return getfieldbyname_sanitize(name, 1);
}

char *getbinarybyname(char *name, int *len) {
  FIELD *inp;
  int mylen = strlen(name);
  char *rc = NULL;

  for (inp = ParseInput(); inp; inp = inp->next) {
    if (strncasecmp(inp->name, name, mylen) == 0) {
      rc = inp->buffer;
      *len = inp->bufferlen;
      break;
    }
  }
  return rc;
}


