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

#define  DEVICE_NAME	"Software"	/* NULL, "Software", "DMedia", "ALSA" */
#if 0
#undef DEVICE_NAME
#define  DEVICE_NAME	"Software"
#endif

#include <stdio.h>
#include <unistd.h>
#include <math.h>

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

#include "driver.h"
#include "wavfile.h"

#define DEFAULT_INFILE		SRC_PATH"/sqr-1kHz.wav"
static char infile[1024];

#ifndef M_E
# define M_E			2.7182818284590452354
#endif

int main(int argc, char **argv)
{
   ALCdevice *device = NULL;
   ALCcontext *context = NULL;

   if (argc > 1)
      strncpy(infile, argv[1], 1024);
   else
      strncpy(infile, DEFAULT_INFILE, 1024);

   device = alcOpenDevice(DEVICE_NAME);
   testForError(device, "No default audio device available.");

   context = alcCreateContext(device, NULL);
   testForError(context, "Unable to create a valid context.");

   alcMakeContextCurrent(context);
   testForALCError(device);

   do {
      unsigned int no_samples, freq;
      char bps, channels;
      void *data;

      data = fileLoad((char *)&infile, &no_samples, &freq, &bps, &channels);
      testForError(data, "Input file not found.\n");

      if (data)
      {
         ALuint buffer, source;
         ALenum format;
         ALint q, status;
         float _time = 0.0;

         if      ((bps == 8) && (channels == 1)) format = AL_FORMAT_MONO8;
         else if ((bps == 8) && (channels == 2)) format = AL_FORMAT_STEREO8;
         else if ((bps == 16) && (channels == 1)) format = AL_FORMAT_MONO16;
         else if ((bps == 16) && (channels == 2)) format = AL_FORMAT_STEREO16;

         alGetError();
         alGenBuffers(1, &buffer);
         alBufferData(buffer, format, data, no_samples*bps>>3, freq);
         free(data);
         testForALError();

         alGenSources(1, &source);
         testForALError();
         alSourcei(source, AL_BUFFER, buffer);
         alSourcei(source, AL_LOOPING, AL_TRUE);
         alSourcePlay(source);

         q = 0;
         do {
            nanoSleep(5e5);
            _time += 0.5;

            q++;
            if (q > 10)
            {
               ALint offs, offb;
               float off_s;
               q = 0;

               alGetSourcef(source, AL_SEC_OFFSET, &off_s);
               alGetSourcei(source, AL_SAMPLE_OFFSET, &offs);
               alGetSourcei(source, AL_BYTE_OFFSET, &offb);

               printf("buffer position: %5.2f seconds (%i samples, %i bytes)\n",
                      off_s, offs, offb);
            }
            alGetSourcei(source, AL_SOURCE_STATE, &status);
         } while (_time < 90);

         alDeleteSources(1, &source);
         alDeleteBuffers(1, &buffer);

      }

   } while(0);

   context = alcGetCurrentContext();
   device = alcGetContextsDevice(context);
   testForALCError(device);

   alcMakeContextCurrent(NULL);
   alcDestroyContext(context);
   alcCloseDevice(device);

   return 0;
}
