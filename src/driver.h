/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *
 * This file is in the Public Domain and comes with no warranty.
 * Erik Hofman <erik@ehofman.com>
 *
 */

#ifndef __DRIVER_H_
#define __DRIVER_H_

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <aax.h>

#ifdef NDEBUG
# include <malloc.h>
#else
# include <base/logging.h>
#endif
#include <base/types.h>

char *getDeviceName(int, char **);
char *getCaptureName(int, char **);
char *getCommandLineOption(int, char **, char *);
char *getInputFile(int, char **, const char *);
int getNumSources(int, char **);
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

#define nanoSleep(a)	msecSleep((long)a/1000000)

#endif

