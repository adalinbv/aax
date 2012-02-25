#ifndef __DRIVER_H_
#define __DRIVER_H_

#include <base/types.h>
#include <base/logging.h>
#include <ringbuffer.h>

#undef NDEBUG
#ifdef NDEBUG
# include <malloc.h>
#else
# include <base/logging.h>
#endif

char *	getDeviceName(int, char **);
char *  getCaptureName(int, char **);
char *	getCommandLineOption(int, char **, char *);
char *	getInputFile(int, char **, const char *);
int	getNumSources(int, char **);
float	getPitch(int, char **);
float   getGain(int, char **);
int	getMode(int, char **);
char *	getRenderer(int, char **);
int     printCopyright(int, char **);

_aaxDriverBackend *getDriverBackend(int, char **, char **);

void testForError(void *, char *);
void testForState(int, const char *);
void testForALCError(void *);
void testForALError();

void nanoSleep(unsigned long);
char *strDup(const char *);

#endif

