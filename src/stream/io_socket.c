/*
 * Copyright 2015-2017 by Erik Hofman.
 * Copyright 2015-2017 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <error.h>

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
# include <ws2tcpip.h>
#endif
#ifndef WIN32
#define closesocket	close
#endif


#include <base/types.h>
#include <base/timer.h>

#include "audio.h"

int
_socket_open(_io_t *io, const char *server)
{
   int size = io->param[_IO_SOCKET_SIZE];
   int port = io->param[_IO_SOCKET_PORT];
   int timeout_ms = io->param[_IO_SOCKET_TIMEOUT];
   int fd = -1;

   if (server && (size > 4000) && (port > 0))
   {
      int slen = strlen(server);
      if (slen < 256)
      {
         struct addrinfo hints, *host;
         char sport[16];
         int res = 0;

#ifdef WIN32
// The WSAStartup function initiates use of the Winsock DLL by a process.
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
            if (timeout_ms < 5) timeout_ms = 5;
            fd = socket(host->ai_family, host->ai_socktype, host->ai_protocol);
            if (fd >= 0)
            {
               struct timeval tv;
               int on = 1;
               
               tv.tv_sec = timeout_ms / 1000;
               tv.tv_usec = (timeout_ms * 1000) % 1000000;              
               setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&on, sizeof(on));
               setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
               setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(int));
               io->error_max = (unsigned)(500.0f/timeout_ms); // 0.1 sec.
#if 0
 unsigned int m;
 int n;

 m = sizeof(n);
 getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&n, &m);
 printf("socket receive buffer size: %u\n", n);
#endif
               if (connect(fd, host->ai_addr, host->ai_addrlen) >= 0) {
                  io->fds.fd = fd;
               }
               else
               {
                  closesocket(fd);
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
_socket_close(_io_t *io)
{
   int rv = closesocket(io->fds.fd);
   io->fds.fd = -1;
   return rv;
}

ssize_t
_socket_read(_io_t *io, void *buf, size_t count)
{
   ssize_t rv = 0;

   assert(buf);
   assert(count);

   do {
      rv = recv(io->fds.fd, buf, count, 0);
   } while (rv < 0 && errno == EINTR);

   if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (++io->error_ctr < io->error_max) {
         rv = 0;
      }
   }
   else {
      io->error_ctr = 0;
   }

   return rv;
}

ssize_t
_socket_write(_io_t *io, const void *buf, size_t size)
{
   ssize_t rv = send(io->fds.fd, buf, size, 0);
   if (rv < 0 && errno == EINTR) rv = send(io->fds.fd, buf, size, 0);
   return rv;
}

void
_socket_wait(_io_t *io, float timeout_msec)
{
   poll(&io->fds, 1, (int)timeout_msec);
}

int
_socket_set(_io_t *io, enum _aaxStreamParam ptype, ssize_t param)
{
   int rv = -1;
   switch (ptype)
   {
   case __F_NO_BYTES:
   case __F_RATE:
   case __F_PORT:
   case __F_TIMEOUT:
      io->param[ptype - __F_NO_BYTES] = param;
      rv = 0;
      break;
   default:
      break;
   }
   return rv;
}

ssize_t
_socket_get(UNUSED(_io_t *io), enum _aaxStreamParam ptype)
{
   ssize_t rv = 0;
   switch (ptype)
   {
   case __F_NO_BYTES:
   case __F_POSITION:
   default:
      break;
   }
   return rv;
}
