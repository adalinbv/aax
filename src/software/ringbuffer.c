/*
 * Copyright 2005-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <api.h>

#include <base/logging.h>

#include "ringbuffer.h"
#include "arch.h"


static void _aaxRingBufferIMA4ToPCM16(int32_t **__restrict,const void *__restrict,int,int,unsigned int);


void
_aaxRingBufferFillNonInterleaved(_aaxRingBuffer *rb, const void *data, unsigned blocksize, char looping)
{
   unsigned int t, fmt, no_tracks, tracksize;
   int32_t **tracks;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);

   rb->set_state(rb, RB_CLEARED);
   rb->set_parami(rb, RB_LOOPING, looping);

   fmt = rb->get_parami(rb, RB_FORMAT);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   tracksize = rb->get_parami(rb, RB_NO_SAMPLES) *
               rb->get_parami(rb, RB_BYTES_SAMPLE);

   tracks = (int32_t**)rb->get_dataptr_noninterleaved(rb->id);
   switch (fmt)
   {
   case AAX_IMA4_ADPCM:
   {
#if 1
      printf("WARNING: AAX_IMA4_ADPCM not implemented for _aaxRingBufferFillNonInterleaved\n");
      exit(-1);
#else
      int t;
      for (t=0; t<no_tracks; t++)
      {
         int32_t **track = tracks[t];
         _aaxRingBufferIMA4ToPCM16(tracks, data, 1, blocksize, no_samples);
      }
      rb->format = AAX_PCM16S;
#endif
      break;
   }
   default:
      for (t=0; t<no_tracks; t++)
      {
         char *s, *d;

         s = (char *)data + t*tracksize;
         d = (char *)tracks[t];
         _aax_memcpy(d, s, tracksize);
      }
   }
   rb->release_dataptr_noninterleaved(rb->id);
}

void
_aaxRingBufferFillInterleaved(_aaxRingBuffer *rb, const void *data, unsigned blocksize, char looping)
{
   unsigned int fmt, bps, no_samples, no_tracks, tracksize;
   int32_t **tracks;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(data != 0);

   rb->set_state(rb, RB_CLEARED);
   rb->set_parami(rb, RB_LOOPING, looping);

   fmt = rb->get_parami(rb, RB_FORMAT);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   tracksize = no_samples * bps;

   tracks = (int32_t**)rb->get_dataptr_noninterleaved(rb->id);
   switch (fmt)
   {
   case AAX_IMA4_ADPCM:
      _aaxRingBufferIMA4ToPCM16(tracks, data, no_tracks, blocksize, no_samples);
      break;
   case AAX_PCM32S:
      _batch_cvt24_32_intl(tracks, data, 0, no_tracks, no_samples);
      
      break;
   case AAX_FLOAT:
      _batch_cvt24_ps_intl(tracks, data, 0, no_tracks, no_samples);
      break;
   case AAX_DOUBLE:
      _batch_cvt24_pd_intl(tracks, data, 0, no_tracks, no_samples);
      break;
   default:
      if (no_tracks == 1) {
         _aax_memcpy(tracks[0], data, tracksize);
      }
      else /* stereo */
      {
         unsigned int frame_size = no_tracks * bps;
         unsigned int t;

         for (t=0; t<no_tracks; t++)
         {
            char *sptr, *dptr;
            unsigned int i;

            sptr = (char *)data + t*bps;
            dptr = (char *)tracks[t];
            i = no_samples;
            do
            {
               memcpy(dptr, sptr, bps);
               sptr += frame_size;
               dptr += bps;
            }
            while (--i);
         }
      }
   } /* switch */
   rb->release_dataptr_noninterleaved(rb->id);
}


void
_aaxRingBufferGetDataInterleaved(_aaxRingBuffer *rb, void* data, unsigned int samples, int channels, float fact)
{
   unsigned int fmt, bps, no_samples, t, no_tracks;
   void **ptr, **tracks;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb != 0);
   assert(data != 0);

   fmt = rb->get_parami(rb, RB_FORMAT);
   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   if (no_tracks > channels) no_tracks = channels;

   assert(samples >= (unsigned int)(fact*no_samples));

   tracks = (void**)rb->get_dataptr_noninterleaved(rb->id);

   fact = 1.0f/fact;
   ptr = (void**)tracks;
   if (fact != 1.0f)
   {
      unsigned int size = samples*bps;
      char *p;
      
      if (bps == sizeof(int32_t))
      {
         p = (char*)(no_tracks*sizeof(void*));
         tracks = (void**)_aax_malloc(&p, no_tracks*(sizeof(void*) + size));
         for (t=0; t<no_tracks; t++)
         {
            tracks[t] = p;
            _aaxProcessResample(tracks[t], ptr[t], 0, samples, 0, fact);
            p += size;
         }
      }
      else
      {
         unsigned int scratch_size;
         int32_t **scratch;
         char *sptr;

         scratch_size = 2*sizeof(int32_t*);
         sptr = (char*)scratch_size;
        
         scratch_size += (no_samples+samples)*sizeof(int32_t);
         scratch = (int32_t**)_aax_malloc(&sptr, scratch_size);
         scratch[0] = (int32_t*)sptr;
         scratch[1] = (int32_t*)(sptr + no_samples*sizeof(int32_t));

         p = (char*)(no_tracks*sizeof(void*));
         tracks = (void**)_aax_malloc(&p, no_tracks*(sizeof(void*) + size));
         for (t=0; t<no_tracks; t++)
         {
            tracks[t] = p;
            bufConvertDataToPCM24S(scratch[0], ptr[t], no_samples, fmt);
            _aaxProcessResample(scratch[1], scratch[0], 0, samples, 0, fact);
            bufConvertDataFromPCM24S(tracks[t], scratch[1], 1, samples, fmt, 1);
            p += size;
         }
         free(scratch);
      }
   }

   if (no_tracks == 1) {
      _aax_memcpy(data, tracks[0], samples*bps);
   }
   else
   {
      for (t=0; t<no_tracks; t++)
      {
         uint8_t *s = (uint8_t*)tracks[t];
         uint8_t *d = (uint8_t *)data + t*bps;
         unsigned int i =  samples;
         do
         {
            memcpy(d, s, bps);
            d += no_tracks*bps;
            s += bps;
         }
         while (--i);
      }
   }
   rb->release_dataptr_noninterleaved(rb->id);

   if (ptr != tracks) free(tracks);
}

/* -------------------------------------------------------------------------- */

/*
 * Convert 4-bit IMA to 16-bit PCM
 */

static void
_aaxRingBufferIMA4ToPCM16(int32_t **__restrict dst, const void *__restrict src, int tracks, int blocksize, unsigned int no_samples)
{
   unsigned int i, blocks, block_smp;
   int16_t *d[_AAX_MAX_SPEAKERS];
   uint8_t *s = (uint8_t *)src;
   int t;

   if (tracks > _AAX_MAX_SPEAKERS)
      return;

   /* copy buffer pointers */
   for(t=0; t<tracks; t++) {
      d[t] = (int16_t*)dst[t];
   }

   block_smp = BLOCKSIZE_TO_SMP(blocksize);
   blocks = no_samples / block_smp;
   i = blocks-1;
   do
   {
      for (t=0; t<tracks; t++)
      {
         _sw_bufcpy_ima_adpcm(d[t], s, 1, block_smp);
         d[t] += block_smp;
         s += blocksize;
      }
   }
   while (--i);

   no_samples -= blocks*block_smp;
   if (no_samples)
   {
      int t;
      for (t=0; t<tracks; t++)
      {
         _sw_bufcpy_ima_adpcm(d[t], s, 1, no_samples);
         s += blocksize;
      }
   }
}

