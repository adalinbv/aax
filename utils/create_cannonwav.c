/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *
 * This file is in the Public Domain and comes with no warranty.
 * Erik Hofman <erik@ehofman.com>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>


#ifdef __APPLE__
# include <OpenAL/al.h>
# include <OpenAL/alc.h>
#else
# include <AL/al.h>
# include <AL/alc.h>
# include <AL/alext.h>
#endif

#ifndef AL_VERSION_1_1
# ifdef __APPLE__
#  include <OpenAL/altypes.h>
#  include <OpenAL/alctypes.h>
#else
#  include <AL/altypes.h>
#  include <AL/alctypes.h>
# endif
#endif
#include <AL/alut.h>

#include "base/types.h"
#include "base/logging.h"

void *fileLoad(char *, unsigned int *, unsigned int *, char *, char *);
void fileWrite(char *, void *, unsigned int, float, char, char);

#define WAVE_HEADER_SIZE        11
#define DEFAULT_OUTPUT_RATE	32000
const char outfile[1204] = SRC_PATH"/stereo.wav";
const char infile[1024] = SRC_PATH"/fuzzy.wav";


void testForError(void *p, char *s)
{
   if (p == NULL)
   {
     printf("\n%s\n", s);
     exit(-1);
   }
}

void testForALCError(ALCdevice *device)
{
    ALenum error;
    error = alcGetError(device);
    if (error != ALC_NO_ERROR)
      printf("\nAn ALC Error occurred: #%x\n", error);
}


int main(int argc, char **argv)
{
   if (alutInit(&argc, argv))
   {
      if (argc > 0)
      {
         strncpy((char *)&infile, argv[1], 1024);
      }

      if (argc > 2)
      {
         strncpy((char *)&outfile, argv[2], 1024);
      }
 
      if (1)
      {
         void *buffer;
         ALenum format;
         int size;
         float freq;

         buffer = alutLoadMemoryFromFile(infile, &format, &size, &freq);
         if (buffer != AL_NONE)
         {
             char bps, channels;
             switch(format)
             {
             case AL_FORMAT_STEREO8:
                channels = 2;
             case AL_FORMAT_MONO8:
                bps = 1;
                break;
             case AL_FORMAT_STEREO16:
                channels = 2;
             case AL_FORMAT_MONO16:
                bps = 2;
                break;
             }
             fileWrite((char *)&outfile, buffer, size, freq, bps, channels);

         } else
            printf("File not found: %s\n", infile);

         free(buffer);

      } else
         printf("Unable to open the output file.\n");

      alutExit();

   } else
       printf("Unable to initialize ALUT:\n\t%s",
                   alutGetErrorString(alutGetError()));

   return 0;
}


/* -------------------------------------------------------------------------- */

unsigned int _bswap32(unsigned int x)
{
   x = ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
   x = (x >> 16) | (x << 16);
   return x;
}

unsigned int _bswap32h(unsigned int x)
{
   x = ((x >>  8) & 0x00FF00FFL) | ((x <<  8) & 0xFF00FF00L);
   return x;
}

unsigned int _bswap32w(unsigned int x)
{
   x = (x >> 16) | (x << 16);
   return x;
}
