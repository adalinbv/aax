/*
 * Copyright 2015 by Erik Hofman.
 * Copyright 2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>           /* read, write, close, lseek, access */
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <stdio.h>
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
#endif
#if HAVE_WINSOCK2_H
# include <w32api.h>
# define WINVER WindowsXP
# include <winsock2.h>
# include <ws2tcpip.h>
# define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#include <base/types.h>
#include <base/timer.h>

#include "socket.h"

int
_socket_open(const char *server, int rate, int port, int timeout_ms)
{
   int fd = -1;

   if (server && (rate > 4000) && (port > 0))
   {
      int slen = strlen(server);
      if (slen < 256)
      {
         struct addrinfo hints, *host;
         char sport[16];
         int res = 0;

#ifdef WIN32
         WSADATA wsaData;
         res = WSAStartup(MAKEWORD(1,1), &wsaData);
#endif

         if (res == 0)
         {
            snprintf(sport, 15, "%d", port);
            sport[15] = '\0';

            memset(&hints, 0, sizeof hints);
            hints.ai_socktype = SOCK_STREAM;
            res = getaddrinfo(server, (port > 0) ? sport : NULL, &hints, &host);
         }
         if (res == 0)
         {
            if (timeout_ms < 500) timeout_ms = 500;
            fd = socket(host->ai_family, host->ai_socktype, host->ai_protocol);
            if (fd >= 0)
            {
               int size = 4*rate*timeout_ms/1000.0f;
               struct timeval tv;
               
               tv.tv_sec = timeout_ms / 1000;
               tv.tv_usec = (timeout_ms * 1000) % 1000000;              
               setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, 0, 0);
               setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
               setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(int));
#if 0
unsigned int m;
int n;

m = sizeof(n);
getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&n, &m);
printf("socket receive buffer size: %u\n", n);
#endif
               if (connect(fd, host->ai_addr, host->ai_addrlen) < 0)
               {
                  close(fd);
                  fd = -1;
               }
            }
            freeaddrinfo(host);
         }
         else {
            errno = -res;
         }
      }
      else {
         errno = ENAMETOOLONG;
      }
   }
   else {
      errno = EACCES;
   }

   return fd;
}

int
_socket_close(int fd)
{
   return close(fd);
}

ssize_t
_socket_read(int fd, void *buf, size_t size)
{
   ssize_t res = recv(fd, buf, size, 0);
   if ((res < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      res = 0;
   }
   return res;
}

ssize_t
_socket_write(int fd, const void *buf, size_t size)
{
   return send(fd, buf, size, 0);
}

off_t
_socket_seek(int fd, off_t offs, int whence)
{
   errno = EPERM;
   return (off_t)-1;
}

int
_socket_stat(int fd, struct stat *stat)
{
   errno = EPERM;
   return -1;
}

