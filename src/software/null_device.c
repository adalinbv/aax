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

#include <math.h>		/* for floor, and rint */
#include <errno.h>		/* for ETIMEDOUT */
#include <sys/time.h>		/* for struct time */

#include <aax.h>
#include <base/threads.h>
#include <base/types.h>

#include <api.h>

#include "arch.h"
#include "audio.h"

#define NONE_RENDERER		"None"
#define DEFAULT_RENDERER	AAX_NAME_STR""
#define LOOPBACK_RENDERER	AAX_NAME_STR" Loopback"
#define DEFAULT_OUTPUT_RATE	44100

static _aaxDriverDetect _aaxNoneDriverDetect;
static _aaxDriverGetDevices _aaxNoneDriverGetDevices;
static _aaxDriverGetInterfaces _aaxNoneDriverGetInterfaces;
static _aaxDriverConnect _aaxNoneDriverConnect;
static _aaxDriverDisconnect _aaxNoneDriverDisconnect;
static _aaxDriverSetup _aaxNoneDriverSetup;
static _aaxDriverState _aaxNoneDriverAvailable;
static _aaxDriverState _aaxNoneDriverNotAvailable;
static _aaxDriverCallback _aaxNoneDriverPlayback;
static _aaxDriver2dMixerCB _aaxNoneDriverStereoMixer;
static _aaxDriverCallback _aaxNoneDriverPlayback;
static _aaxDriver2dMixerCB _aaxNoneDriverStereoMixer;
static _aaxDriver3dMixerCB _aaxNoneDriver3dMixer;
static _aaxDriverGetName _aaxNoneDriverGetName;
static _aaxDriverPrepare3d _aaxNoneDriver3dPrepare;
static _aaxDriverPrepare _aaxNoneDriverPrepare;
static _aaxDriverPostProcess _aaxNoneDriverPostProcess;
static _aaxDriverThread _aaxNoneDriverThread;

const _aaxDriverBackend _aaxNoneDriverBackend =
{
   0.8f,
   AAX_PCM8S,
   0,
   0,

   AAX_VERSION_STR,
   NONE_RENDERER,
   AAX_VENDOR_STR,
   NONE_RENDERER,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxNoneDriverDetect,
   (_aaxDriverGetDevices *)&_aaxNoneDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxNoneDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxNoneDriverGetName,
   (_aaxDriverThread *)&_aaxNoneDriverThread,

   (_aaxDriverConnect *)&_aaxNoneDriverConnect,
   (_aaxDriverDisconnect *)&_aaxNoneDriverDisconnect,
   (_aaxDriverSetup *)&_aaxNoneDriverSetup,
   (_aaxDriverState *)&_aaxNoneDriverAvailable,		/* pause  */
   (_aaxDriverState *)&_aaxNoneDriverAvailable,		/* resume */
   NULL,
   (_aaxDriverCallback *)&_aaxNoneDriverPlayback,
   (_aaxDriver2dMixerCB *)&_aaxNoneDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxNoneDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxNoneDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxNoneDriverPostProcess,
   (_aaxDriverPrepare *)&_aaxNoneDriverPrepare,		/* effects */

   (_aaxDriverState *)_aaxNoneDriverAvailable,	/* supports playback */
   (_aaxDriverState *)_aaxNoneDriverNotAvailable, /* supports capture  */
   (_aaxDriverState *)_aaxNoneDriverAvailable	/* is available      */
};


static _aaxDriverCaptureCallback _aaxLoopbackDriverCapture;

char _null_default_renderer[100] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxLoopbackDriverBackend =
{
   1.0,
   AAX_PCM16S,
   DEFAULT_OUTPUT_RATE,
   2,

   AAX_VERSION_STR,
   LOOPBACK_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_null_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxNoneDriverDetect,
   (_aaxDriverGetDevices *)&_aaxNoneDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxNoneDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxNoneDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxNoneDriverConnect,
   (_aaxDriverDisconnect *)&_aaxNoneDriverDisconnect,
   (_aaxDriverSetup *)&_aaxNoneDriverSetup,
   (_aaxDriverState *)&_aaxNoneDriverAvailable,
   (_aaxDriverState *)&_aaxNoneDriverAvailable,
   (_aaxDriverCaptureCallback *)&_aaxLoopbackDriverCapture,
   (_aaxDriverCallback *)&_aaxNoneDriverPlayback,
   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxSoftwareDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)_aaxNoneDriverAvailable,
   (_aaxDriverState *)_aaxNoneDriverNotAvailable,
   (_aaxDriverState *)_aaxNoneDriverAvailable
};

static int
_aaxNoneDriverDetect(int mode)
{
   return AAX_TRUE;
}

static void *
_aaxNoneDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   const char *hwstr = _aaxGetSIMDSupportString();

   if (mode == AAX_MODE_READ) return NULL;

   snprintf(_null_default_renderer, 99, "%s %s", DEFAULT_RENDERER, hwstr);

   return (void *)&_aaxNoneDriverBackend;
}

static int
_aaxNoneDriverDisconnect(void *id)
{
   return AAX_TRUE;
}

static int
_aaxNoneDriverSetup(const void *id, size_t *bufsize, int fmt, unsigned int *tracks, float *speed)
{
   return AAX_TRUE;
}

static int
_aaxNoneDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxNoneDriverNotAvailable(const void *id)
{
   return AAX_FALSE;
}

static int
_aaxNoneDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
{
   return 0;
}

static int
_aaxNoneDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr)
{
   return AAX_FALSE;
}

static void
_aaxNoneDriver3dPrepare(void *sp3d, void *f3d, const void *info, const void *p2d, void* src)
{
}

static void
_aaxNoneDriverPrepare(const void *id, void *s, const void *l)
{
}

static void
_aaxNoneDriverPostProcess(const void *id, void *s, const void *l)
{
}

static int
_aaxNoneDriverStereoMixer(const void *id, void *d, void *s, void *p, void *m, float pitch, float volume, unsigned char ctr)
{
   return AAX_FALSE;
}

static char *
_aaxNoneDriverGetName(const void *id, int playback)
{
   return "None Driver";
}

static char *
_aaxNoneDriverGetDevices(const void *id, int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "\0\0"
   };
   return (char *)rd[mode];

}

static char *
_aaxNoneDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "\0\0"
   };
   return (char *)rd[mode];

}

static int
_aaxLoopbackDriverCapture(const void *id, void **data, size_t *size, void *scratch)
{
#if 0
   _driver_t *handle = (_driver_t *)id;
   size_t buflen;

   if ((handle->mode != 0) || (size == 0) || (data == 0))
      return AAX_FALSE;

   buflen = *size;
   if (buflen == 0)
      return AAX_TRUE;

   *size = 0;
   if (data)
   {
      int res;

      res = read(handle->fd, data, buflen);
      if (res == -1) return AAX_FALSE;
      *size = res;

      return AAX_TRUE;
   }
#endif
   return AAX_FALSE;
}

void
_aaxNoneDriverProcessFrame(void* config)
{
   _aaxAudioFrame* frame = (_aaxAudioFrame*)config;
   _intBuffers *he;
   float dt, d_pos;
   int stage;

   dt = 1.0f/frame->info->refresh_rate;

   stage = 0;
   he = frame->emitters_3d;
   do
   {
      unsigned int i, num;

      num = _intBufGetMaxNum(he, _AAX_EMITTER);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr_src;
         _emitter_t *emitter;
         _aaxEmitter *src;

         dptr_src = _intBufGet(he, _AAX_EMITTER, i);
         if (!dptr_src) continue;

         d_pos = 0.0f;

         emitter = _intBufGetDataPtr(dptr_src);
         src = emitter->source;
         if (_IS_PLAYING(src))
         {
            _intBufferData *dptr_sbuf;
            unsigned int nbuf, rv = 0;
            int streaming;

            nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);

            streaming = (nbuf > 1);
            dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER,
                                               src->pos);
            if (dptr_sbuf)
            {
               _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
               _oalRingBuffer *src_rb = embuf->ringbuffer;

               do
               {
                  float s_offs, s_duration;
                  float d_offs = dt;

                  if (_oalRingBufferTestPlaying(src_rb) == 0)
                  {
                     if (streaming) {
                        _oalRingBufferStartStreaming(src_rb);
                     } else {
                        _oalRingBufferStart(src_rb);
                     }
                  }

                  s_duration = _oalRingBufferGetDuration(src_rb);
                  s_offs = _oalRingBufferGetOffsetSec(src_rb);
                  if ((s_offs+dt) > s_duration)
                  {
                     if (!_oalRingBufferGetLooping(src_rb))
                     {
                        d_offs = s_duration - s_offs;
                        s_offs = s_duration;
                        rv = AAX_TRUE;
                     }
                     else {
                        s_offs = fmodf(s_offs+dt, s_duration);
                     }
                  } else {
                     s_offs += dt;
                  }
                  _oalRingBufferSetOffsetSec(src_rb, s_offs);
                  d_pos += d_offs;

                  src->curr_pos_sec += dt;

                  /*
                   * The current buffer of the source has finished playing.
                   * Decide what to do next.
                   */
                  if (rv)
                  {
                     _oalRingBufferStop(src_rb);
                     if (streaming)
                     {
                        /* is there another buffer ready to play? */
                        if (++src->pos == nbuf)
                        {
                           /*
                            * The last buffer was processed, return to the
                            * first buffer or stop? 
                            */
                           if TEST_FOR_TRUE(emitter->looping) {
                              src->pos = 0;
                           }
                           else
                           {
                              _SET_STOPPED(src);
                              _SET_PROCESSED(src);
                              break;
                           }
                        }

                        rv &= (d_pos < dt);
                        if (rv)
                        {
                           _intBufReleaseData(dptr_sbuf,_AAX_EMITTER_BUFFER);
                           dptr_sbuf = _intBufGet(src->buffers,
                                              _AAX_EMITTER_BUFFER, src->pos);
                           embuf = _intBufGetDataPtr(dptr_sbuf);
                           src_rb = embuf->ringbuffer;
                        }
                     }
                     else
                     {
                        _SET_PROCESSED(src);
                        break;
                     }
                  }
               }
               while (rv);
               _intBufReleaseData(dptr_sbuf, _AAX_EMITTER_BUFFER);
            }
            _intBufReleaseNum(src->buffers, _AAX_EMITTER_BUFFER);
         }
         _intBufReleaseData(dptr_src, _AAX_EMITTER);
      }
      _intBufReleaseNum(he, _AAX_EMITTER);

      if (stage == 0) {	/* 3d stage */
         he = frame->emitters_2d;
      }
   }
   while (++stage < 2); /* positional and stereo */

   _PROP_MTX_CLEAR_CHANGED(frame->props3d);
   _PROP_PITCH_CLEAR_CHANGED(frame->props3d);
}


void *
_aaxNoneDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame* mixer;
   _sensor_t* sensor;
   struct timespec ts;
   float delay_sec;
   float elapsed;
   float dt = 0.0f;


   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   dest_rb = _oalRingBufferCreate(AAX_TRUE);
   if (!dest_rb) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->refresh_rate;

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      sensor = _intBufGetDataPtr(dptr_sensor);
      mixer = sensor->mixer;
      mixer->ringbuffer = dest_rb;
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
   }
   else
   {
      _oalRingBufferDelete(dest_rb);
      return NULL;
   }

   elapsed = 0.0f;
   _aaxMutexLock(handle->thread.mutex);
   do
   {
      float delay = delay_sec;

      elapsed -= delay;
      if (elapsed <= 0.0f)
      {
         struct timeval now;
         float fdt;

         elapsed += 60.0f;               /* resync the time every 60 seconds */

         dt = delay;
         fdt = floorf(dt);

         gettimeofday(&now, 0);
         ts.tv_sec = (time_t)(now.tv_sec + fdt);

         dt -= fdt;
         dt += now.tv_usec*1e-6f;
         ts.tv_nsec = (long)(dt*1e9f);
         if (ts.tv_nsec >= 1e9f)
         {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
         }
      }
      else
      {
         dt += delay;
         if (dt >= 1.0f)
         {
            float fdt = floorf(dt);
            ts.tv_sec += (time_t)fdt;
            dt -= fdt;
         }
         ts.tv_nsec = (long)(dt*1e9f);
      }

      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if ((mixer->emitters_3d || mixer->emitters_2d) && _IS_PLAYING(handle))
      {
          dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
          if (dptr_sensor)
          {
            _aaxNoneDriverProcessFrame(mixer);
            _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
         }
      }
   }
   while (_aaxConditionWaitTimed(handle->thread.condition, handle->thread.mutex, &ts) == ETIMEDOUT);

   _aaxMutexUnLock(handle->thread.mutex);
   _oalRingBufferDelete(dest_rb);

   return handle;
}

