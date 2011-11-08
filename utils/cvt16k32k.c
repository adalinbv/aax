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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

#include "base/types.h"
#include "base/logging.h"
#include "base/malloc/rmalloc.h"
#include "../test/wavfile.h"

static const char infile[] = "/tmp/audiodump.wav";



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
   ALCdevice *device = NULL;
   ALCcontext *context = NULL;

   device = alcOpenDevice("Software");
   testForError(device, "No default audio device available.");

   context = alcCreateContext(device, NULL);
   testForError(context, "Unable to create a valid context.");

   alcMakeContextCurrent(context);
   testForALCError(device);

   do {
      unsigned int size;
      char bps, channels;
      void *data;
      int freq;

      data = fileLoad((char *)&infile, &size, &freq, &bps, &channels);
      testForError(data, "Input file not found.\n");

      do {
         struct timespec sleept = { 5e7, 0 };
         ALuint buffer, source;
         ALenum format;
         ALint status;

         if      ((bps == 1) && (channels == 1)) format = AL_FORMAT_MONO8;
         else if ((bps == 1) && (channels == 2)) format = AL_FORMAT_STEREO8;
         else if ((bps == 2) && (channels == 1)) format = AL_FORMAT_MONO16;
         else if ((bps == 2) && (channels == 2)) format = AL_FORMAT_STEREO16;
         else break;

         alGetError();
         alGenBuffers(1, &buffer);
         alBufferData(buffer, format, data, size, freq);
         free(data);
         testForALError();

         alGenSources(1, &source);
         testForALError();
         alSourcei(source, AL_BUFFER, buffer);

         alSourcePlay(source);
         do {
            nanosleep(&sleept, 0);
            alGetSourcei(source, AL_SOURCE_STATE, &status);
         } while (status == AL_PLAYING);

         alDeleteSources(1, &source);
         alDeleteBuffers(1, &buffer);

      } while (0);

   } while(0);

   context = alcGetCurrentContext();
   device = alcGetContextsDevice(context);
   testForALCError(device);

   alcMakeContextCurrent(NULL);
   alcDestroyContext(context);
   alcCloseDevice(device);

   return 0;
}
