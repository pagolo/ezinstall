#ifndef CONDITIONS_H
#define CONDITIONS_H

#ifndef HAVE_STDLIB_H
#error "Sorry, you need stdlib.h"
#endif
#ifndef HAVE_LIBMYSQLCLIENT
#error "Sorry, you need libmysqlclient library"
#endif
#if !defined(HAVE_STRCASECMP) && defined(HAVE_STRICMP)
#define strcasecmp stricmp
#endif

#endif /* CONDITIONS_H */

