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
#include <unistd.h>
#include <time.h>

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

#include "base/logging.h"
#include "base/types.h"
#include "driver.h"
#include "wavfile.h"

#define MAXSRC			32
#define FILE_PATH		SRC_PATH"/stereo.wav"

int main(int argc, char **argv)
{
   ALCdevice *device = NULL;
   ALCcontext *context = NULL;
   char *devname, *infile;

   infile = getInputFile(argc, argv, FILE_PATH);
   devname = getDeviceName(argc, argv);

   device = alcOpenDevice(devname);
   testForError(device, "No default audio device available.");

   context = alcCreateContext(device, NULL);
   testForError(context, "Unable to create a valid context.");

   alcMakeContextCurrent(context);
   testForALCError(device);

   do {
      ALuint pos, buffer, source[MAXSRC];
      unsigned int randfact = RAND_MAX/MAXSRC;
      unsigned int q, no_samples, freq;
      char bps, channels, data[1024];
      ALenum format;

      bps = 1;
      channels = 1;
      no_samples = 1024;
      freq = 44100;
      format = AL_FORMAT_MONO8;

      alGetError();
      alGenBuffers(1, &buffer);
      alBufferData(buffer, format, &data, no_samples*bps, freq);

      testForALError();

      srand((unsigned)time(0));
      pos = rand()/randfact;

      memset(source, 0, MAXSRC * sizeof(ALuint));

      for (q=0; q<255; q++)
      {
         alGenSources(1, &source[pos]);
         /* testForALError(); */

#if 1
         printf("%4i; source[%2i] = %i\n", q, pos, source[pos]);
#endif

         alSourcei(source[pos], AL_BUFFER, buffer);
         testForALError();

         pos = rand()/randfact;
         if (source[pos] != 0)
         {
            alDeleteSources(1, &source[pos]);
            source[pos] = 0;
            testForALError();
         }
      }
      printf("\n");
         
#if 1
      for (pos=0; pos<MAXSRC; pos++)
         if(source[pos] != 0)
            alDeleteSources(1, &source[pos]);
#else
      alDeleteSources(MAXSRC, source);
#endif
      alDeleteBuffers(1, &buffer);

   } while(0);

   context = alcGetCurrentContext();
   device = alcGetContextsDevice(context);
   testForALCError(device);

   alcMakeContextCurrent(NULL);
   alcDestroyContext(context);
   alcCloseDevice(device);

   return 0;
}
