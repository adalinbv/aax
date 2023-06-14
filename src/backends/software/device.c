/*
 * Copyright 2005-2023 by Erik Hofman.
 * Copyright 2009-2023 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>		/* for floor, and rint */
#include <errno.h>		/* for ETIMEDOUT */
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>		/* for struct time */
#endif
#include <assert.h>

#include <aax/aax.h>

#include <base/threads.h>
#include <base/types.h>
#include <dsp/effects.h>

#include <api.h>

#include <software/renderer.h>
#include <software/audio.h>

#define NONE_RENDERER		"None"
#define DEFAULT_RENDERER	AAX_NAME_STR""
#define LOOPBACK_RENDERER	AAX_NAME_STR" Loopback"
#define DEFAULT_OUTPUT_RATE	44100

static _aaxDriverDetect _aaxNoneDriverDetect;
static _aaxDriverNewHandle _aaxNoneDriverNewHandle;
static _aaxDriverFreeHandle _aaxNoneDriverFreeHandle;
static _aaxDriverGetDevices _aaxNoneDriverGetDevices;
static _aaxDriverGetInterfaces _aaxNoneDriverGetInterfaces;
static _aaxDriverConnect _aaxNoneDriverConnect;
static _aaxDriverDisconnect _aaxNoneDriverDisconnect;
static _aaxDriverSetup _aaxNoneDriverSetup;
static _aaxDriverPlaybackCallback _aaxNoneDriverPlayback;
static _aaxDriverSetName _aaxNoneDriverSetName;
static _aaxDriverGetName _aaxNoneDriverGetName;
static _aaxDriverRender _aaxNoneDriverRender;
static _aaxDriverPrepare3d _aaxNoneDriver3dPrepare;
static _aaxDriverPrepare _aaxNoneDriverPrepare;
static _aaxDriverPostProcess _aaxNoneDriverPostProcess;
static _aaxDriverThread _aaxNoneDriverThread;
static _aaxDriverGetSetSources _aaxNoneDriverGetSetSources;
static _aaxDriverState _aaxNoneDriverState;
static _aaxDriverParam _aaxNoneDriverParam;
static _aaxDriverLog _aaxNoneDriverLog;

const _aaxDriverBackend _aaxNoneDriverBackend =
{
   AAX_VERSION_STR,
   NONE_RENDERER,
   AAX_VENDOR_STR,
   NONE_RENDERER,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxNoneDriverDetect,
   (_aaxDriverNewHandle *)&_aaxNoneDriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxNoneDriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxNoneDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxNoneDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxNoneDriverSetName,
   (_aaxDriverGetName *)&_aaxNoneDriverGetName,
   (_aaxDriverRender *)&_aaxNoneDriverRender,
   (_aaxDriverThread *)&_aaxNoneDriverThread,

   (_aaxDriverConnect *)&_aaxNoneDriverConnect,
   (_aaxDriverDisconnect *)&_aaxNoneDriverDisconnect,
   (_aaxDriverSetup *)&_aaxNoneDriverSetup,
   NULL,
   (_aaxDriverPlaybackCallback *)&_aaxNoneDriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxNoneDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxNoneDriverPostProcess,
   (_aaxDriverPrepare *)&_aaxNoneDriverPrepare,		/* effects */
   NULL,

   (_aaxDriverGetSetSources*)_aaxNoneDriverGetSetSources,

   (_aaxDriverState *)_aaxNoneDriverState,
   (_aaxDriverParam *)&_aaxNoneDriverParam,
   (_aaxDriverLog *)&_aaxNoneDriverLog
};


static _aaxDriverNewHandle _aaxLoopbackDriverNewHandle;
static _aaxDriverConnect _aaxLoopbackDriverConnect;
static _aaxDriverDisconnect _aaxLoopbackDriverDisconnect;

static _aaxDriverRender _aaxLoopbackDriverRender;
static _aaxDriverSetup _aaxLoopbackDriverSetup;
static _aaxDriverParam _aaxLoopbackDriverParam;
static _aaxDriverLog _aaxLoopbackDriverLog;

typedef struct
{
   void *handle;
   int mode;
   float latency;
   float frequency;
   float refresh_rate;
   size_t no_frames;
   enum aaxFormat format;
   uint8_t no_channels;
   uint8_t bits_sample;
   char sse_level;

   _aaxRenderer *render;

} _driver_t;

static char _loopback_default_renderer[100] = LOOPBACK_RENDERER;
const _aaxDriverBackend _aaxLoopbackDriverBackend =
{
   AAX_VERSION_STR,
   LOOPBACK_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_loopback_default_renderer,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxNoneDriverDetect,
   (_aaxDriverNewHandle *)&_aaxLoopbackDriverNewHandle,
   (_aaxDriverFreeHandle *)&_aaxNoneDriverFreeHandle,
   (_aaxDriverGetDevices *)&_aaxNoneDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxNoneDriverGetInterfaces,

   (_aaxDriverSetName *)&_aaxNoneDriverSetName,
   (_aaxDriverGetName *)&_aaxNoneDriverGetName,
   (_aaxDriverRender *)&_aaxLoopbackDriverRender,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxLoopbackDriverConnect,
   (_aaxDriverDisconnect *)&_aaxLoopbackDriverDisconnect,
   (_aaxDriverSetup *)&_aaxLoopbackDriverSetup,
   NULL,
   (_aaxDriverPlaybackCallback *)&_aaxNoneDriverPlayback,
   NULL,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,
   NULL,

   ( _aaxDriverGetSetSources*)_aaxSoftwareDriverGetSetSources,

   (_aaxDriverState *)_aaxNoneDriverState,
   (_aaxDriverParam *)&_aaxLoopbackDriverParam,
   (_aaxDriverLog *)&_aaxLoopbackDriverLog
};

static int
_aaxNoneDriverDetect(UNUSED(int mode))
{
   return AAX_TRUE;
}

static void *
_aaxNoneDriverNewHandle(UNUSED(enum aaxRenderMode mode))
{
   return NULL;
}

static int
_aaxNoneDriverFreeHandle(UNUSED(void *id))
{
   return AAX_TRUE;
}

static void *
_aaxNoneDriverConnect(UNUSED(void *config), UNUSED(const void *id), UNUSED(xmlId *xid), UNUSED(const char *renderer), enum aaxRenderMode mode)
{
   if (mode == AAX_MODE_READ) return NULL;
   return (void *)&_aaxNoneDriverBackend;
}

static int
_aaxNoneDriverDisconnect(UNUSED(void *id))
{
   return AAX_TRUE;
}

static int
_aaxNoneDriverSetup(UNUSED(const void *id), UNUSED(float *refresh_rate), UNUSED(int *fmt), UNUSED(unsigned int *tracks), UNUSED(float *speed), UNUSED(int *bitrate), UNUSED(int registered), UNUSED(float period_rate))
{
   return AAX_TRUE;
}

static size_t
_aaxNoneDriverPlayback(UNUSED(const void *id), UNUSED(void *s), UNUSED(float pitch), UNUSED(float volume), UNUSED(char batched))
{
   return 0;
}

static void
_aaxNoneDriver3dPrepare(UNUSED(void* src), UNUSED(const void *daat))
{
}

static void
_aaxNoneDriverPrepare(UNUSED(const void *id))
{
}

static void
_aaxNoneDriverPostProcess(UNUSED(const void *id))
{
}

static int
_aaxNoneDriverSetName(const void *id, int type, const char *name)
{
   return AAX_FALSE;
}

static char *
_aaxNoneDriverGetName(UNUSED(const void *id), UNUSED(int playback))
{
   return NULL;
}

_aaxRenderer*
_aaxNoneDriverRender(UNUSED(const void* config))
{
   return NULL;
}

static unsigned int
_aaxNoneDriverGetSetSources(UNUSED(unsigned int max), UNUSED(int num))
{
   return 0;
}

static int
_aaxNoneDriverState(UNUSED(const void *id), enum _aaxDriverState state)
{
   int rv = AAX_FALSE;
   switch(state)
   {
   case DRIVER_AVAILABLE:
   case DRIVER_PAUSE:
   case DRIVER_RESUME:
   case DRIVER_SUPPORTS_PLAYBACK:
      rv = AAX_TRUE;
      break;
   case DRIVER_SUPPORTS_CAPTURE:
   case DRIVER_SHARED_MIXER:
   case DRIVER_NEED_REINIT:
   default:
      break;
   }
   return rv;
}

static float
_aaxNoneDriverParam(const void *id, enum _aaxDriverParam param)
{
   _driver_t *handle = (_driver_t*)id;
   float rv = 0.0f;
   switch(param)
   {
		/* float */
   case DRIVER_LATENCY:
      rv = 1e-6f;
      break;
   case DRIVER_MAX_VOLUME:
      rv = 1.0f;
      break;
   case DRIVER_MIN_VOLUME:
      rv = 0.0f;
      break;
   case DRIVER_VOLUME:
      rv = 1.0f;
      break;
   case DRIVER_REFRESHRATE:
         rv = handle->refresh_rate;
         break;

		/* int */
   case DRIVER_MIN_FREQUENCY:
      rv = 8000.0f;
      break;
   case DRIVER_MAX_FREQUENCY:
      rv = _AAX_MAX_MIXER_FREQUENCY;
      break;
   case DRIVER_MIN_TRACKS:
      rv = 1.0f;
      break;
   case DRIVER_MAX_TRACKS:
      rv = (float)_AAX_MAX_SPEAKERS;
      break;
   case DRIVER_MIN_PERIODS:
   case DRIVER_MAX_PERIODS:
      rv = 1.0f;
      break;
   case DRIVER_MAX_SOURCES:
      rv = 0;
      break;
   case DRIVER_MAX_SAMPLES:
         rv = AAX_FPINFINITE;
         break;
   case DRIVER_SAMPLE_DELAY:
      rv = 0.0f;
      break;

		/* boolean */
   case DRIVER_TIMER_MODE:
   case DRIVER_BATCHED_MODE:
      rv = (float)AAX_TRUE;
      break;
   case DRIVER_SHARED_MODE:
   default:
      break;
   }
   return rv;
}

static char *
_aaxNoneDriverLog(UNUSED(const void *id), UNUSED(int prio), UNUSED(int type), UNUSED(const char *str))
{
   return NULL;
}

static char *
_aaxNoneDriverGetDevices(UNUSED(const void *id), int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "\0\0"
   };
   return (char *)rd[mode ? 0 : 1];

}

static char *
_aaxNoneDriverGetInterfaces(UNUSED(const void *id), UNUSED(const char *devname), int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "\0\0"
   };
   return (char *)rd[mode ? 0 : 1];

}

static void *
_aaxLoopbackDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *rv = calloc(1, sizeof(_driver_t));
   if (rv)
   {
      rv->latency = 0.001f;
      rv->mode = mode;
   }
   return rv;
}

static void *
_aaxLoopbackDriverConnect(void *config, const void *id, UNUSED(xmlId *xid), UNUSED(const char *renderer), enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   if (!handle) {
      handle = _aaxLoopbackDriverNewHandle(mode);
      if (handle) handle->handle = config;
   }
   return (void *)handle;
}


static int
_aaxLoopbackDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (handle && handle->render)
   {
      handle->render->close(handle->render->id);
      free(handle->render);
   }
   free(id);

   return AAX_TRUE;
}

static int
_aaxLoopbackDriverSetup(const void *id, float *refresh_rate, int *fmt, unsigned int *tracks, float *speed, int *bitrate, int registered, float period_rate)
{
   _driver_t *handle = (_driver_t *)id;
   if (handle)
   {
      assert(speed);
      assert(refresh_rate);
      assert(fmt);
      assert(tracks);
      assert(speed);
      assert(bitrate);

      handle->format = *fmt;
      handle->frequency = *speed;
      handle->no_channels = *tracks;
      handle->refresh_rate = *refresh_rate;
      handle->bits_sample = aaxGetBitsPerSample(*fmt);
      handle->no_frames = (size_t)rintf((float)*speed / *refresh_rate);
      handle->latency = 1.0f / period_rate;
      handle->render = _aaxSoftwareInitRenderer(handle->latency, handle->mode, registered);
      if (handle->render)
      {
         const char *rstr = handle->render->info(handle->render->id);
         snprintf(_loopback_default_renderer, 99, "%s %s", LOOPBACK_RENDERER, rstr);
      }
   }
   return AAX_TRUE;
}

void
_aaxSoftwareDriver3dPrepare(void* src, const void *data)
{
   assert(data);
   assert(src);

   _aaxEmitterPrepare3d(src, data);
}

static float
_aaxLoopbackDriverParam(const void *id, enum _aaxDriverParam param)
{
   _driver_t *handle = (_driver_t *)id;
   float rv = 0.0f;
   if (handle)
   {
      switch(param)
      {
		/* float */
      case DRIVER_LATENCY:
         rv = handle->latency;
         break;
      case DRIVER_MAX_VOLUME:
         rv = 1.0f;
         break;
      case DRIVER_MIN_VOLUME:
         rv = 0.0f;
         break;
      case DRIVER_VOLUME:
         rv = 1.0f;
         break;

		/* int */
      case DRIVER_MIN_FREQUENCY:
         rv = 8000.0f;
         break;
      case DRIVER_MAX_FREQUENCY:
         rv = _AAX_MAX_MIXER_FREQUENCY;
         break;
      case DRIVER_MIN_TRACKS:
         rv = 1.0f;
         break;
      case DRIVER_MAX_TRACKS:
         rv = (float)_AAX_MAX_SPEAKERS;
         break;
      case DRIVER_MIN_PERIODS:
      case DRIVER_MAX_PERIODS:
         rv = 1.0f;
         break;
      case DRIVER_MAX_SOURCES:
         rv = ((_handle_t*)(handle->handle))->backend.ptr->getset_sources(0, 0);
         break;
      case DRIVER_MAX_SAMPLES:
         rv = AAX_FPINFINITE;
         break;
      case DRIVER_SAMPLE_DELAY:
         rv = (float)handle->no_frames;
         break;

		/* boolean */
      case DRIVER_TIMER_MODE:
      case DRIVER_BATCHED_MODE:
          rv = (float)AAX_TRUE;
          break;
      case DRIVER_SHARED_MODE:
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxLoopbackDriverLog(const void *id, UNUSED(int prio), UNUSED(int type), const char *str)
{
   _driver_t *handle = (_driver_t *)id;
   static char _errstr[256] = "\0";

   if (str)
   {
      size_t len = _MIN(strlen(str)+1, 256);

      memcpy(_errstr, str, len);
      _errstr[255] = '\0';  /* always null terminated */

      __aaxDriverErrorSet(handle->handle, AAX_BACKEND_ERROR, (char*)&_errstr);
      _AAX_SYSLOG(_errstr);
   }

   return (char*)&_errstr;
}

_aaxRenderer*
_aaxLoopbackDriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

void
_aaxNoneDriverProcessFrame(void* config)
{
   _aaxAudioFrame* frame = (_aaxAudioFrame*)config;
   _intBuffers *he;
   float dt, d_pos;
   int stage;

   dt = 1.0f/frame->info->period_rate;

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
         if (_IS_PLAYING(src->props3d))
         {
            _intBufferData *dptr_sbuf;
            unsigned int nbuf, rv = 0;
            int streaming;

            nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);

            streaming = (nbuf > 1);
            dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER,
                                                 src->buffer_pos);
            if (dptr_sbuf)
            {
               _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
               _aaxRingBuffer *src_rb = embuf->ringbuffer;

               do
               {
                  float s_offs, s_duration;
                  float d_offs = dt;

                  if (src_rb->get_parami(src_rb, RB_IS_PLAYING) == 0)
                  {
                     if (streaming) {
                        src_rb->set_state(src_rb, RB_STARTED_STREAMING);
                     } else {
                        src_rb->set_state(src_rb, RB_STARTED);
                     }
                  }

                  s_duration = src_rb->get_paramf(src_rb, RB_DURATION_SEC);
                  s_offs = src_rb->get_paramf(src_rb, RB_OFFSET_SEC);
                  if ((s_offs+dt) > s_duration)
                  {
                     if (!src_rb->get_parami(src_rb, RB_LOOPING))
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
                  src_rb->set_paramf(src_rb, RB_OFFSET_SEC, s_offs);
                  d_pos += d_offs;

                  src->curr_pos_sec += dt;

                  /*
                   * The current buffer of the source has finished playing.
                   * Decide what to do next.
                   */
                  if (rv)
                  {
                     src_rb->set_state(src_rb, RB_STOPPED);
                     if (streaming)
                     {
                        /* is there another buffer ready to play? */
                        if (++src->buffer_pos == nbuf)
                        {
                           /*
                            * The last buffer was processed, return to the
                            * first buffer or stop? 
                            */
                           if TEST_FOR_TRUE(emitter->looping) {
                              src->buffer_pos = 0;
                           }
                           else
                           {
                              _SET_STOPPED(src->props3d);
                              _SET_PROCESSED(src->props3d);
                              break;
                           }
                        }

                        rv &= (d_pos < dt);
                        if (rv)
                        {
                           _intBufReleaseData(dptr_sbuf,_AAX_EMITTER_BUFFER);
                           dptr_sbuf = _intBufGet(src->buffers,
                                          _AAX_EMITTER_BUFFER, src->buffer_pos);
                           embuf = _intBufGetDataPtr(dptr_sbuf);
                           src_rb = embuf->ringbuffer;
                        }
                     }
                     else
                     {
                        _SET_PROCESSED(src->props3d);
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
}


void *
_aaxNoneDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   const _aaxDriverBackend *be;
   _intBufferData *dptr_sensor;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame* smixer;
   _sensor_t* sensor;
   float delay_sec;
   ssize_t nsamps;
   char batched;
   int res;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   be = handle->backend.ptr;
   dest_rb = be->get_ringbuffer(MAX_EFFECTS_TIME, handle->info->mode);
   if (!dest_rb) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->period_rate;
   batched = handle->finished ? AAX_TRUE : AAX_FALSE;

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      sensor = _intBufGetDataPtr(dptr_sensor);
      smixer = sensor->mixer;
      handle->ringbuffer = dest_rb;
      nsamps = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
   }
   else
   {
      be->destroy_ringbuffer(dest_rb);
      return NULL;
   }

   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (handle->info->mode != AAX_MODE_READ)
      {
         if (smixer->emitters_3d || smixer->emitters_2d || smixer->frames)
         {
            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            if (dptr_sensor)
            {
               _aaxNoneDriverProcessFrame(smixer);

               /** process registered devices */
               if (smixer->devices)
               {
                  _aaxMixerInfo* info = smixer->info;
                  _aaxSensorsProcess(dest_rb, smixer->devices, smixer->props2d,
                                     info->track, batched);
               }
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
            }
         }
         else
         {
            if (smixer->capturing)
            {
               _intBuffers *rbs = smixer->play_ringbuffers;
               _aaxRingBuffer *rv;

               rv = dest_rb->duplicate(dest_rb, AAX_TRUE, AAX_FALSE);
               rv->set_state(rv, RB_STARTED);
               rv->set_state(rv, RB_REWINDED);

               _intBufAddData(rbs, _AAX_RINGBUFFER, rv);

               _aaxSignalTrigger(&handle->buffer_ready);
            }
            smixer->curr_pos_sec += delay_sec;
            smixer->curr_sample += nsamps;
         }
      }

      if (handle->finished) {
         _aaxSemaphoreRelease(handle->finished);
      }
      res = _aaxSignalWaitTimed(&handle->thread.signal, delay_sec);
   }
   while (res == AAX_TIMEOUT || res == AAX_TRUE);

   _aaxMutexUnLock(handle->thread.signal.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      be->destroy_ringbuffer(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }

   return handle;
}

void*
_aaxSoftwareMixerThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *smixer;
   int state, tracks;
   float delay_sec;
   int res;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   be = handle->backend.ptr;
   delay_sec = 1.0f/handle->info->period_rate;

   tracks = 2;
   smixer = NULL;
   dest_rb = be->get_ringbuffer(MAX_EFFECTS_TIME, handle->info->mode);
   if (dest_rb)
   {
      dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr_sensor)
      {
         _aaxMixerInfo* info;
         _sensor_t* sensor;

         sensor = _intBufGetDataPtr(dptr_sensor);
         smixer = sensor->mixer;
         info = smixer->info;

         tracks = info->no_tracks;
         dest_rb->set_parami(dest_rb, RB_NO_TRACKS, tracks);
         dest_rb->set_format(dest_rb, AAX_PCM24S, AAX_TRUE);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, info->frequency);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, AAX_TRUE);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = handle->ringbuffer;
   if (!dest_rb) {
      return NULL;
   }

   /* get real duration, it might have been altered for better performance */
   delay_sec = dest_rb->get_paramf(dest_rb, RB_DURATION_SEC);

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   _aaxMutexLock(handle->thread.signal.mutex);
   do
   {
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) || (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->state(handle->backend.handle, DRIVER_PAUSE);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->state(handle->backend.handle, DRIVER_RESUME);
         }
         state = handle->state;
      }

      /* do all the mixing */
      _aaxSoftwareMixerThreadUpdate(handle, handle->ringbuffer);

      if (handle->finished) {
         _aaxSemaphoreRelease(handle->finished);
      }
      res = _aaxSignalWaitTimed(&handle->thread.signal, delay_sec);
   }
   while (res == AAX_TIMEOUT || res == AAX_TRUE);

   _aaxMutexUnLock(handle->thread.signal.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      be->destroy_ringbuffer(handle->ringbuffer);
      handle->ringbuffer = NULL;
   }

   return handle;
}

unsigned int
_aaxSoftwareDriverGetSetSources(unsigned int max, int num)
{
   static unsigned int _max_sources = _AAX_MAX_SOURCES_AVAIL;
   static unsigned int _sources = _AAX_MAX_SOURCES_AVAIL;
   unsigned int abs_num = abs(num);
   unsigned int ret = _sources;

   if (max)
   {
//    static int capabilites = _aaxGetCapabilities(NULL);
//    static int cores = (capabilites & AAX_CPU_CORES)+1;
//    static int simd256 = (capabilites & AAX_SIMD256);

//    if (!simd256) max = 64*cores;

      _aaxAtomicIntSet(&_max_sources, max);     // _max_sources = max;
      _aaxAtomicIntSet(&_sources, max);         // _sources = max;
      ret = max;
   }

   if (abs_num && (abs_num < _AAX_MAX_MIXER_REGISTERED))
   {
      unsigned int _src = _sources - num;
      if ((_sources >= (unsigned int)num) && (_src < _max_sources))
      {
         _aaxAtomicIntSet(&_sources, _src);     // _sources = _src;
         ret = abs_num;
      }
   }

   return ret;
}
