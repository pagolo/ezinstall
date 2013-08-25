#include "main.h"
#include "mylocale.h"

char current_language[4];

void append_localelist(LOCALELIST **head, LOCALELIST *ptr) {
  if (ptr == NULL) return;
  if (*head == NULL) {
    *head = ptr;
  }
  else {
    LOCALELIST *tmp = *head;
    while (tmp->next) tmp = tmp->next;
    tmp->next = ptr;
  }
}

void free_localelist(LOCALELIST **head) {
  LOCALELIST *ptr, *next;
  for (ptr = *head; ptr != NULL; ptr = next) {
    next = ptr->next;
    free(ptr->locale_code);
    free(ptr->language_name);
    free(ptr);
  }
  *head = NULL;
}

static int my_dirs(const struct dirent *dirent) {
  int result = 0;
  if (dirent->d_name[0] != '.' && strlen(dirent->d_name) == 2) {
    mode_t mode = 0;
    {
      struct stat st;
      char buf[sizeof (MYLOCALES) + strlen(dirent->d_name) + 1];

      stpcpy(stpcpy(buf, MYLOCALES), dirent->d_name);

      if (stat(buf, &st) == 0)
        mode = st.st_mode;
    }

    result = S_ISDIR(mode);
  }

  return result;
}

static int select_dirs(const struct dirent *dirent) {
  int result = 0;
  if (strncmp(dirent->d_name, current_language, 3) == 0 && strcasecmp(&(dirent->d_name)[6], "utf8") == 0) {
    mode_t mode = 0;
    {
      struct stat st;
      char buf[sizeof (LOCALEDIR) + strlen(dirent->d_name) + 1];

      stpcpy(stpcpy(stpcpy(buf, LOCALEDIR), "/"), dirent->d_name);

      if (stat(buf, &st) == 0)
        mode = st.st_mode;
    }

    result = S_ISDIR(mode);
  }

  return result;
}

static char *get_language(void *mapped, size_t size) {
  char *str = NULL;

  /* Read the information from the file.  */
  struct {
    unsigned int magic;
    unsigned int nstrings;
    unsigned int strindex[0];
  } *filedata = mapped;

  if (filedata->magic == LIMAGIC(LC_IDENTIFICATION)
          && (sizeof *filedata
          + (filedata->nstrings
          * sizeof (unsigned int))
          <= size))
    str = ((char *) mapped
          + filedata->strindex[_NL_ITEM_INDEX(_NL_IDENTIFICATION_LANGUAGE)]);
  return str;
}

LOCALELIST *get_locales_language() {
  LOCALELIST *list = NULL;
  struct dirent **dirents;
  int ndirents;
  int cnt;

  ndirents = scandir(LOCALEDIR, &dirents, select_dirs, alphasort);

  // build languages list
  for (cnt = 0; cnt < ndirents; ++cnt) {
    LOCALELIST *ptr = calloc(sizeof (LOCALELIST), 1);
    if (!ptr) return NULL;
    ptr->locale_code = strdup(dirents[cnt]->d_name);
    {
      char buf[sizeof (LOCALEDIR) + strlen(dirents[cnt]->d_name)
              + sizeof "/LC_IDENTIFICATION"];
      char *enddir;
      struct stat st;
      stpcpy(enddir = stpcpy(stpcpy(stpcpy(buf, LOCALEDIR), "/"),
              dirents[cnt]->d_name),
              "/LC_IDENTIFICATION");
      if (stat(buf, &st) == 0 && S_ISREG(st.st_mode)) {
        int fd = open(buf, O_RDONLY);
        if (fd != -1) {
          void *mapped = mmap(NULL, st.st_size, PROT_READ,
                  MAP_SHARED, fd, 0);
          if (mapped != MAP_FAILED) {
            ptr->language_name = mysprintf("%s (%.2s)", get_language(mapped, st.st_size), &ptr->locale_code[3]);
            munmap(mapped, st.st_size);
          }
          close(fd);
        }
      }
    }
    append_localelist(&list, ptr);
  }
  // free direntries
  if (ndirents > 0) {
    while (ndirents--) {
      free(dirents[ndirents]);
    }
    free(dirents);
  }
  if (list == NULL) {
    list = calloc(sizeof (LOCALELIST), 1);
    if (!list) return NULL;
    list->locale_code = mysprintf("%s%.2s.utf8", current_language, current_language);
    list->locale_code[3] = toupper(list->locale_code[3]);
    list->locale_code[4] = toupper(list->locale_code[4]);
    list->language_name = strdup(list->locale_code);
  }
  return list;
}

LOCALELIST *get_locales_all() {
  LOCALELIST *list = NULL;
  struct dirent **dirents;
  int ndirents;
  int cnt;
  current_language[2] = '_';
  ndirents = scandir(MYLOCALES, &dirents, my_dirs, alphasort);

  list = calloc(sizeof (LOCALELIST), 1);
  if (!list) return NULL;
  list->locale_code = strdup("C");
  list->language_name = strdup("English (default)");

  for (cnt = 0; cnt < ndirents; ++cnt) {
    memcpy(current_language, dirents[cnt]->d_name, 2);
    append_localelist(&list, get_locales_language());
  }
  // free direntries
  if (ndirents > 0) {
    while (ndirents--) {
      free(dirents[ndirents]);
    }
    free(dirents);
  }
  return list;
}
/*
int main(int argc, char **argv)
{
  LOCALELIST *my_list, *tmp;

  printf("default locale: %s\n", getenv("LANG"));  
  setlocale(LC_ALL, argc > 1 ? argv[1] : "");
  bindtextdomain("test", MYLOCALES);
  textdomain("test");
  printf(_("Hello World!\n"));
  
  for (my_list = tmp = get_locales_all(); tmp != NULL; tmp = tmp->next)
    printf("code=%s\nlanguage=%s\n", tmp->locale_code, tmp->language_name);

  return 0;
}
 */