/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2007-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#pragma once

#if HAVE_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef __USE_GNU
# undef __USE_GNU
#endif
#ifdef HAVE_POLL_H
# include <poll.h>

#elif HAVE_WINSOCK2_H
#  include <ws2tcpip.h>
typedef int nfds_t;

# ifndef POLLRDNORM
#  define POLLRDNORM    0x0100
# endif

# ifndef POLLRDBAND
#  define POLLRDBAND    0x0200
# endif

# ifndef POLLIN
#  define POLLIN        (POLLRDNORM|POLLRDBAND)
# endif

# ifndef POLLWRNORM
#  define POLLWRNORM    0x0010
# endif

# ifndef POLLOUT
#  define POLLOUT       (POLLWRNORM)
# endif

# ifndef POLLERR
#  define POLLERR       0x0001
# endif

# ifndef POLLNVAL
#  define POLLNVAL      0x0004
# endif

# ifndef _IOC
#  define _IOC(inout,group,num,len) \
              (inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num))
# endif

# ifndef _IOWR
#  define _IOWR(g,n,t)  _IOC(IOC_IN | IOC_OUT,  (g), (n), sizeof(t))
# endif
#endif

#if !defined(HAVE_POLL_H) && !defined(HAVE_WINDOWS_H)
#define POLLIN  0
struct pollfd {
   int fd;
   short events;
   short revents;
};
#endif
