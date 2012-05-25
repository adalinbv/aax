/*
 * Copyright (C) 2005-2011 by Erik Hofman.
 * Copyright (C) 2007-2011 by Adalin B.V.
 *
 * This file is part of OpenAL-AeonWave.
 *
 *  OpenAL-AeonWave is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenAL-AeonWave is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenAL-AeonWave.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AAX_LOGGING_H
#define __AAX_LOGGING_H 1

#if HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(HAVE_RMALLOC_H) && defined(USE_RMALLOC)
# define USE_LOGGING	1
# include <stdio.h>
# include <rmalloc.h>
#else
# define USE_LOGGING	0
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif

#ifdef HAVE_SYSLOG_H
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

#define PRINT_VEC(vec) \
	printf ("% 7.6f % 7.6f % 7.6f % 7.6f\n", \
             vec[0],vec[1],vec[2],vec[3]);

#define PRINT_ROW(mtx, r, c) \
    printf ("% 6.3f % 6.3f % 6.3f % 6.3f%c", \
             mtx[0][r],mtx[1][r],mtx[2][r],mtx[3][r],c);

#define PRINT_MATRIX(mtx) \
    PRINT_ROW(mtx, 0, '\n'); PRINT_ROW(mtx, 1, '\n'); \
    PRINT_ROW(mtx, 2, '\n'); PRINT_ROW(mtx, 3, '\n');

#define PRINT_MATRICES(m1, m2) \
    PRINT_ROW(m1, 0, '\t'); PRINT_ROW(m2, 0, '\n'); \
    PRINT_ROW(m1, 1, '\t'); PRINT_ROW(m2, 1, '\n'); \
    PRINT_ROW(m1, 2, '\t'); PRINT_ROW(m2, 2, '\n'); \
    PRINT_ROW(m1, 3, '\t'); PRINT_ROW(m2, 3, '\n');


void __oal_log(int level, int id, const char *s, const char *id_s[], int current_level);

#if USE_LOGGING

# define __AAX_LOG(a, b, c, d, e) __oal_log((a), (b), (c), (d), (e))

# define LOG_PREFIX_FL	"%s at %i\n\t\t\t\t\t"
# define __THD_LOG_FL(str, m, n, f, l)                             \
   do {                                                            \
      char s[100];                                                 \
      snprintf(s, 99, LOG_PREFIX_FL""str":  %s (%x)", f, l, n, (unsigned int)m); \
      __AAX_LOG(LOG_INFO, 0, s);                                   \
   } while(0);

# define LOG_PREFIX     "\t\t\t\t\t\t"
# define __THD_LOG(str, m)                        \
   do {                                           \
      char s[100];                                \
      snprintf(s, 99, LOG_PREFIX""str":  %x", (unsigned int)m); \
      __AAX_LOG(LOG_INFO, 0, s);                  \
   } while(0);

#else /* USE_LOGGING */
# define __AAX_LOG(a, b, c, d)
# define __THD_LOG(str, m)
#endif

#endif /* !__AAX_LOGGING_H */

