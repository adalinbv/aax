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

#include <base/types.h>
#include <base/logging.h>

#include "../test/wavfile.h"

void testForError(void *p, char *s)
{
   if (p == NULL)
   {
     printf("\n%s\n", s);
     exit(-1);
   }
}

#define _2PI	2*3.14159265358979323846

void
writeConstantOffset(const char *fname, int val, double sample_freq, int tracks,int bps, double duration)
{
#if 0
   unsigned int n, size;
   void *data;

   bps >>= 3;

   n = duration * sample_freq;
   size = n * bps;
   data = calloc(tracks, size);
   testForError(data, "Unable to allocate enough memroy.\n");

   memset(data, val, tracks*size);

   fileWrite(fname, data, n, sample_freq, bps, tracks);

   free(data);
#endif
}

void
writeSineWave(const char *fname, double freq, double sample_freq, int tracks, double duration)
{
   unsigned int no_samples;
   enum aaxFormat format;
   aaxBuffer buffer;

   format = AAX_PCM16S;
   no_samples = duration*sample_freq;

   buffer = aaxBufferCreate(NULL, no_samples, tracks, format);
   aaxBufferSetSetup(buffer, AAX_FREQUENCY, sample_freq);

   aaxBufferProcessWaveform(buffer, freq, AAX_SINE_WAVE, 1.0, AAX_OVERWRITE);
   aaxBufferWriteToFile(buffer, fname, AAX_OVERWRITE);

#if 0
   static const double mul = (double)(int)0x7FFF;
   unsigned int i, no_samples, size, bps = 2;
   double n, dt, f, s;
   int16_t *data, *ptr;

   n = duration * sample_freq;
   f = ceil((sample_freq / freq) * duration);
   duration = f * freq / sample_freq;
   n = duration * sample_freq;

   no_samples = n;
   size = no_samples * bps;
   data = calloc(tracks, size);
   testForError(data, "Unable to allocate enough memroy.\n");

   /* Generate waveform */
   s = 0;
   dt = _2PI/f;
   ptr = data;
   i = no_samples;
   do
   {
      int t = tracks;
      int16_t samp;

      if (s >= _2PI) s -= _2PI;
      samp = ceil(sin(s)*mul);
      do {
         *ptr++ = samp;
      } while (--t);

      s += dt;
   }
   while (--i);

   fileWrite(fname, data, n, sample_freq, bps, tracks);

   free(data);
#endif
}

void
writeSawtoothWave(const char *fname, double freq, double sample_freq, int tracks, double duration)
{
   unsigned int no_samples;
   enum aaxFormat format;
   aaxBuffer buffer;

   format = AAX_PCM16S;
   no_samples = duration*sample_freq;

   buffer = aaxBufferCreate(NULL, no_samples, tracks, format);
   aaxBufferSetSetup(buffer, AAX_FREQUENCY, sample_freq);

   aaxBufferProcessWaveform(buffer, freq,AAX_SAWTOOTH_WAVE, 1.0, AAX_OVERWRITE);
   aaxBufferWriteToFile(buffer, fname, AAX_OVERWRITE);

#if 0
   static const int no_harmoncs = 16;
   static double mul = (double)((int)0x7FFF / 2);
   unsigned int i, j, no_samples, size, bps = 2;
   double n, dt, f, s;
   int16_t *data, *ptr;

   n = duration * sample_freq;
   f = ceil((sample_freq / freq) * duration);
   duration = f * freq / sample_freq;
   n = duration * sample_freq;

   no_samples = n;
   size = no_samples * bps;
   data = calloc(tracks, size);
   testForError(data, "Unable to allocate enough memroy.\n");

   /* Generate waveform */
   dt = _2PI/f;
   j = no_harmoncs;
   f = sample_freq / freq;
   do
   {
      double ndt = dt*j;
      double nmul = mul/j;

      s = 0;
      ptr = data;
      i = no_samples;
      do
      {
         int t = tracks;
         int16_t samp;

         /* if (s >= _2PI) s -= _2PI; */
         samp = ceil(sin(s)*nmul);
         do {
            *ptr++ += samp;
         } while (--t);

         s += ndt;
      }
      while (--i);
   }
   while (--j);


   fileWrite(fname, data, n, sample_freq, bps, tracks);

   free(data);
#endif
}

void
writeSquareWave(const char *fname, double freq, double sample_freq, int tracks,
double duration)
{
   unsigned int no_samples;
   enum aaxFormat format;
   aaxBuffer buffer;

   format = AAX_PCM16S;
   no_samples = duration*sample_freq;

   buffer = aaxBufferCreate(NULL, no_samples, tracks, format);
   aaxBufferSetSetup(buffer, AAX_FREQUENCY, sample_freq);

   aaxBufferProcessWaveform(buffer, freq,AAX_SAWTOOTH_WAVE, 1.0, AAX_OVERWRITE);
   aaxBufferWriteToFile(buffer, fname, AAX_OVERWRITE);

#if 0
   static double mul = (double)((int)0x7FFF) * 0.75;
   static const int no_harmoncs = 4;
   unsigned int i, j, no_samples;
   unsigned int size, bps = 2;
   double n, dt, f, s;
   int16_t *data, *ptr;

   n = duration * sample_freq;
   f = ceil((sample_freq / freq) * duration);
   duration = f * freq / sample_freq;
   n = duration * sample_freq;

   no_samples = n;
   size = no_samples * bps;
   data = calloc(tracks, size);
   testForError(data, "Unable to allocate enough memroy.\n");

   /* Generate waveform */
   j = no_harmoncs;
   do
   {
      double nfreq = 2*freq + freq*(j-1);
      double nmul = mul/(2*j-1);

      if (nfreq > sample_freq) continue;

      if (j == 1) {
         nfreq = freq;
      }

      f = sample_freq / nfreq;
      dt = _2PI/f;

      s = 0;
      ptr = data;
      i = no_samples;
      do
      {
         int t = tracks;
         int16_t samp;

         /* if (s >= _2PI) s -= _2PI; */
         samp = ceil(sin(s)*nmul);
         do {
            *ptr++ += samp;
         } while (--t);

         s += dt;
      }
      while (--i);
   }
   while (--j);


   fileWrite(fname, data, n, sample_freq, bps, tracks);

   free(data);
#endif
}

int main()
{
   writeConstantOffset(SRC_PATH"constant-0x33.wav", 0x33, 16000.0, 1, 8, 1.333);
   writeSineWave(SRC_PATH"sine-1kHz.wav", 1000.0, 44100.0, 1, 1.0);
   writeSineWave(SRC_PATH"sine-440Hz.wav", 440.0, 22050.0, 1, 1.0);
//   writeSineWave(SRC_PATH"sine-440Hz-1period.wav", 440.0, 37000.0, 1, 2*440.0/37000);
   writeSquareWave(SRC_PATH"square-3kHz.wav", 300, 32000, 1, 1.0);
   writeSawtoothWave(SRC_PATH"sawtooth-75Hz.wav", 75, 16000, 1, 1.0);
}
