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
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>		/* for struct time */
#endif
#include <assert.h>

#include <aax/aax.h>

#include <base/threads.h>
#include <base/types.h>

#include <api.h>
#include <arch.h>

#define NONE_RENDERER		"None"
#define DEFAULT_RENDERER	AAX_NAME_STR""
#define LOOPBACK_RENDERER	AAX_NAME_STR" Loopback"
#define DEFAULT_OUTPUT_RATE	44100

static _aaxDriverDetect _aaxNoneDriverDetect;
static _aaxDriverNewHandle _aaxNoneDriverNewHandle;
static _aaxDriverGetDevices _aaxNoneDriverGetDevices;
static _aaxDriverGetInterfaces _aaxNoneDriverGetInterfaces;
static _aaxDriverConnect _aaxNoneDriverConnect;
static _aaxDriverDisconnect _aaxNoneDriverDisconnect;
static _aaxDriverSetup _aaxNoneDriverSetup;
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
static _aaxDriverState _aaxNoneDriverState;
static _aaxDriverParam _aaxNoneDriverParam;
static _aaxDriverLog _aaxNoneDriverLog;

const _aaxDriverBackend _aaxNoneDriverBackend =
{
   AAX_VERSION_STR,
   NONE_RENDERER,
   AAX_VENDOR_STR,
   NONE_RENDERER,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxNoneDriverDetect,
   (_aaxDriverNewHandle *)&_aaxNoneDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxNoneDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxNoneDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxNoneDriverGetName,
   (_aaxDriverThread *)&_aaxNoneDriverThread,

   (_aaxDriverConnect *)&_aaxNoneDriverConnect,
   (_aaxDriverDisconnect *)&_aaxNoneDriverDisconnect,
   (_aaxDriverSetup *)&_aaxNoneDriverSetup,
   NULL,
   (_aaxDriverCallback *)&_aaxNoneDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxNoneDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxNoneDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxNoneDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxNoneDriverPostProcess,
   (_aaxDriverPrepare *)&_aaxNoneDriverPrepare,		/* effects */

   (_aaxDriverState *)_aaxNoneDriverState,
   (_aaxDriverParam *)&_aaxNoneDriverParam,
   (_aaxDriverLog *)&_aaxNoneDriverLog
};


static _aaxDriverNewHandle _aaxLoopbackDriverNewHandle;
static _aaxDriverConnect _aaxLoopbackDriverConnect;
static _aaxDriverDisconnect _aaxLoopbackDriverDisconnect;
static _aaxDriver3dMixerCB _aaxLoopbackDriver3dMixer;
static _aaxDriverPrepare3d _aaxLoopbackDriver3dPrepare;
static _aaxDriver2dMixerCB _aaxLoopbackDriverStereoMixer;

static _aaxDriverSetup _aaxLoopbackDriverSetup;
static _aaxDriverParam _aaxLoopbackDriverParam;
static _aaxDriverLog _aaxLoopbackDriverLog;

typedef struct
{
   float latency;
   float frequency;
   enum aaxFormat format;
   uint8_t no_channels;
   uint8_t bits_sample;
   char sse_level;

   _oalRingBufferMix1NFunc *mix_mono3d;

} _driver_t;

char _loopback_default_renderer[100] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxLoopbackDriverBackend =
{
   AAX_VERSION_STR,
   LOOPBACK_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_loopback_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxNoneDriverDetect,
   (_aaxDriverNewHandle *)&_aaxLoopbackDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxNoneDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxNoneDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxNoneDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxLoopbackDriverConnect,
   (_aaxDriverDisconnect *)&_aaxLoopbackDriverDisconnect,
   (_aaxDriverSetup *)&_aaxLoopbackDriverSetup,
   NULL,
   (_aaxDriverCallback *)&_aaxNoneDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxLoopbackDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxLoopbackDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxLoopbackDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)_aaxNoneDriverState,
   (_aaxDriverParam *)&_aaxLoopbackDriverParam,
   (_aaxDriverLog *)&_aaxLoopbackDriverLog
};

static int
_aaxNoneDriverDetect(int mode)
{
   return AAX_TRUE;
}

static void *
_aaxNoneDriverNewHandle(enum aaxRenderMode mode)
{
   return NULL;
}

static void *
_aaxNoneDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   const char *hwstr = _aaxGetSIMDSupportString();

   if (mode == AAX_MODE_READ) return NULL;

   snprintf(_loopback_default_renderer, 99, "%s %s", DEFAULT_RENDERER, hwstr);

   return (void *)&_aaxNoneDriverBackend;
}

static int
_aaxNoneDriverDisconnect(void *id)
{
   return AAX_TRUE;
}

static int
_aaxNoneDriverSetup(const void *id, size_t *bufsize, int *fmt, unsigned int *tracks, float *speed)
{
   return AAX_TRUE;
}

static int
_aaxNoneDriverPlayback(const void *id, void *s, float pitch, float volume)
{
   return 0;
}

static int
_aaxNoneDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   return AAX_FALSE;
}

static void
_aaxNoneDriver3dPrepare(void *sp3d, void *f3d, const void *info, const void *p2d, void* src)
{
}

static void
_aaxNoneDriverPrepare(const void *id, const void *hid, void *s, const void *l)
{
}

static void
_aaxNoneDriverPostProcess(const void *id, void *s, const void *l)
{
}

static int
_aaxNoneDriverStereoMixer(const void *id, void *d, void *s, void *p, void *m, float pitch, float volume, unsigned char ctr, unsigned int nbuf)
{
   return AAX_FALSE;
}

static char *
_aaxNoneDriverGetName(const void *id, int playback)
{
   return NULL;
}

static int
_aaxNoneDriverState(const void *id, enum _aaxDriverState state)
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
   float rv = 0.0f;
   switch(param)
   {
   case DRIVER_LATENCY:
   case DRIVER_MAX_VOLUME:
   case DRIVER_MIN_VOLUME:
   case DRIVER_VOLUME:
   default:
      break;
   }
   return rv;
}

static char *
_aaxNoneDriverLog(const void *id, int prio, int type, const char *str)
{
   return NULL;
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

static void *
_aaxLoopbackDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *rv = calloc(1, sizeof(_driver_t));
   if (rv)
   {
      rv->latency = 0.0f;
      rv->mix_mono3d = _oalRingBufferMixMonoGetRenderer(mode);
   }
   return rv;
}

static void *
_aaxLoopbackDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   if (!handle) {
      handle = _aaxLoopbackDriverNewHandle(mode);
   }
   return (void *)handle;
}


static int
_aaxLoopbackDriverDisconnect(void *id)
{
   free(id);
   return AAX_TRUE;
}

static int
_aaxLoopbackDriverSetup(const void *id, size_t *frames, int *fmt, unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   if (handle)
   {
      handle->format = *fmt;
      handle->frequency = *speed;
      handle->no_channels = *tracks;
      handle->bits_sample = aaxGetBitsPerSample(*fmt);
      
      if (frames && speed && (*speed > 0)) {
         handle->latency = (float)*frames / (float)*speed;
      } else {
         handle->latency = 0.0f;
      }
   }
   return AAX_TRUE;
}

int
_aaxLoopbackDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   _driver_t *handle = (_driver_t *)id;
   return handle->mix_mono3d(d, s, p, m, n, ctr, nbuf);
}

void
_aaxLoopbackDriver3dPrepare(void* sp3d, void* fp3d, const void* info, const void* p2d, void* src)
{
   assert(sp3d);
   assert(info);
   assert(p2d);
   assert(src);

   _oalRingBufferPrepare3d(sp3d, fp3d, info, p2d, src);
}

int
_aaxLoopbackDriverStereoMixer(const void *id, void *d, void *s, void *p, void *m, float pitch, float volume, unsigned char ctr, unsigned int nbuf)
{
   int ret;

   assert(s);
   assert(d);

   ret = _oalRingBufferMixMulti16(d, s, p, m, pitch, volume, ctr, nbuf);

   return ret;
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
      case DRIVER_LATENCY:
         rv = handle->latency;
      case DRIVER_MAX_VOLUME:
         rv = 1.0f;
         break;
      case DRIVER_MIN_VOLUME:
         rv = 0.0f;
         break;
      case DRIVER_VOLUME:
         rv = 1.0f;
         break;
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxLoopbackDriverLog(const void *id, int prio, int type, const char *str)
{
   static char _errstr[256];
   int len = _MIN(strlen(str)+1, 256);

   memcpy(_errstr, str, len);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

static int
_aaxLoopbackDriverCapture(const void *id, void **data, int offs, size_t *size, void *scratch, size_t scratchlen, float gain)
{
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
         if (_IS_PLAYING(src->dprops3d))
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

                  if (_oalRingBufferGetParami(src_rb, RB_IS_PLAYING) == 0)
                  {
                     if (streaming) {
                        _oalRingBufferStartStreaming(src_rb);
                     } else {
                        _oalRingBufferStart(src_rb);
                     }
                  }

                  s_duration = _oalRingBufferGetParamf(src_rb, RB_DURATION_SEC);
                  s_offs = _oalRingBufferGetParamf(src_rb, RB_OFFSET_SEC);
                  if ((s_offs+dt) > s_duration)
                  {
                     if (!_oalRingBufferGetParami(src_rb, RB_LOOPING))
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
                  _oalRingBufferSetParamf(src_rb, RB_OFFSET_SEC, s_offs);
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
                              _SET_STOPPED(src->dprops3d);
                              _SET_PROCESSED(src->dprops3d);
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
                        _SET_PROCESSED(src->dprops3d);
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

   _PROP_MTX_CLEAR_CHANGED(frame->dprops3d->props3d);
   _PROP_PITCH_CLEAR_CHANGED(frame->dprops3d->props3d);
}


void *
_aaxNoneDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame* mixer;
   _aaxTimer *timer;
   _sensor_t* sensor;
   float delay_sec;


   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   dest_rb = _oalRingBufferCreate(REVERB_EFFECTS_TIME);
   if (!dest_rb) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->refresh_rate;

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      sensor = _intBufGetDataPtr(dptr_sensor);
      mixer = sensor->mixer;
      handle->ringbuffer = dest_rb;
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
   }
   else
   {
      _oalRingBufferDelete(dest_rb);
      return NULL;
   }

   timer = _aaxTimerCreate();
   _aaxTimerStartRepeatable(timer, delay_sec);

   _aaxMutexLock(handle->thread.mutex);
   do
   {
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
   while (_aaxTimerWait(timer, handle->thread.mutex) == AAX_TIMEOUT);

   _aaxTimerDestroy(timer);
   _aaxMutexUnLock(handle->thread.mutex);
   _oalRingBufferDelete(dest_rb);

   return handle;
}

