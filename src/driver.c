/*
 * Copyright (C) 2008-2012 by Erik Hofman.
 * Copyright (C) 2009-2012 by Adalin B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY ADALIN B.V. ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL ADALIN B.V. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUTOF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Adalin B.V.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* strrchr */
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>	/* access */
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <base/logging.h>

#include "driver.h"


char *
getDeviceName(int argc, char **argv)
{
    static char devname[255];
    int len = 255;
    char *s;

    /* -d for device name */
    s = getCommandLineOption(argc, argv, "-d");
    if (s)
    {
        strncpy((char *)&devname, s, len);
        len -= strlen(s);

        /* -r for a separate renderer */
        s = getCommandLineOption(argc, argv, "-r");
        if (s)
        {
            strncat((char *)&devname, " on ", len);
            len -= 4;

            strncat((char *)&devname, s, len);
        }
        s = (char *)&devname;
    }

    return s;
}

char *
getCaptureName(int argc, char **argv)
{
    static char devname[255];
    int len = 255;
    char *s;

   /* -c for a capture device */
    s = getCommandLineOption(argc, argv, "-c");
    if (s)
    {
        strncpy((char *)&devname, s, len);
        len -= strlen(s);
    }

    return s;
}

char *
getRenderer(int argc, char **argv)
{
    char *renderer = 0;

    /* -r for a separate renderer */
    renderer = getCommandLineOption(argc, argv, "-r");
    return renderer;
}

int
getNumEmitters(int argc, char **argv)
{
    int num = 1;
    char *ret;

    /* -n for the number of emitters */
    ret = getCommandLineOption(argc, argv, "-n");
    if (ret) num = atoi(ret);
    return num;
}

float
getPitch(int argc, char **argv)
{
    float num = 1.0f;
    char *ret = getCommandLineOption(argc, argv, "-p");
    if (ret) num = (float)atof(ret);
    return num;
}

float
getGain(int argc, char **argv)
{
    float num = 1.0f;
    char *ret = getCommandLineOption(argc, argv, "-g");
    if (ret) num = (float)atof(ret);
    return num;
}

int
printCopyright(int argc, char **argv)
{
    char *ret = getCommandLineOption(argc, argv, "-c");
    if (ret) {
        printf("%s\n", aaxGetCopyrightString());
    }
    return ret ? -1 : 0;
}

int
getMode(int argc, char **argv)
{
    int mode = AAX_MODE_WRITE_STEREO;
    char *ret = getCommandLineOption(argc, argv, "-m");
    if (ret)
    {
        if (!strcasecmp(ret, "hrtf")) mode = AAX_MODE_WRITE_HRTF;
        else if (!strcasecmp(ret, "spatial")) mode = AAX_MODE_WRITE_SPATIAL;
        else if (!strcasecmp(ret, "surround")) mode = AAX_MODE_WRITE_SURROUND;
    }
    return mode;
}

char *
getInputFile(int argc, char **argv, const char *filename)
{
    char *fn = getCommandLineOption(argc, argv, "-i");

    if (!fn) fn = (char *)filename;
    if (access(fn, F_OK|R_OK) < 0) fn = NULL;
    return fn;
}

void
testForError(void *p, char *s)
{
    if (p == NULL)
    {
        int err = aaxGetErrorNo();
        printf("\nError: %s\n", s);
        if (err) {
            printf("%s\n\n", aaxGetErrorString(err));
        }
        exit(-1);
    }
}

void
testForState(int res, const char *func)
{
    if (res != AAX_TRUE)
    {
        int err = aaxGetErrorNo();
        printf("%s:\t\t%i\n", func, res);
        printf("(%i) %s\n\n", err, aaxGetErrorString(err));
        exit(-1);
    }
}

char *
getCommandLineOption(int argc, char **argv, char *option)
{
    int slen = strlen(option);
    char *rv = 0;
    int i;
    
    for (i=0; i<argc; i++)
    {
        if (strncmp(argv[i], option, slen) == 0)
        {
            rv = "";
            rv = "";
            i++;
            if (i<argc) rv = argv[i];
        }
    }

    return rv;
}

#ifndef _WIN32
# include <termios.h>
# include <unistd.h>
# include <fcntl.h>
# include <sys/time.h>
# include <time.h>

int msecSleep(unsigned long dt_ms)
{
    static struct timespec s;
    s.tv_sec = (dt_ms/1000);
    s.tv_nsec = (dt_ms-s.tv_sec*1000)*1000000;
    return nanosleep(&s, 0);
}

void set_mode(int want_key)
{
    static struct termios old, new;
    if (!want_key) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
        return;
    }

    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
}

int get_key()
{
    int c = 0;
    struct timeval tv;
    fd_set fs;
    tv.tv_usec = tv.tv_sec = 0;

    FD_ZERO(&fs);
    FD_SET(STDIN_FILENO, &fs);
    select(STDIN_FILENO + 1, &fs, 0, 0, &tv);

    if (FD_ISSET(STDIN_FILENO, &fs)) {
        c = getchar();
        set_mode(0);
    }
    return c;
}

#else

# include <conio.h>

int get_key()
{
   if (kbhit()) {
      return getch();
   }
   return 0;
}

void set_mode(int want_key)
{
}
#endif

char *strDup(const char *s)
{
    unsigned int len = strlen(s)+1;
    char *p = malloc(len);
    if (p) memcpy(p, s,len);
    return p;
}
