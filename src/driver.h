/*
 * Copyright (C) 2008-2015 by Erik Hofman.
 * Copyright (C) 2009-2015 by Adalin B.V.
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

#ifndef __DRIVER_H_
#define __DRIVER_H_

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <aax/aax.h>

#ifdef NDEBUG
# include <malloc.h>
#else
# include <base/logging.h>
#endif
#include <base/types.h>

void set_mode(int want_key);
int get_key();

char *getDeviceName(int, char **);
char *getCaptureName(int, char **);
char *getCommandLineOption(int, char **, char *);
char *getInputFile(int, char **, const char *);
char *getOutputFile(int, char**, const char *);
enum aaxFormat getAudioFormat(int, char **, enum aaxFormat);
int getNumEmitters(int, char **);
float getPitch(int, char **);
float getGain(int, char **);
int getMode(int, char **);
char *getRenderer(int, char **);
int printCopyright(int, char **);
char *strDup(const char *);

void testForError(void *, char *);
void testForState(int, const char *);
void testForALCError(void *);
void testForALError();

/* geometry */
float _vec3Magnitude(const aaxVec3f v);


#ifdef _WIN32
# include <windef.h>
DWORD __attribute__((__stdcall__)) SleepEx(DWORD,BOOL);
# define msecSleep(tms)	SleepEx((DWORD)tms, FALSE)
#else
int msecSleep(unsigned long);
#endif

#endif

