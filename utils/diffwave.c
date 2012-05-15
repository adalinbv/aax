/* -*- mode: C; tab-width:8; c-basic-offset:8 -*-
 * vi:set ts=8:
 *
 * This file is in the Public Domain and comes with no warranty.
 * Erik Hofman <erik@ehofman.com>
 *
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

#include "base/types.h"
#include "base/logging.h"
#include "base/malloc/rmalloc.h"

#include "../test/wavfile.h"

static const char infile1[] = "oal-audio-nearest.wav";
static const char infile2[] = "oal-audio.wav";
static const char outfile[] = "oal-audio-diff.wav";


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

void testForALError()
{
   ALenum error;
   error = alGetError();
    if (error != AL_NO_ERROR)
      printf("\nAn AL Error occurred: #%x\n", error);
}


int main()
{
   unsigned int i, size1, size2;
   char bps1, bps2, tracks1, tracks2;
   uint16_t *data1, *data2;
   int freq1, freq2;
   float src_step;

   data1 = fileLoad((char *)&infile1, &size1, &freq1, &bps1, &tracks1);
   testForError(data1, "Input file1 not found.\n");

   data2 = fileLoad((char *)&infile2, &size2, &freq2, &bps2, &tracks2);
   testForError(data2, "Input file2 not found.\n");

   assert(bps1 == sizeof(uint16_t));
   assert(bps2 == sizeof(uint16_t));
   assert(tracks1 == tracks2);

   src_step = freq1 / freq2;

#if 0
printf("size1: %i\tsize2: %i\n", (unsigned int)floor(src_step*size1), size2);
   assert(size2 = (unsigned int)floor(src_step * size1));
#endif

   size2 /= bps2;
   for (i=0; i<size2; i++)
   {
      unsigned int pos = i*src_step;
      uint16_t tmp = data1[pos];
      data2[i] -= tmp;
   }

   fileWrite((char *)&outfile, data2, size2, freq2, bps2, tracks2);

   free(data2);
   free(data1);

   return 0;
}
