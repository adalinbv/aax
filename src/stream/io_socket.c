/*
 * Copyright 2015-2021 by Erik Hofman.
 * Copyright 2015-2021 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
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
# include <unistd.h>		/* read, write, close, lseek, access */
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
# include <arpa/inet.h>		/* inet_ntop */
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
#include <base/dlsym.h>

#include "io.h"

DECL_FUNCTION(OPENSSL_init_ssl);
DECL_FUNCTION(SSL_new);
DECL_FUNCTION(SSL_free);
DECL_FUNCTION(SSL_CTX_new);
DECL_FUNCTION(SSL_CTX_free);
DECL_FUNCTION(SSL_set_fd);
DECL_FUNCTION(SSL_connect);
DECL_FUNCTION(SSL_shutdown);
DECL_FUNCTION(SSL_read);
DECL_FUNCTION(SSL_write);
DECL_FUNCTION(TLS_client_method);
DECL_FUNCTION(SSL_CIPHER_get_name);
DECL_FUNCTION(SSL_get_current_cipher);
DECL_FUNCTION(SSL_get_error);

int
_socket_open(_io_t *io, _data_t *buf, const char *remote, const char *pathname)
{
   static int recursive = 0;
   static void *audio = NULL;
   int size = io->param[_IO_SOCKET_SIZE];
   int port = io->param[_IO_SOCKET_PORT];
   int timeout_ms = io->param[_IO_SOCKET_TIMEOUT];
   int res, fd = -1;

   recursive++;

#ifdef WIN32
// The WSAStartup function initiates use of the Winsock DLL by a process.
   WSADATA wsaData;
   res = WSAStartup(MAKEWORD(1,1), &wsaData);
   if (res)
   {
      errno = -res;
      return fd;
   }
#endif

   audio = _aaxIsLibraryPresent("ssl", "1.1");
   if (audio)
   {
      _aaxGetSymError(0);

      TIE_FUNCTION(SSL_new);
      if (pSSL_new)
      {
         TIE_FUNCTION(SSL_free);
         TIE_FUNCTION(OPENSSL_init_ssl);
         TIE_FUNCTION(SSL_CTX_new);
         TIE_FUNCTION(SSL_CTX_free);
         TIE_FUNCTION(SSL_set_fd);
         TIE_FUNCTION(SSL_connect);
         TIE_FUNCTION(SSL_shutdown);
         TIE_FUNCTION(SSL_read);
         TIE_FUNCTION(SSL_write);
         TIE_FUNCTION(TLS_client_method);
         TIE_FUNCTION(SSL_CIPHER_get_name);
         TIE_FUNCTION(SSL_get_current_cipher);
         TIE_FUNCTION(SSL_get_error);
      }
   }

   if (timeout_ms < 5) {
      timeout_ms = 5;
   }
   io->error_max = (unsigned)(3000.0f/timeout_ms); // 3.0 sec.

   if (remote && (size > 4000) && (port > 0))
   {
      int slen = strlen(remote);
      if (slen < 256)
      {
         // Two tries, first a secure connection, then a plain text connection
         int tries = 2;

         do
         {
            struct hostent *host;

            errno = 0;
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd >= 0)
            {
               struct timeval tv;
               int on = 1;

               tv.tv_sec = timeout_ms / 1000;
               tv.tv_usec = (timeout_ms * 1000) % 1000000;
#ifdef SO_NOSIGPIPE
               setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
               setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&on,sizeof(on));
               if (tries == 1) {
                  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv,
                                                          sizeof(tv));
               }
               setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&size,sizeof(size));
#if 0
 unsigned int m;
 int n;

 printf("timeout_ms: %i\n", timeout_ms);
 printf("error_max: %i\n", io->error_max);

 m = sizeof(n);
 getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, &m);
 printf("socket receive buffer size: %u\n", n);
#endif

               host = gethostbyname(remote);
               if (host)
               {
                  struct sockaddr_in dest_addr;

                  dest_addr.sin_family = AF_INET;
                  dest_addr.sin_port = htons(port);
                  dest_addr.sin_addr.s_addr = *(long*)(host->h_addr);
                  memset(&(dest_addr.sin_zero), '\0', 8);

#if 0
{
 char buffer[INET_ADDRSTRLEN];
 inet_ntop( AF_INET, &dest_addr.sin_addr, buffer, sizeof( buffer ));
 printf( "address:%s\n", buffer );
}
#endif
                  if (connect(fd, (struct sockaddr*)&dest_addr,
                                  sizeof(struct sockaddr)) >= 0)
                  {
                     io->fds.fd = fd;

                     if (io->protocol == PROTOCOL_HTTPS &&
                         pSSL_new && tries == 2)
                     {
                        void *method = pTLS_client_method();
                        io->ssl_ctx = pSSL_CTX_new(method);
                        if (io->ssl_ctx)
                        {
                           io->ssl = pSSL_new(io->ssl_ctx);
                           if (io->ssl)
                           {
                              pSSL_set_fd(io->ssl, fd);
                              res = pSSL_connect(io->ssl);
                              if (res > 0) break;

                              pSSL_free(io->ssl);
                              io->ssl = NULL;
                           }
                        }

                        pSSL_CTX_free(io->ssl_ctx);
                        closesocket(fd);
                        fd = -1;
                     }
                  }
               }
               else
               {
                  closesocket(fd);
                  fd = -1;
               }
            }

            msecSleep(200);
         }
         while (--tries);

         if (fd != -1 && recursive < 5)
         {
            const char *agent = aaxGetVersionString(NULL);
            char *protname, *server, *extension;
            char *path = (char*)pathname;
            char *s = (char*)remote;

            res = io->prot->connect(io->prot, buf, io, &s, path, agent);
            if (res == -300)
            {
               _protocol_t protocol;

               io->prot = _prot_free(io->prot);
               _socket_close(io);

               protocol = _url_split(s, &protname, &server, &path,
                                        &extension, &port);
#if 0
 printf("\nredirect name: '%s'\n", remote);
 printf("recursive: %i\n", recursive);
 printf("protocol: '%s'\n", protname);
 printf("server: '%s'\n", server);
 printf("path: '%s'\n", path);
 printf("ext: '%s'\n", extension);
 printf("port: %i\n", port);
#endif
               io->prot = _prot_create(protocol);
               if (io->prot) { // recursively call _socket_open
                  fd = _socket_open(io, buf, server, path);
               }
            }
         }
      }
      else {
         errno = ENAMETOOLONG;
      }
   }
   else {
      errno = EACCES;
   }

   recursive--;

   return fd;
}

int
_socket_close(_io_t *io)
{
   int rv;

   if (io->ssl)
   {
      pSSL_shutdown(io->ssl);
      pSSL_free(io->ssl);
      pSSL_CTX_free(io->ssl_ctx);
      io->ssl = NULL;
   }

   rv = closesocket(io->fds.fd);
   io->fds.fd = -1;
   return rv;
}

ssize_t
_socket_read(_io_t *io, _data_t *buf, size_t count)
{
   size_t size = _MIN(count, _aaxDataGetFreeSpace(buf));
   void *ptr = _aaxDataGetPtr(buf);
   ssize_t rv = 0;

   if (size)
   {
      if (io->ssl) {
         rv = pSSL_read(io->ssl, ptr, size);
      }
      else
      {
         do {
            rv = recv(io->fds.fd, ptr, size, 0);
         } while (rv < 0 && errno == EINTR);
      }

      if (rv >= 0)
      {
         io->error_ctr = 0;
         _aaxDataIncreaseOffset(buf, rv);

         if (io->prot) {
            rv = io->prot->process(io->prot, buf);
         }
      }
      else if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
         if (++io->error_ctr < io->error_max) {
            rv = 0;
         }
      }
   }

   return rv;
}

ssize_t
_socket_write(_io_t *io, _data_t *buf)
{
   ssize_t rv = _aaxDataGetDataAvail(buf);
   if (rv > 0)
   {
      void *data = _aaxDataGetData(buf);
      ssize_t res = 0;

      if (io->ssl) {
         res = pSSL_write(io->ssl, data, rv);
      }
      else if (io->fds.fd >= 0)
      {
         res = send(io->fds.fd, data, rv, 0);
         if (res < 0 && errno == EINTR) {
            res = send(io->fds.fd,data, rv, 0);
         }
      }

      if (res > 0) {
         rv = _aaxDataMove(buf, NULL, res);
      }
   }

   return rv;
}

void
_socket_wait(_io_t *io, float timeout_msec)
{
#if HAVE_POLL_H
   poll(&io->fds, 1, (int)timeout_msec);
#endif
}

int
_socket_set(_io_t *io, enum _aaxStreamParam ptype, ssize_t param)
{
   int rv = -1;
   switch (ptype)
   {
   case __F_NO_BYTES:
      io->param[_IO_SOCKET_SIZE] = param;
      rv = 0;
      break;
   case __F_RATE:
      io->param[_IO_SOCKET_RATE] = param;
      rv = 0;
      break;
   case __F_PORT:
      io->param[_IO_SOCKET_PORT] = param;
      rv = 0;
      break;
   case __F_TIMEOUT:
      io->param[_IO_SOCKET_TIMEOUT] = param;
      rv = 0;
      break;
   case __F_POSITION:
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
   case __F_POSITION:
      rv = -1;
      break;
   case __F_NO_BYTES:
      break;
   default:
      if (io->prot) {
         rv = io->prot->get_param(io->prot, ptype);
      }
      break;
   }
   return rv;
}

char*
_socket_name(_io_t *io, enum _aaxStreamParam ptype)
{
   char *rv = NULL;
   if (io->prot) {
      rv = io->prot->name(io->prot, ptype);
   }
   return rv;
}
