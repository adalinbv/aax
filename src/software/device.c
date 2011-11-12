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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>		/* for getenv */
#include <assert.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>		/* for struct timeval */

#include <signal.h>
#ifdef SOLARIS /* needed with at least Solaris 8 */
# include <siginfo.h>
#endif


#include <aax.h>
#include <xml.h>

#include <api.h>
#include <arch.h>
#include <devices.h>
#include <base/types.h>
#include <base/logging.h>
#include <base/threads.h>

#include "device.h"

#define DEFAULT_RENDERER	AAX_NAME_STR""
#define DEFAULT_OUTPUT_RATE	22050
#define WAVE_HEADER_SIZE	11
#define WAVE_EXT_HEADER_SIZE	17

#ifndef O_BINARY
# define O_BINARY	0
#endif

static _aaxDriverDetect _aaxSoftwareDriverDetect;
static _aaxDriverGetDevices _aaxSoftwareDriverGetDevices;
static _aaxDriverGetInterfaces _aaxSoftwareDriverGetInterfaces;
static _aaxDriverConnect _aaxSoftwareDriverConnect;
static _aaxDriverDisconnect _aaxSoftwareDriverDisconnect;
static _aaxDriverSetup _aaxSoftwareDriverSetup;
static _aaxDriverState _aaxSoftwareDriverAvailable;
static _aaxDriverState _aaxSoftwareDriverNotAvail;
static _aaxDriverCallback _aaxSoftwareDriverPlayback;
static _aaxDriverGetName _aaxSoftwareDriverGetName;

static char _default_renderer[100] = DEFAULT_RENDERER;
_aaxDriverBackend _aaxSoftwareDriverBackend =
{
   1.0,
   AAX_PCM16S,
   DEFAULT_OUTPUT_RATE,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxSoftwareDriverDetect,
   (_aaxDriverGetDevices *)&_aaxSoftwareDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxSoftwareDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxSoftwareDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareDriverThread,

   (_aaxDriverConnect *)&_aaxSoftwareDriverConnect,
   (_aaxDriverDisconnect *)&_aaxSoftwareDriverDisconnect,
   (_aaxDriverSetup *)&_aaxSoftwareDriverSetup,
   (_aaxDriverState *)&_aaxSoftwareDriverAvailable,
   (_aaxDriverState *)&_aaxSoftwareDriverAvailable,
   NULL,
   (_aaxDriverCallback *)&_aaxSoftwareDriverPlayback,
   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxSoftwareDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareDriverPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareDriverApplyEffects,

   (_aaxDriverState *)&_aaxSoftwareDriverAvailable,
   (_aaxDriverState *)&_aaxSoftwareDriverNotAvail,
   (_aaxDriverState *)&_aaxSoftwareDriverAvailable
};

typedef struct
{
   uint32_t waveHeader[WAVE_EXT_HEADER_SIZE];
   int fd;
   char *name;
   float frequency_hz;
   float update_dt;
   uint32_t size_bytes;
   uint16_t no_channels;
   uint8_t bytes_sample;
   char sse_level;

   int16_t *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif
} _driver_t;

static uint32_t _aaxDefaultWaveHeader[WAVE_EXT_HEADER_SIZE];
static const char *default_renderer = "File: /tmp/AWaveOutput.wav";
#ifndef strdup
char *strdup(const char *);
#endif

static int _aaxSoftwareDriverUpdateHeader(const void *id);
static int _aaxSoftwareDriverThreadUpdate(_handle_t*, _oalRingBuffer*);
static unsigned int _aaxSoftwareDriverMixFrames(_oalRingBuffer*, _intBuffers*);

static int
_aaxSoftwareDriverDetect()
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   return AAX_TRUE;
}

static void *
_aaxSoftwareDriverConnect(const void *id, void *xid, const char *device, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *renderer = (char *)device;

   if (mode == AAX_MODE_READ) return NULL;

   if (!renderer) {
      renderer = (char*)default_renderer;
   }

   if (xid || renderer)
   {
      char *s = NULL;
      int fd = -1;

      if (renderer)
      {

         if (!strncasecmp(renderer, "File:", 5))
         {
            renderer += 5;
            while (*renderer == ' ' && *renderer != '\0') renderer++;
         }

         if (!strcasecmp(renderer, "default")) {
            s = (char *)default_renderer;
         }
         else {
            s = strdup(renderer);
         }
      }
      else if (xid) {
         s = xmlNodeGetString(xid, "renderer");
      }

      if (s && (*s == '~'))
      {
         char *home = getenv("HOME");
         if (home)
         {
            int hlen = strlen(home);
            int slen = strlen(s);
            char *ptr;

            ptr = realloc(s, slen+hlen);
            if (ptr)
            {
               s = ptr+hlen-1;
               memmove(s, ptr, slen+1);
               memcpy(ptr, home, hlen);
               s = ptr;
            }
         }
      }

      if (s) {
         fd = open(s, O_WRONLY|O_CREAT|O_TRUNC, 0644);
      }

      if (fd != -1)
      {
         if (!handle) {
            handle = (_driver_t *)calloc(1, sizeof(_driver_t));
         }

         if (handle)
         {
            const char *hwstr = _aaxGetSIMDSupportString();
            snprintf(_default_renderer, 99, "%s %s", DEFAULT_RENDERER, hwstr);
            handle->sse_level = _aaxGetSSELevel();

            if (xid)
            {
               int i;

               i = xmlNodeGetInt(xid, "frequency-hz");
               if (i) handle->frequency_hz = i;
               if (i)
               {
                  if (i < _AAX_MIN_MIXER_FREQUENCY)
                  {
                     _AAX_SYSLOG("waveout; frequency too small.");
                     i = _AAX_MIN_MIXER_FREQUENCY;
                  }
                  else if (i > _AAX_MAX_MIXER_FREQUENCY)
                  {
                     _AAX_SYSLOG("waveout; frequency too large.");
                     i = _AAX_MAX_MIXER_FREQUENCY;
                  }
                  handle->frequency_hz = i;
               }

               i = xmlNodeGetInt(xid, "channels");
               if (i)
               {
                  if (i < 1)
                  {
                     _AAX_SYSLOG("waveout; no. tracks too small.");
                     i = 1;
                  }
                  else if (i > _AAX_MAX_SPEAKERS)
                  {
                     _AAX_SYSLOG("waveout; no. tracks too great.");
                     i = _AAX_MAX_SPEAKERS;
                  }
                  handle->no_channels = i;
               }

               i = xmlNodeGetInt(xid, "bits-per-sample");
               if (i)
               {
                  if (i != 16)
                  {
                     _AAX_SYSLOG("waveout; unsopported bits-per-sample");
                     i = 16;
                  }
                  handle->bytes_sample = i/8;
               }
            }

            _oalRingBufferMixMonoSetRenderer(mode);
            handle->name = s;
         }
         close(fd);
      }
      else {
         handle = NULL;
      }
   }

   return handle;
}

static int
_aaxSoftwareDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      free(handle->ptr);
      if (handle->name && handle->name != default_renderer) {
         free(handle->name);
      }
      ret = _aaxSoftwareDriverUpdateHeader(id);
      close(handle->fd);
      free(handle);
   }

   return ret;
}

static int
_aaxSoftwareDriverSetup(const void *id, size_t *bufsize, int fmt,
                        unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   int rv;

   assert(handle);

   handle->frequency_hz = *speed;
   handle->size_bytes = 0;

   if (!handle->no_channels) handle->no_channels = *tracks;

   switch(fmt)
   {
   case AAX_PCM8S:
      handle->bytes_sample = 1;
      break;
   case AAX_PCM16S:
      handle->bytes_sample = 2;
      break;
   default:
      return AAX_FALSE;
   }

   handle->fd = open(handle->name, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0644);
   if (handle->fd < 0) return AAX_FALSE;


   memcpy(handle->waveHeader, _aaxDefaultWaveHeader, 4*WAVE_EXT_HEADER_SIZE);
   if (is_bigendian())
   {
      int i;
      for (i=0; i<WAVE_EXT_HEADER_SIZE; i++) {
         handle->waveHeader[i] = _bswap32(handle->waveHeader[i]);
      }
   }

   _aaxSoftwareDriverUpdateHeader(id);
   rv = write(handle->fd, handle->waveHeader, WAVE_HEADER_SIZE*4);

   return (rv != -1) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxSoftwareDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxSoftwareDriverNotAvail(const void *id)
{  
   return AAX_FALSE; 
}

static int
_aaxSoftwareDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
{
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_tracks, no_samples;
   unsigned int offs, outbuf_size;
   _oalRingBufferSample *rbd;
   int16_t *data;
   int res;

   assert(rb);
   assert(rb->sample);
   assert(id != 0);

   rbd = rb->sample;
   offs = _oalRingBufferGetOffsetSamples(rb);
   no_tracks = _oalRingBufferGetNoTracks(rb);
   no_samples = _oalRingBufferGetNoSamples(rb) - offs;

   no_tracks = handle->no_channels;
   assert(no_tracks == handle->no_channels);

   outbuf_size = no_tracks * no_samples*sizeof(int16_t);
   if (handle->ptr == 0)
   {
      char *p = 0;
      handle->ptr = (int16_t *)_aax_malloc(&p, outbuf_size);
      handle->scratch = (int16_t*)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
   data = handle->scratch;
   assert(outbuf_size <= handle->buf_len);

#if 0
{
   unsigned int t;
   for (t=0; t<no_tracks; t++)
   {
      int32_t *ptr = rbd->track[t] + offs;
      unsigned int j;
      j = no_samples-offs;
      do {
if (*ptr > 0x007fffff || -(*ptr) > 0x007ffff) printf("! ptr; %08X (%08X)\n", *ptr, -(*ptr));
         ptr++;
      } while (--j);
   }
}
#endif
   _batch_cvt24_16_intl(data, (const int32_t**)rbd->track, offs, no_tracks, no_samples);

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)data, no_tracks*no_samples);
   }

   res = write(handle->fd, data, outbuf_size);
   if (res == -1)
   {
      _AAX_SYSLOG(strerror(errno));
      return 0;
   }
   handle->size_bytes += res;

   /*
    * Update the file header once every second
    */
   handle->update_dt += _oalRingBufferGetDuration(rb);
   if (handle->update_dt >= 1.0f)
   {
      _aaxSoftwareDriverUpdateHeader(id);
      handle->update_dt -= 1.0f;
   }

   return 0;
}

int
_aaxSoftwareDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n)
{
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxSoftwareDriverBackend.gain;
   ret = _oalRingBufferMixMono16(d, s, p, m, gain, n);

   return ret;
}

void
_aaxSoftwareDriver3dPrepare(void* sp3d, void* fp3d, const void* info, const void* p2d, void* src)
{
   assert(sp3d);
   assert(info);
   assert(p2d);
   assert(src);

   _oalRingBufferPrepare3d(sp3d, fp3d, info, p2d, src);
}

void
_aaxSoftwareDriverApplyEffects(const void *id, void *drb, const void *props2d)
{
   _oalRingBuffer2dProps *p2d = (_oalRingBuffer2dProps*)props2d;
   _oalRingBuffer *rb = (_oalRingBuffer *)drb;
   _oalRingBufferDelayEffectData* delay_effect;
   _oalRingBufferFreqFilterInfo* freq_filter;
   _oalRingBufferSample *rbd;
   void* distortion_effect;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;

   delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
   distortion_effect = _EFFECT_GET_DATA(p2d, DISTORTION_EFFECT);
   freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
   if (delay_effect || freq_filter || distortion_effect)
   {
      unsigned int dde_bytes, track_len_bytes;
      unsigned int track, tracks, dno_samples;
      unsigned int bps, dde_samps;

      bps = rbd->bytes_sample;
      dde_samps = rbd->dde_samples;
      track_len_bytes = rbd->track_len_bytes;
      dde_bytes = dde_samps * bps;
      dno_samples = rbd->no_samples;

      tracks = rbd->no_tracks;
      for (track=0; track<tracks; track++)
      {
         int32_t *scratch = rbd->scratch[SCRATCH_BUFFER1];
         int32_t *d2 = rbd->scratch[SCRATCH_BUFFER0];
         int32_t *d1 = rbd->track[track];
         char *cd2 = (char*)d2 - dde_bytes;
         char *cd1 = (char*)d1 - dde_bytes;

         /* save the unmodified next dde buffer for later use */
         _aax_memcpy(cd2-dde_bytes, cd1+track_len_bytes, dde_bytes);

         /* copy the data for processing */
         if ( distortion_effect) {
            distortion_effect = &_EFFECT_GET(p2d, DISTORTION_EFFECT, 0);
         }
         _aax_memcpy(cd2, cd1, dde_bytes+track_len_bytes);
         bufEffectsApply(d1, d2, scratch, 0, dno_samples, dde_samps, track,
                         freq_filter, delay_effect, distortion_effect);

         /* restore the unmodified next dde buffer */
         _aax_memcpy(cd1, cd2-dde_bytes, dde_bytes);
#if 0
{
   unsigned int i, diff = 0;
   for (i=0; i<dno_samples; i++) {
      if (d2[i] != d1[i]) diff++;
   }
   printf("no diff samples: %i\n", diff);
}
#endif
      }
   }
}

void
_aaxSoftwareDriverPostProcess(const void *id, void *d, const void *s)
{
   _oalRingBuffer *rb = (_oalRingBuffer*)d;
   _sensor_t *sensor = (_sensor_t*)s;
   unsigned int track, tracks;
   _oalRingBufferSample *rbd;
   void *ptr = 0;
   char *p;

   assert(rb != 0);
   assert(rb->sample != 0);

   rbd = rb->sample;

   if (sensor && _FILTER_GET_DATA(sensor, EQUALIZER_LF)
        && _FILTER_GET_DATA(sensor, EQUALIZER_HF))
   {
      p = 0;
      ptr = _aax_malloc(&p, 2*rbd->track_len_bytes);
   }

   /* set up this way because we always need to apply compression */
   tracks = rbd->no_tracks;
   for (track=0; track<tracks; track++)
   {
      int32_t *d1 = (int32_t *)rbd->track[track];
      unsigned int dmax = rbd->no_samples;

      if (ptr)
      {
         _oalRingBufferFreqFilterInfo* filter;
         int32_t *d2 = (int32_t *)p;
         int32_t *d3 = d2 + dmax;

         _aax_memcpy(d3, d1, rbd->track_len_bytes);
         filter = sensor->filter[EQUALIZER_LF].data;
         bufFilterFrequency(d1, d3, 0, dmax, 0, track, filter);

         filter = sensor->filter[EQUALIZER_HF].data;
         bufFilterFrequency(d2, d3, 0, dmax, 0, track, filter);
         _batch_fmadd(d1, d2, dmax, 1.0, 0.0);
      }
      _aaxProcessCompression(d1, 0, dmax);
   }
   free(ptr);
}

int
_aaxSoftwareDriverStereoMixer(const void *id, void *d, void *s, void *p, void *m, float pitch, float volume)
{
   int ret;

   assert(s);
   assert(d);

   volume *= _aaxSoftwareDriverBackend.gain;
   ret = _oalRingBufferMixMulti16(d, s, p, m, pitch, volume);

   return ret;
}

char *
_aaxSoftwareDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = "default";

   if (handle)
   {
      if (!handle->name)
         handle->name = strdup("default");

      ret = handle->name;
   }

   return ret;
}

char *
_aaxSoftwareDriverGetDevices(const void *id, int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "File\0\0"
   };
   return (char *)rd[mode];
}

static char *
_aaxSoftwareDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static const char *rd[2] = {
    "\0\0",
    "~/"AAX_NAME_STR"Out.wav\0/tmp/"AAX_NAME_STR"Out.wav\0\0"
   };
   return (char *)rd[mode];
}

#if 0
pthread_key_t handle_key = 0;
pthread_key_t rb_key = 0;
void
_aaxSoftwareDriverSigHandler(int cause, void *HowCome, void *ptr)
{
   siginfo_t *reason = (siginfo_t *)HowCome;
   _oalRingBuffer *dest_rb;
   _handle_t *handle;

#if 0
   handle = pthread_getspecific(handle_key);
   dest_rb = pthread_getspecific(rb_key);
#endif

#if 0
// _IS_STANDBY(handle);

   if (state != handle->state)
   {
      const _aaxDriverBackend *be = handle->backend.ptr;

      if (_IS_PAUSED(handle) || (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
         be->pause(handle->backend.handle);
      }
      else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
         be->resume(handle->backend.handle);
      }
      state = handle->state;
   }
#endif

   /* do all the mixing */
//   _aaxSoftwareDriverThreadUpdate(handle, dest_rb);
}

void*
_aaxSoftwareDriverThreadSignal(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   struct sigaction sa;
   float delay_sec;
   int tracks;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   be = handle->backend.ptr;
   delay_sec = 1.0/handle->info->refresh_rate;

   tracks = 2;
   mixer = NULL;
   dest_rb = _oalRingBufferCreate(AAX_TRUE);
   if (dest_rb)
   {
      dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr_sensor)
      {
         _aaxMixerInfo* info;
         _sensor_t* sensor;

         sensor = _intBufGetDataPtr(dptr_sensor);
         mixer = sensor->mixer;
         info = mixer->info;

         if (info->mode == AAX_MODE_READ) {
            _oalRingBufferSetFormat(dest_rb, be->codecs, info->format);
         } else {
            _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
         }
         tracks = info->no_tracks;
         _oalRingBufferSetNoTracks(dest_rb, tracks);
         _oalRingBufferSetFrequency(dest_rb, info->frequency);
         _oalRingBufferSetDuration(dest_rb, delay_sec);
         _oalRingBufferInit(dest_rb, AAX_TRUE);
         _oalRingBufferStart(dest_rb);

         mixer->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }
   dest_rb = mixer->ringbuffer;
   if (!dest_rb) {
      return NULL;
   }

   /* get real duration, it might have been altered for better performance */
   delay_sec = _oalRingBufferGetDuration(dest_rb);

   /* Install our SIGPROF signal handler */
   sa.sa_sigaction = _aaxSoftwareDriverSigHandler;
   sigemptyset( &sa.sa_mask );
   sa.sa_flags = SA_SIGINFO; /* we want a siginfo_t */
   if (sigaction(SIGALRM, &sa, 0) == 0)
   {
      struct itimerval itimer;
      int res;

      res = pthread_key_create(&handle_key, NULL);
      res = pthread_setspecific(handle_key, handle);

      res = pthread_key_create(&rb_key, NULL);
      res = pthread_setspecific(rb_key, dest_rb);

{
_oalRingBuffer *dest_rb;
   _handle_t *handle;

   handle = pthread_getspecific(handle_key);
   dest_rb = pthread_getspecific(rb_key);

}

      /* Request SIGPROF */
      itimer.it_interval.tv_sec = 0;
      itimer.it_interval.tv_usec = delay_sec*1e6f;
      itimer.it_value.tv_sec = 0;
      itimer.it_value.tv_usec = delay_sec*1e6f;
      if(setitimer(ITIMER_REAL, &itimer, NULL) == 0)
      {
         _aaxMutexLock(handle->thread.mutex);
         _aaxConditionWait(handle->thread.condition, handle->thread.mutex);
         _aaxMutexUnLock(handle->thread.mutex);
      }

      /* stop the timer */
      itimer.it_interval.tv_usec = 0;
      itimer.it_value.tv_usec = 0;
      setitimer(ITIMER_PROF, &itimer, NULL);

      pthread_key_delete(rb_key);
       pthread_key_delete(handle_key);
   }
   else {
      _AAX_SYSLOG("software driver: Unable to register the signal handler");
   }

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferStop(mixer->ringbuffer);
      _oalRingBufferDelete(mixer->ringbuffer);
   }

   return handle;
}
#endif

void*
_aaxSoftwareDriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   _intBufferData *dptr_sensor;
   const _aaxDriverBackend *be;
   _oalRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   unsigned int bufsz;
   struct timespec ts;
   float delay_sec;
   float dt = 0.0;
   float elapsed;
   int state;
   int tracks;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   be = handle->backend.ptr;
   delay_sec = 1.0f/handle->info->refresh_rate;

   tracks = 2;
   mixer = NULL;
   dest_rb = _oalRingBufferCreate(AAX_TRUE);
   if (dest_rb)
   {
      dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
      if (dptr_sensor)
      {
         _aaxMixerInfo* info;
         _sensor_t* sensor;

         sensor = _intBufGetDataPtr(dptr_sensor);
         mixer = sensor->mixer;
         info = mixer->info;

         if (info->mode == AAX_MODE_READ) {
            _oalRingBufferSetFormat(dest_rb, be->codecs, info->format);
         } else {
            _oalRingBufferSetFormat(dest_rb, be->codecs, AAX_PCM24S);
         }
         tracks = info->no_tracks;
         _oalRingBufferSetNoTracks(dest_rb, tracks);
         _oalRingBufferSetFrequency(dest_rb, info->frequency);
         _oalRingBufferSetDuration(dest_rb, delay_sec);
         _oalRingBufferInit(dest_rb, AAX_TRUE);
         _oalRingBufferStart(dest_rb);

         mixer->ringbuffer = dest_rb;
         _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
      }
   }

   dest_rb = mixer->ringbuffer;
   if (!dest_rb) {
      return NULL;
   }

   /* get real duration, it might have been altered for better performance */
   bufsz = _oalRingBufferGetNoSamples(dest_rb);
   delay_sec = _oalRingBufferGetDuration(dest_rb);

   be->pause(handle->backend.handle);
   state = AAX_SUSPENDED;

   elapsed = 0.0f;
   _aaxMutexLock(handle->thread.mutex);
   do
   {
      static int res = 0;
      float time_fact = 1.0f -(float)res/(float)bufsz;
      float delay = delay_sec*time_fact;

if (res != 0 && res != 1) printf("res: %i\n\t\t\t", res);
      /* once per second when standby */
//    if (_IS_STANDBY(handle)) delay = 1.0f;

      elapsed -= delay;
      if (elapsed <= 0.0f)
      {
         struct timeval now;
         float fdt;

         elapsed += 60.0f;               /* resync the time every 60 seconds */

         dt = delay;
         fdt = floorf(dt);

         gettimeofday(&now, 0);
         ts.tv_sec = now.tv_sec + fdt;

         dt -= fdt;
         dt += now.tv_usec*1e-6f;
         ts.tv_nsec = dt*1e9f;
         if (ts.tv_nsec >= 1e9f)
         {
            ts.tv_sec++;
            ts.tv_nsec -= 1e9f;
         }
      }
      else
      {
         dt += delay;
         if (dt >= 1.0f)
         {
            float fdt = floorf(dt);
            ts.tv_sec += fdt;
            dt -= fdt;
         }
         ts.tv_nsec = dt*1e9f;
      }

      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) || (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->pause(handle->backend.handle);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->resume(handle->backend.handle);
         }
         state = handle->state;
      }

      /* do all the mixing */
      res = _aaxSoftwareDriverThreadUpdate(handle, dest_rb);
   }
   while (_aaxConditionWaitTimed(handle->thread.condition, handle->thread.mutex, &ts) == ETIMEDOUT);

   _aaxMutexUnLock(handle->thread.mutex);

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _oalRingBufferStop(mixer->ringbuffer);
      _oalRingBufferDelete(mixer->ringbuffer);
   }

   return handle;
}

void
_aaxSoftwareDriverProcessFrame(void* rb, void* info, void *sp2d, void *sp3d, void *fp2d, void *fp3d, void *e2d, void *e3d, const void* backend, void* handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _oalRingBuffer *dest_rb = (_oalRingBuffer*)rb;
   _oalRingBuffer3dProps *props3d;
   _oalRingBuffer2dProps *props2d;
   _intBuffers *he;
   int stage;
   float dt;

   dt = _oalRingBufferGetDuration(dest_rb);
   props2d = fp2d ? (_oalRingBuffer2dProps*)fp2d : (_oalRingBuffer2dProps*)sp2d;
   props3d = fp3d ? (_oalRingBuffer3dProps*)fp2d : (_oalRingBuffer3dProps*)sp2d;

   stage = 0;
   he = e3d;
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

         dest_rb->curr_pos_sec = 0.0f;

         emitter = _intBufGetDataPtr(dptr_src);
         src = emitter->source;
         if (_IS_PLAYING(src))
         {
            _intBufferData *dptr_sbuf;
            unsigned int nbuf, rv = 0;
            int streaming;

            nbuf = _intBufGetNum(src->buffers, _AAX_EMITTER_BUFFER);
            assert(nbuf > 0);

            streaming = (nbuf > 1);
            dptr_sbuf = _intBufGet(src->buffers, _AAX_EMITTER_BUFFER, src->pos);
            if (dptr_sbuf)
            {
               _embuffer_t *embuf = _intBufGetDataPtr(dptr_sbuf);
               _oalRingBuffer *src_rb = embuf->ringbuffer;
               do
               {
                  if (_IS_STOPPED(src)) {
                     _oalRingBufferStop(src_rb);
                  }
                  else if (_oalRingBufferTestPlaying(src_rb) == 0)
                  {
                     if (streaming) {
                        _oalRingBufferStartStreaming(src_rb);
                     } else {
                        _oalRingBufferStart(src_rb);
                     }
                  }

                  /* 3d mixing */
                  if (stage == 0)
                  {
                     assert(_IS_POSITIONAL(src));
                     be->prepare3d(sp3d, fp3d, info, props2d, src);
                     if (src->curr_pos_sec >= props2d->delay_sec)
                        rv = be->mix3d(handle, dest_rb, src_rb, src->props2d,
                                               props2d, emitter->track);
                  }
                  else
                  {
                     assert(!_IS_POSITIONAL(src));
                     rv = be->mix2d(handle, dest_rb, src_rb, src->props2d,
                                           props2d, 1.0, 1.0);
                  }

                  src->curr_pos_sec += dt;

                  /*
                   * The current buffer of the source has finished playing.
                   * Decide what to do next.
                   */
                  if (rv)
                  {
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

                        rv &= _oalRingBufferTestPlaying(dest_rb);
                        if (rv)
                        {
                           _intBufReleaseData(dptr_sbuf,_AAX_EMITTER_BUFFER);
                           dptr_sbuf = _intBufGet(src->buffers,
                                              _AAX_EMITTER_BUFFER, src->pos);
                           embuf = _intBufGetDataPtr(dptr_sbuf);
                           src_rb = embuf->ringbuffer;
                        }
                     }
                     else /* !streaming */
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
            _oalRingBufferStart(dest_rb);
         }
         _intBufReleaseData(dptr_src, _AAX_EMITTER);
      }
      _intBufReleaseNum(he, _AAX_EMITTER);

      if (stage == 0)   /* 3d stage */
      {
         /* apply environmental effects */
         if (num > 0) {
            be->effects(handle, dest_rb, props2d);
         }
         he = e2d;
      }
   }
   while (++stage < 2); /* positional and stereo */

   _PROP_MTX_CLEAR_CHANGED(props3d);
   _PROP_PITCH_CLEAR_CHANGED(props3d);
}

unsigned int
_aaxSoftwareDriverSignalFrames(void *frames)
{
   _intBuffers *hf = (_intBuffers*)frames;
   unsigned int num = 0;

   if (hf)
   {
      unsigned int i;

      num = _intBufGetMaxNum(hf, _AAX_FRAME);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hf, _AAX_FRAME, i);
         if (dptr)
         {
            _frame_t* frame = _intBufGetDataPtr(dptr);
            // _intBuffers *ringbuffers = frame->submix->ringbuffers;

            if TEST_FOR_TRUE(frame->submix->capturing)
            {
               frame->thread.update = 1;
               _aaxConditionSignal(frame->thread.condition);
            }
            _intBufReleaseData(dptr, _AAX_FRAME);
         }
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
   }
   return num;
}

int
_aaxSoftwareDriverPlayFrame(void* frame, const void* backend, void* sensor, void* handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _aaxAudioFrame* mixer = (_aaxAudioFrame*)frame;
   _oalRingBuffer *dest_rb = mixer->ringbuffer;
   int res;

   /* postprocess registered audio frames */
   if (mixer->frames) {
      _aaxSoftwareDriverMixFrames(dest_rb, mixer->frames);
   }
   be->postprocess(handle, dest_rb, sensor);

   /* play all mixed audio */
   res = be->play(handle, 0, dest_rb, 1.0, 1.0);

   if TEST_FOR_TRUE(mixer->capturing)
   {
      unsigned int nbufs;
 
      _oalRingBufferForward(dest_rb);
      _intBufAddData(mixer->ringbuffers, _AAX_RINGBUFFER, dest_rb);

      nbufs = _intBufGetNum(mixer->ringbuffers, _AAX_RINGBUFFER);
      if (nbufs == 1) {
         dest_rb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE);
      }
      else
      {
         void **ptr;

         ptr = _intBufShiftIndex(mixer->ringbuffers, _AAX_RINGBUFFER, 0, 1);
         assert(ptr != NULL);

         dest_rb = (_oalRingBuffer *)ptr[0];
         free(ptr);
      }
      _intBufReleaseNum(mixer->ringbuffers, _AAX_RINGBUFFER);
      mixer->ringbuffer = dest_rb;
   }

   _oalRingBufferClear(dest_rb);
   _oalRingBufferStart(dest_rb);

   return res;
}

float
_aaxSoftwareDriverReadFrame(void *config, const void* backend, void *handle)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   _aaxAudioFrame* mixer = (_aaxAudioFrame*)config;
   _oalRingBuffer *dest_rb;
   size_t tracksize;
   float rv = 0;
   char *dbuf;
   int res;

   /*
    * dest_rb is thread specific and does not need a lock
    */
   dest_rb = mixer->ringbuffer;
   dbuf = dest_rb->sample->scratch[0];
   tracksize = _oalRingBufferGetTrackSize(dest_rb);

   res = be->record(handle, dbuf, &tracksize, 1.0, 1.0);
   if TEST_FOR_TRUE(res)
   {
      _oalRingBuffer *nrb;

      _oalRingBufferFillInterleaved(dest_rb, dbuf, 1, AAX_FALSE);
      nrb = _oalRingBufferDuplicate(dest_rb, AAX_FALSE);

       _oalRingBufferForward(dest_rb);
       /*
        * mixer->ringbuffers is shared between threads and needs
        * to be locked
        */
       _intBufAddData(mixer->ringbuffers, _AAX_RINGBUFFER, dest_rb);

       mixer->ringbuffer = nrb;
       rv = 1.0f/mixer->info->refresh_rate;
   }
   return rv;
}

/*-------------------------------------------------------------------------- */

static uint32_t _aaxDefaultWaveHeader[WAVE_EXT_HEADER_SIZE] =
{
    0x46464952,                 /*  0. "RIFF"                                */
    0x00000024,                 /*  1. (file_length - 8)                     */
    0x45564157,                 /*  2. "WAVE"                                */

    0x20746d66,                 /*  3. "fmt "                                */
    0x00000010,                 /*  4.                                       */
    0x00020001,                 /*  5. PCM & stereo                          */
    DEFAULT_OUTPUT_RATE,        /*  6.                                       */
    0x0001f400,                 /*  7. (sample_rate*channels*bits_sample/8)  */
    0x00100004,                 /*  8. (channels*bits_sample/8)              *
                                 *     & 16 bits per sample                  */
/* used for both the extensible data section and data section */
    0x61746164,                 /*  9. "data"                                */
    0,                          /* 10. length of the data block              *
                                 *     (sampels*channels*bits_sample/8)      */
    0,0,
/* data section starts here in case of the extensible format */
    0x61746164,                 /* 15. "data"                                */
    0
};

static int
_aaxSoftwareDriverUpdateHeader(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int res = 0;

   if (handle->size_bytes != 0)
   {
      unsigned int fmt, size = handle->size_bytes;
      uint32_t s;
      off_t floc;

      s =  WAVE_HEADER_SIZE*4 - 8 + size;
      handle->waveHeader[1] = s;

      fmt = handle->waveHeader[5] & 0xFFF;
      s = (handle->no_channels << 16) | fmt;	/* PCM */
      handle->waveHeader[5] = s;

      s = handle->frequency_hz;
      handle->waveHeader[6] = s;

      s *= handle->no_channels * handle->bytes_sample;
      handle->waveHeader[7] = s;

      s = size;
      handle->waveHeader[10] = s;

      if (is_bigendian())
      {
         handle->waveHeader[1] = _bswap32(handle->waveHeader[1]);
         handle->waveHeader[5] = _bswap32(handle->waveHeader[5]);
         handle->waveHeader[6] = _bswap32(handle->waveHeader[6]);
         handle->waveHeader[7] = _bswap32(handle->waveHeader[7]);
         handle->waveHeader[10] = _bswap32(handle->waveHeader[10]);
      }

      floc = lseek(handle->fd, 0L, SEEK_CUR);
      lseek(handle->fd, 0L, SEEK_SET);
      res = write(handle->fd, handle->waveHeader, WAVE_HEADER_SIZE*4);
      lseek(handle->fd, floc, SEEK_SET);
      if (res == -1) {
         _AAX_SYSLOG(strerror(errno));
      }

#if 0
printf("Write:\n");
printf(" 0: %08x\n", handle->waveHeader[0]);
printf(" 1: %08x\n", handle->waveHeader[1]);
printf(" 2: %08x\n", handle->waveHeader[2]);
printf(" 3: %08x\n", handle->waveHeader[3]);
printf(" 4: %08x\n", handle->waveHeader[4]);
printf(" 5: %08x\n", handle->waveHeader[5]);
printf(" 6: %08x\n", handle->waveHeader[6]);
printf(" 7: %08x\n", handle->waveHeader[7]);
printf(" 8: %08x\n", handle->waveHeader[8]);
printf(" 9: %08x\n", handle->waveHeader[9]);
printf("10: %08x\n", handle->waveHeader[10]);
#endif
   }

   return res;
}

/**
 * Write a canonical WAVE file from memory to a file.
 *
 * @param a pointer to the exact ascii file location
 * @param no_samples number of samples per audio track
 * @param fs sample frequency of the audio tracks
 * @param no_tracks number of audio tracks in the buffer
 * @param format audio format
 */
void
_aaxSoftwareDriverWriteFile(const char *file, enum aaxProcessingType type,
                            void *buffer, unsigned int no_samples,
                            unsigned int freq, char no_tracks,
                            enum aaxFormat format)
{
   uint32_t waveHeader[WAVE_EXT_HEADER_SIZE];
   unsigned int size;
   int fd, res, oflag;
   int fmt, bps;
   uint32_t s;
   off_t floc;

   switch (format) {
   case AAX_PCM8S:
   case AAX_PCM16S:
   case AAX_PCM24S:
   case AAX_PCM32S:
      fmt = 0x1;
      break;
   case AAX_FLOAT:
   case AAX_DOUBLE:
      fmt = 0x3;
      break;
   case AAX_ALAW:
      fmt = 0x6;
      break;
   case AAX_MULAW:
      fmt = 0x7;
      break;
   default:
      _AAX_SYSLOG("File: unsupported format");
      return;
   }

   oflag = O_CREAT|O_WRONLY|O_BINARY;
   if (type == AAX_OVERWRITE) oflag |= O_TRUNC;
// if (type == AAX_APPEND) oflag |= O_APPEND;
   fd = open(file, oflag, 0644);
   if (fd < 0)
   {
      printf("Error: Unable to write to file.\n");
      return;
   }

   memcpy(waveHeader, _aaxDefaultWaveHeader, WAVE_EXT_HEADER_SIZE*4);

   bps = _oalRingBufferFormatsBPS[format];

   floc = lseek(fd, 0L, SEEK_END);
   size = floc - WAVE_EXT_HEADER_SIZE*4;
   size += no_samples * no_tracks * bps;
   s = WAVE_HEADER_SIZE*4 - 8 + size;
   waveHeader[1] = s;

   s = (no_tracks << 16) | fmt;
   waveHeader[5] = s;

   s = freq;
   waveHeader[6] = s;

   s *= no_tracks * bps;
   waveHeader[7] = s;

   s = size;
   waveHeader[10] = s;

   if (is_bigendian())
   {
      waveHeader[1] = _bswap32(waveHeader[1]);
      waveHeader[5] = _bswap32(waveHeader[5]);
      waveHeader[6] = _bswap32(waveHeader[6]);
      waveHeader[7] = _bswap32(waveHeader[7]);
      waveHeader[10] = _bswap32(waveHeader[10]);
   }

   lseek(fd, 0L, SEEK_SET);
   res = write(fd, &waveHeader, WAVE_HEADER_SIZE*4);
   if (res == -1) {
      _AAX_SYSLOG(strerror(errno));
   }

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)buffer, no_samples*no_tracks);
   }

   if (type == AAX_APPEND) {
      lseek(fd, floc, SEEK_SET);
   }
   size = no_samples * no_tracks * bps;
   res = write(fd, buffer, size);
   if (res == -1) {
      _AAX_SYSLOG(strerror(errno));
   }

   close(fd);
}

static unsigned int
_aaxSoftwareDriverMixFrames(_oalRingBuffer *dest_rb, _intBuffers *hf)
{
   unsigned int i, num = 0;
   if (hf)
   {
      num = _intBufGetMaxNum(hf, _AAX_FRAME);
      for (i=0; i<num; i++)
      {
         _intBufferData *dptr = _intBufGet(hf, _AAX_FRAME, i);
         if (dptr)
         {
            static struct timespec sleept = {0, 1000};
            _frame_t* frame = _intBufGetDataPtr(dptr);
            _aaxAudioFrame *mixer = frame->submix;
            _intBuffers *ringbuffers;
            float sleep;
            int p = 0;

            sleep = 0.1f / frame->submix->info->refresh_rate;
            sleept.tv_nsec = sleep * 1e9f;

            /* Can't call aaxAudioFrameWaitForBuffer because of a dead-lock */
            ringbuffers = mixer->ringbuffers;
            while ((mixer->capturing == 1) && (p++ < 500))
            {
               _intBufReleaseData(dptr, _AAX_FRAME);

               nanosleep(&sleept, 0);

               dptr = _intBufGet(hf, _AAX_FRAME, i);
               if (!dptr) break;
               frame = _intBufGetDataPtr(dptr);
            }

            if (dptr)
            {
               if (mixer->capturing > 1)
               {
                  _intBufferData *buf;
                  _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
                  buf = _intBufPopData(ringbuffers, _AAX_RINGBUFFER);
                  _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);

                  if (buf)
                  {
                     unsigned int dno_samples;
                     unsigned char track, tracks;
                     _oalRingBuffer *src_rb;

                     dno_samples =_oalRingBufferGetNoSamples(dest_rb);
                     tracks=_oalRingBufferGetNoTracks(dest_rb);
                     src_rb = _intBufGetDataPtr(buf);

                     for (track=0; track<tracks; track++)
                     {
                        int32_t *data = dest_rb->sample->track[track];
                        int32_t *sptr = src_rb->sample->track[track];
                        _batch_fmadd(data, sptr, dno_samples, 1.0, 0.0);
                     }

                     _intBufGetNum(ringbuffers, _AAX_RINGBUFFER);
                     _intBufPushData(ringbuffers, _AAX_RINGBUFFER, buf);
                     _intBufReleaseNum(ringbuffers, _AAX_RINGBUFFER);
                     mixer->capturing = 1;
                  }
               }
               _intBufReleaseData(dptr, _AAX_FRAME);
            }
         }
#if 0
for (i=0; i<_oalRingBufferGetNoTracks(dest_rb); i++) {
   unsigned int dno_samples =_oalRingBufferGetNoSamples(dest_rb);
   int32_t *data = dest_rb->sample->track[i];
   _batch_saturate24(data, dno_samples);
}
#endif
      }
      _intBufReleaseNum(hf, _AAX_FRAME);
   }
   return num;
}

static int
_aaxSoftwareDriverThreadUpdate(_handle_t *handle, _oalRingBuffer *dest_rb)
{
   const _aaxDriverBackend* be;
   _intBufferData *dptr_sensor;
   _aaxAudioFrame *mixer;
   _sensor_t* sensor;
   int res = 0;

   assert(handle);
   assert(handle->sensors);
   assert(handle->backend.ptr);
   assert(handle->info->no_tracks);

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      sensor = _intBufGetDataPtr(dptr_sensor);
      mixer = sensor->mixer;
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
   }
   else {
      return 0;
   }

   be = handle->backend.ptr;
   if (_IS_PLAYING(handle) || _IS_STANDBY(handle))
   {
      void* be_handle = handle->backend.handle;

      if (_IS_PLAYING(handle) && be->is_available(be_handle))
      {
         if (handle->info->mode == AAX_MODE_READ)
         {
            float dt;

            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            dt = _aaxSoftwareDriverReadFrame(mixer, be, be_handle);
            mixer->curr_pos_sec += dt;
            _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
         }
         else if (mixer->emitters_3d || mixer->emitters_2d)
         {
            _oalRingBuffer2dProps sp2d;
            _oalRingBuffer3dProps sp3d;

            /* signal frames to update */
            _aaxSoftwareDriverSignalFrames(mixer->frames);

            /* copying here prevents locking the listener the whole time */
            /* it's used for just one time-frame anyhow                  */
            dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
            sensor = _intBufGetDataPtr(dptr_sensor);
            memcpy(&sp2d, mixer->props2d, sizeof(_oalRingBuffer2dProps));
            memcpy(&sp2d.pos, handle->info->speaker,
                               _AAX_MAX_SPEAKERS*sizeof(vec4));
            memcpy(&sp2d.hrtf, handle->info->hrtf, 2*sizeof(vec4));
            memcpy(&sp3d, mixer->props3d, sizeof(_oalRingBuffer3dProps));
            _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

            _aaxSoftwareDriverProcessFrame(mixer->ringbuffer, handle->info,
                                           &sp2d, &sp3d, NULL, NULL,
                                           mixer->emitters_2d,
                                           mixer->emitters_3d,
                                           be, be_handle);

            res = _aaxSoftwareDriverPlayFrame(mixer, be, sensor, be_handle);
         }
      }
      else /* if (_IS_STANDBY(handle) */
      {
         if (handle->info->mode != AAX_MODE_READ)
         {
            if (mixer->emitters_3d || mixer->emitters_2d)
            {
               dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               _aaxNoneDriverProcessFrame(mixer);
               _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
            }
         }
      }
   }

   return res;
}

