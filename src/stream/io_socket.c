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
# include <winsock2.h>
# include <ws2tcpip.h>
# define EWOULDBLOCK WSAEWOULDBLOCK
#endif
#ifndef WIN32
#define closesocket	close
#endif


#include <base/types.h>
#include <base/timer.h>

#include "io.h"
#include "extension.h"

int
_socket_open(_io_t *io, const char *server)
{
   int rate = io->param[_IO_SOCKET_RATE];
   int port = io->param[_IO_SOCKET_PORT];
   int timeout_ms = io->param[_IO_SOCKET_TIMEOUT];
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
               if (connect(fd, host->ai_addr, host->ai_addrlen) >= 0) {
                  io->fd = fd;
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
   int rv = closesocket(io->fd);
   io->fd = -1;
   return rv;
}

ssize_t
_socket_read(_io_t *io, void *buf, size_t count)
{
   ssize_t rv = recv(io->fd, buf, count, 0);
   if (rv == EINTR) rv = recv(io->fd, buf, count, 0);
   if ((rv < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      rv = 0;
   }
   return rv;
}

ssize_t
_socket_write(_io_t *io, const void *buf, size_t size)
{
   ssize_t rv = send(io->fd, buf, size, 0);
   if (rv == EINTR) rv = send(io->fd, buf, size, 0);
   return rv;
}

int
_socket_set(_io_t *io, enum _aaxStreamParam ptype, ssize_t param)
{
   int rv = -1;
   switch (ptype)
   {
   case __F_RATE:
   case __F_PORT:
   case __F_TIMEOUT:
      io->param[ptype - __F_RATE] = param;
      rv = 0;
      break;
   default:
      break;
   }
   return rv;
}

ssize_t
_socket_get(_io_t *io, enum _aaxStreamParam ptype)
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
