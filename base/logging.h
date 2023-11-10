/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __OAL_LOGGING_H
#define __OAL_LOGGING_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef USE_RMALLOC
# define USE_LOGGING	1
# include <stdio.h>
# include <rmalloc.h>
#else
# define USE_LOGGING	0
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#if HAVE_SYSLOG_H
# include <syslog.h>
#else
# define LOG_EMERG	0	/* system is unusable */
# define LOG_ALERT	1	/* action must be taken immediately */
# define LOG_CRIT	2	/* critical conditions */
# define LOG_ERR	3       /* error conditions */
# define LOG_WARNING	4	/* warning conditions */
# define LOG_NOTICE	5	/* normal but significant condition */
# define LOG_INFO	6	/* informational */
# define LOG_DEBUG	7	/* debug-level messages */

# define LOG_SYSLOG	40
#endif

#define PRINT_VEC3PTR(vec) \
        printf ("% 7.6f % 7.6f % 7.6f (%7.6f)\n", \
             vec->v3[0],vec->v3[1],vec->v3[2],_MINMAX(vec->v3[3],0,1));

#define PRINT_VEC3(vec)	\
	printf ("% 7.6f % 7.6f % 7.6f (%7.6f)\n", \
	    vec.v3[0],vec.v3[1],vec.v3[2],_MINMAX(vec.v3[3],0,1));

#define PRINT_VECPTR(vec) \
	printf ("% 7.6f % 7.6f % 7.6f % 7.6f\n", \
	    vec->v4[0],vec->v4[1],vec->v4[2],vec->v4[3]);

#define PRINT_VEC(vec) \
	printf ("% 7.6f % 7.6f % 7.6f % 7.6f\n", \
            vec.v4[0],vec.v4[1],vec.v4[2],vec.v4[3]);

#define PRINT_ROW(mtx, r, c) \
	printf ("% 6.3f % 6.3f % 6.3f % 6.3f%c", \
            mtx.m4[0][r],mtx.m4[1][r],mtx.m4[2][r],mtx.m4[3][r],c);

#define PRINT_MATRIX(mtx) \
    PRINT_ROW(mtx, 0, '\n'); PRINT_ROW(mtx, 1, '\n'); \
    PRINT_ROW(mtx, 2, '\n'); PRINT_ROW(mtx, 3, '\n');

#define PRINT_MATRICES(m1, m2) \
    PRINT_ROW(m1, 0, '\t'); PRINT_ROW(m2, 0, '\n'); \
    PRINT_ROW(m1, 1, '\t'); PRINT_ROW(m2, 1, '\n'); \
    PRINT_ROW(m1, 2, '\t'); PRINT_ROW(m2, 2, '\n'); \
    PRINT_ROW(m1, 3, '\t'); PRINT_ROW(m2, 3, '\n');


extern void (*_sys_free)(void*);

int _aax_getbool(const char*);
const char* _aax_get_binary_name(const char*);
void __aax_log(int level, int id, const char *s, const char *id_s[], int current_level);

#if USE_LOGGING

# define __OAL_LOG(a, b, c, d, e) __aax_log((a), (b), (c), (d), (e))

# define LOG_PREFIX_FL	"%s at %i\n\t\t\t\t\t"
# define __THD_LOG_FL(str, m, n, f, l)                             \
   do {                                                            \
      char s[100];                                                 \
      snprintf(s, 99, LOG_PREFIX_FL""str":  %s (%x)", f, l, n, (unsigned int)m); \
      __OAL_LOG(LOG_INFO, 0, s);                                   \
   } while(0);

# define LOG_PREFIX     "\t\t\t\t\t\t"
# define __THD_LOG(str, m)                        \
   do {                                           \
      char s[100];                                \
      snprintf(s, 99, LOG_PREFIX""str":  %x", (unsigned int)m); \
      __OAL_LOG(LOG_INFO, 0, s);                  \
   } while(0);

#else /* USE_LOGGING */
# define __OAL_LOG(a, b, c, d)
# define __THD_LOG(str, m)
#endif

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !__OAL_LOGGING_H */

