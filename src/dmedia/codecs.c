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

#include "audio.h"

#include <api.h>
#include <devices.h>
#include <ringbuffer.h>

static _oalCodec _sgi_bufcpy_8u;
static _oalCodec _sgi_bufcpy_16s;
static _oalCodec _sgi_bufcpy_24s;
static _oalCodec _sgi_bufcpy_32s;
static _oalCodec _sgi_bufcpy_float;
static _oalCodec _sgi_bufcpy_double;
static _oalCodec _sgi_bufcpy_MuLaw;
static _oalCodec _sgi_bufcpy_ALaw;
static _oalCodec _sgi_bufcpy_ImaADPCM;

const _oalCodec *_oalDMediaCodecs[FORMAT_MAX] =
{
   _sgi_bufcpy_8u,
   _sgi_bufcpy_16s,
   _sgi_bufcpy_24s,
   _sgi_bufcpy_32s,
   _sgi_bufcpy_float,
   _sgi_bufcpy_double,
   _sgi_bufcpy_MuLaw,
   _sgi_bufcpy_ALaw,
   _sgi_bufcpy_ImaADPCM
};


void _sgi_bufcpy_8u(void *dst, void *src, unsigned char bps, unsigned int l)
{
#if 0
   ALint *buf = (ALint *)data;
   register ALuint i, l = *dlen;
   for (i=0; i<l; i++)
      buf[i] -= 128;
   return data;
#endif
}

void _sgi_bufcpy_16s(void *dst, void *src, unsigned char bps, unsigned int l)
{
}

void _sgi_bufcpy_24s(void *dst, void *src, unsigned char bps, unsigned int l)
{
}

void _sgi_bufcpy_32s(void *dst, void *src, unsigned char bps, unsigned int l)
{
}

void _sgi_bufcpy_float(void *dst, void *src, unsigned char bps, unsigned int l)
{
}

void _sgi_bufcpy_double(void *dst, void *src, unsigned char bps, unsigned int l)
{
}


void _sgi_bufcpy_MuLaw(void *dst, void *src, unsigned char bps, unsigned int l)
{
#if 0
   unsigned int i;
   unsigned char **ulaw = calloc(channels, sizeof(void*));
   if (ulaw)
   {
      for (i=0; i<channels; i++)
         pdmG711MulawDecode(ulaw[i], (short *)track[i], *dlen);
      *dlen *= 2;
   }
   return (const void **)ulaw;
#endif
}

void _sgi_bufcpy_ALaw(void *dst, void *src, unsigned char bps, unsigned int l)
{
#if 0
   unsigned int i;
   unsigned char **alaw = calloc(channels, sizeof(void*));
   if (alaw)
   {
      for (i=0; i<channels; i++)
         pdmG711AlawDecode(alaw[i], (short *)track[i], *dlen);
      *dlen *= 2;
   }
   return (const void **)alaw;
#endif
}

void _sgi_bufcpy_ImaADPCM(void *dst, void *src, unsigned char bps, unsigned int l)
{
#if 0
   unsigned int i;
   void *decoder;
   short *iadpcm;
   int error;
   if (pdmDVIAudioDecoderCreate(&decoder) == DM_FAILURE) {
      char detail[1024];
      pdmGetError(&error, detail);
      _oalStateSetError(error);
   } else {
      for (i=0; i < channels; i++)
      {
         if ((iadpcm = malloc(*dlen * 4)) != 0) {
            pdmDVIAudioDecode(decoder, (unsigned char *)track[i], iadpcm, *dlen);
            *dlen *= 4;
            free((void *)track[i]);
            track[i] = (void *)iadpcm;
         }
         else
         { 
            while (i-- != 0)
               free((void *)track[i]);
            _oalStateSetError(AL_OUT_OF_MEMORY);
         }
      }
   }
   return track;
#endif
}

