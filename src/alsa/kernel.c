/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
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

#if 0
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#if 0
#if HAVE_IOCTL
# include <sys/ioctl.h>
#endif
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <fcntl.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif
#include <errno.h>
#include <assert.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#endif

#include <aax/aax.h>
#include <xml.h>

#include <base/types.h>
#include <base/logging.h>
#include <base/dlsym.h>

#include <arch.h>
#include <api.h>
#include <driver.h>
#include <devices.h>
#include <ringbuffer.h>

#include <software/renderer.h>
#include "audio.h"
#include "device.h"

#define MAX_NAME		40
#define NO_FRAGMENTS		2
#define DEFAULT_DEVNUM		0
#define	DEFAULT_DEVNAME		"/dev/snd/pcmC0D0p"
#define DEFAULT_MIXER		"/dev/snd/controlC0"
#define DEFAULT_RENDERER	"ALSA"
#define MAX_ID_STRLEN		64

#define _AAX_DRVLOG(a)         _aaxALSADriverLog(id, 0, 0, a)

static _aaxDriverDetect _aaxALSADriverDetect;
static _aaxDriverNewHandle _aaxALSADriverNewHandle;
static _aaxDriverGetDevices _aaxALSADriverGetDevices;
static _aaxDriverGetInterfaces _aaxALSADriverGetInterfaces;
static _aaxDriverConnect _aaxALSADriverConnect;
static _aaxDriverDisconnect _aaxALSADriverDisconnect;
static _aaxDriverSetup _aaxALSADriverSetup;
static _aaxDriverCaptureCallback _aaxALSADriverCapture;
static _aaxDriverCallback _aaxALSADriverPlayback;
static _aaxDriverGetName _aaxALSADriverGetName;
static _aaxDriverRender _aaxALSADriverRender;
static _aaxDriverState _aaxALSADriverState;
static _aaxDriverParam _aaxALSADriverParam;
static _aaxDriverLog _aaxALSADriverLog;

static char _alsa_default_renderer[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxALSADriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_alsa_default_renderer,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxALSADriverDetect,
   (_aaxDriverNewHandle *)&_aaxALSADriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxALSADriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxALSADriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxALSADriverGetName,
   (_aaxDriverRender *)&_aaxALSADriverRender,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxALSADriverConnect,
   (_aaxDriverDisconnect *)&_aaxALSADriverDisconnect,
   (_aaxDriverSetup *)&_aaxALSADriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxALSADriverCapture,
   (_aaxDriverCallback *)&_aaxALSADriverPlayback,

   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxALSADriverState,
   (_aaxDriverParam *)&_aaxALSADriverParam,
   (_aaxDriverLog *)&_aaxALSADriverLog
};

typedef struct
{
   char *name;
   _aaxRenderer *render;

   char *devnode;
   unsigned int card;
   unsigned int device;
   int fd;

   char no_periods;
   char bits_sample;
   float frequency_hz;
   unsigned int format;
   unsigned int no_tracks;
   size_t buffer_size;

   int mode;
   int use_mmap;
   int exclusive;

} _driver_t;

DECL_FUNCTION(ioctl);
DECL_FUNCTION(poll);

static unsigned int _get_minmax(struct pcm_params*, int, unsigned int);

static const int _mode[] = { O_RDONLY, O_WRONLY };
static const char *_const_alsa_default_name = DEFAULT_DEVNAME;
static int _alsa_default_nodenum = DEFAULT_DEVNUM;

static int
_aaxALSADriverDetect(int mode)
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);
     
   if (TEST_FOR_FALSE(rv)) {
      audio = _aaxIsLibraryPresent(NULL, 0);
      if (audio)
      {
         TIE_FUNCTION(ioctl);
         TIE_FUNCTION(poll);
      }
   }

   if (audio && (access(DEFAULT_DEVNAME, F_OK) != -1)) {
      rv = AAX_TRUE;
   }

   return rv;
}

static void *
_aaxALSADriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      handle->name = (char*)_const_alsa_default_name;
      handle->frequency_hz = (float)48000.0f;
      handle->no_tracks = 2;
      handle->use_mmap = AAX_FALSE;
      handle->mode = _mode[(mode > 0) ? 1 : 0];
      handle->exclusive = O_EXCL;
      handle->fd = -1;
   }

   return handle;
}


static void *
_aaxALSADriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxALSADriverNewHandle(mode);
   }

   if (handle)
   {
      if (xid)
      {
         float f;
         char *s;
         int i;

         if (!handle->devnode)
         {
            s = xmlAttributeGetString(xid, "name");
            if (s)
            {
               handle->nodenum = detect_nodenum(s);
               if (handle->name != _const_alsa_default_name) {
                  free(handle->name);
               }
               handle->name = _aax_strdup(s);
               xmlFree(s);
            }
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("alsa; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("alsa; frequency too large.");
               f = (float)_AAX_MAX_MIXER_FREQUENCY;
            }
            handle->frequency_hz = f;
         }

         if (mode != AAX_MODE_READ)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_SYSLOG("alsa; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("alsa; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_tracks = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_SYSLOG("alsa; unsopported bits-per-sample");
               i = 16;
            }
         }

         if (xmlNodeGetBool(xid, "shared")) {
            handle->exclusive = 0;
         }
      }

      if (renderer)
      {
         handle->nodenum = detect_nodenum(renderer);
         if (handle->name != _const_alsa_default_name) {
            free(handle->name);
         }
         handle->name = _aax_strdup(renderer);
      }
   }


   if (handle)
   {
      int fd, m = handle->mode;

      detect_devnode(handle, m);
      fd = open(handle->devnode, handle->mode|handle->exclusive);
      if (fd)
      {
         handle->fd = fd;
      }
      else
      {
         free(handle);
         handle = NULL;
      }
   }

   return (void *)handle;
}

static int
_aaxALSADriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (handle)
   {
      if (handle->name != _const_alsa_default_name) {
         free(handle->name);
      }
      if (handle->devnode)
      {
         if (handle->devnode != _const_alsa_default_name) {
            free(handle->devnode);
         }
         handle->devnode = 0;
      }

      if (handle->render)
      {
         handle->render->close(handle->render->id);
         free(handle->render);
      }

      close(handle->fd);
      free(handle);

      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxALSADriverSetup(const void *id, size_t *frames, int *fmt,
                   unsigned int *tracks, float *speed, int *bitrate)
{
   _driver_t *handle = (_driver_t *)id;
   int err = 0;

   if (handle->fd >= 0)
   {
      struct snd_pcm_hw_params *params;
      int fd = handle->fd;

      params = calloc(1, sizeof(struct snd_pcm_hw_params));
      if (params)
      {
         int n = NDRV_PCM_HW_PARAM_FIRST_MASK;
         while (n++ <= SNDRV_PCM_HW_PARAM_LAST_MASK)
         {
            struct snd_mask *m = param->mask[n - SNDRV_PCM_HW_PARAM_FIRST_MASK];
            m->bits[0] = ~0;
            m->bits[1] = ~0;
         }
         n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         while (n++ <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
         {
            struct snd_interval *i;
 
            i = param->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
            i->max = ~0;
         }
         param->rmask = ~0U;
         param->info = ~0U;

         if (pioctl(fd, SND_PCM_IOCTL_HW_REFINE, params))
         {
            unsigned int channels, format, freq, bits, periods;
            ssize_t no_samples = 1024;

            freq = _get_minmax(params, SNDRV_PCM_HW_PARAM_RATE,
                                       (unsigned int)*speed);
            channels = _get_minmax(params, SNDRV_PCM_HW_PARAM_CHANNELS,
                                           *tracks);
            no_samples = _get_minmax(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
                                             *frames);
            bits = _get_minmax(params, PCM_PARAM_FRAME_BITS,
                                       aaxGetBitsPerSample(*fmt));
            periods = _get_minmax(params, SNDRV_PCM_HW_PARAM_PERIODS,
                                          handle->no_periods);
         }
         free(params);
      }
      else {
         _AAX_DRVLOG("unable to get the device capabilities")  ;
      }
   }

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}


static size_t
_aaxALSADriverCapture(const void *id, void **data, ssize_t *offset, size_t *frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs = *offset;
   size_t rv = AAX_FALSE;
 
   assert(handle->mode == O_RDONLY);

   *offset = 0;
   if ((frames == 0) || (data == 0))
      return rv;

   if (*frames)
   {
      unsigned int tracks = handle->no_tracks;
      unsigned int frame_size = tracks * handle->bits_sample/8;
      size_t no_frames, buflen;
      size_t res;

      no_frames = *frames;
      buflen = no_frames * frame_size;

      res = read(handle->fd, scratch, buflen);
      if (res > 0)
      {
         int32_t **sbuf = (int32_t**)data;

         res /= frame_size;

         _batch_cvt24_16_intl(sbuf, scratch, offs, tracks, res);
         _alsa_set_volume(handle, sbuf, offs, res, tracks, gain);
         *frames = res;

         rv = AAX_TRUE;
      }
      else {
         _AAX_SYSLOG(strerror(errno));
      }
   }

   return AAX_FALSE;
}

static size_t
_aaxALSADriverPlayback(const void *id, void *s, float pitch, float gain)
{
   _aaxRingBuffer *rb = (_aaxRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   ssize_t offs, outbuf_size, no_samples;
   unsigned int no_tracks;
   audio_buf_info info;
   audio_errinfo err;
   int32_t **sbuf;
   int16_t *data;
   size_t res;

   assert(rb);
   assert(id != 0);

   if (pioctl (handle->fd, SNDCTL_DSP_GETERROR, &err) >= 0)
   {
      if (err.play_underruns > 0)
      {
         char str[128];
         snprintf(str, 128, "alsa: %d underruns\n", err.play_underruns);
         _AAX_SYSLOG(str);
      }
      if (err.rec_overruns > 0)
      {
          char str[128];
          snprintf(str, 128, "alsa: %d overruns\n", err.rec_overruns);
          _AAX_SYSLOG(str);
      }
   }

   if (handle->mode == 0)
      return 0;

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;

   outbuf_size = no_tracks *no_samples*sizeof(int16_t);
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

   sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_READ);
   _alsa_set_volume(handle, sbuf, offs, no_samples, no_tracks, gain);

   _batch_cvt16_intl_24(data, (const int32_t**)sbuf, offs, no_tracks, no_samples);
   rb->release_tracks_ptr(rb);

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)data, no_tracks*no_samples);
   }

   pioctl(handle->fd, SNDCTL_DSP_GETOSPACE, &info);
   if (outbuf_size <= (unsigned int)info.fragsize)
   {
      res = write(handle->fd, data, outbuf_size);
      if (res == -1)
      {
         char errstr[1024];
         snprintf(errstr, 1024, "alsa: %s", strerror(errno));
         _AAX_SYSLOG(errstr);
      }
      else if (res != outbuf_size) {
         _AAX_SYSLOG("alsa: warning: pcm write error");
      }
   }

   /* return the number of samples offset to the expected value */
   /* zero would be spot on                                     */
   outbuf_size = info.fragstotal*info.fragsize - outbuf_size;

   return 0; // (info.bytes-outbuf_size)/(no_tracks*sizeof(int16_t));
}

static char *
_aaxALSADriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && handle->devnode)
      ret = _aax_strdup(handle->name);

   return ret;
}

_aaxRenderer*
_aaxALSADriverRender(const void* config)
{
   _driver_t *handle = (_driver_t *)config;
   return handle->render;
}

static int
_aaxALSADriverState(const void *id, enum _aaxDriverState state)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   switch(state)
   {
   case DRIVER_PAUSE:
      if (handle)
      {
         close(handle->fd);
         handle->fd = -1;
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_RESUME:
      if (handle) 
      {
         handle->fd = open(handle->devnode, handle->mode|handle->exclusive);
         if (handle->fd)
         {
            int err, frag, fd = handle->fd;
            unsigned int param;

            if (handle->alsa_version >= ALSA_VERSION_4)
            {
               int enable = 0;
               err = pioctl(fd, SNDCTL_DSP_COOKEDMODE, &enable);
            }

            frag = log2i(handle->buffer_size);
            frag |= NO_FRAGMENTS << 16;
            pioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);

            param = handle->format;
            err = pioctl(fd, SNDCTL_DSP_SETFMT, &param);
            if (err >= 0)
            {
               param = handle->no_tracks;
               err = pioctl(fd, SNDCTL_DSP_CHANNELS, &param);
            }
            if (err >= 0)
            {
               param = (unsigned int)handle->frequency_hz;
               err = pioctl(fd, SNDCTL_DSP_SPEED, &param);
            }
            if (err >= 0) {
               rv = AAX_TRUE;
            }
         }
      }
      break;
   case DRIVER_AVAILABLE:
      if (handle && handle->alsa_version >= ALSA_VERSION_4)
      {
         alsa_audioinfo ainfo;
         int err;

         ainfo.dev = handle->nodenum;
         err = pioctl(handle->fd, SNDCTL_AUDIOINFO_EX, &ainfo);
         if (err >= 0 && ainfo.enabled) {
           rv = AAX_TRUE;
         }
      }
      else {
         rv = AAX_TRUE;
      }
      break;
   case DRIVER_SHARED_MIXER:
      rv = handle->exclusive ? AAX_FALSE : AAX_TRUE;
      break;
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = AAX_TRUE;
      break;
   case DRIVER_NEED_REINIT:
   default:
      break;
   }

   return rv;
}

static float
_aaxALSADriverParam(const void *id, enum _aaxDriverParam param)
{
   _driver_t *handle = (_driver_t *)id;
   float rv = 0.0f;
   if (handle)
   {
      switch(param)
      {
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
         rv = handle->hwgain;
         break;
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxALSADriverGetDevices(const void *id, int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   int m = (mode > 0) ? 1 : 0;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      _driver_t *handle = (_driver_t*)id;

      t_previous[m] = t_now;

      struct snd_pcm_info info;
      if (ioctl(fd, SND_PCM_IOCTL_INFO, &info))
      {
         _AAX_DRVLOG("unable to get card properties");
         return AAX_FALSE;
      }
   }

   return (char *)&names[mode];
}

static char *
_aaxALSADriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;

   return rv;
}

static char *
_aaxALSADriverLog(const void *id, int prio, int type, const char *str)
{
   static char _errstr[256];

   snpnprintf(_errstr, 256, "alsa: %s\n", str);intf(_errstr, 256, "alsa: %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

static int
_alsa_get_volume(_driver_t *handle)
{
   int rv = 0;

   if (handle && handle->fd >= 0)
   {
      int vlr = -1;

      handle->volumeMax = 100;
      if (handle->alsa_version >= ALSA_VERSION_4)
      {
         errno = 0;
         if (handle->mode == O_RDONLY) {
            rv = pioctl(handle->fd, SNDCTL_DSP_GETRECVOL, &vlr);
         } else {
            rv = pioctl(handle->fd, SNDCTL_DSP_GETPLAYVOL, &vlr);
         }
         handle->volumeCur = ((vlr & 0xFF) + ((vlr >> 8) & 0xFF))/2;
      }
      else
      {
         int devs = 0;
         if (handle->mode == O_RDONLY)
         {
            pioctl(handle->fd, SOUND_MIXER_READ_RECMASK, &devs);
            if (devs & SOUND_MASK_IGAIN)
            {
               rv = pioctl(handle->mixfd, SOUND_MIXER_READ_IGAIN, &vlr);
               handle->volumeCur = ((vlr & 0xFF) + ((vlr >> 8) & 0xFF))/2;
            }
         }
         else
         {
            pioctl(handle->fd, SOUND_MIXER_READ_DEVMASK, &devs);
            if (devs & SOUND_MASK_OGAIN)
            {
               rv = pioctl(handle->mixfd, SOUND_MIXER_READ_OGAIN, &vlr);
               handle->volumeCur = ((vlr & 0xFF) + ((vlr >> 8) & 0xFF))/2;
            }
         }
      }
      handle->volumeInit = (float)handle->volumeCur/(float)handle->volumeMax;

      handle->volumeMin = 0;
      if (rv == EINVAL) {
         handle->volumeMax = 0;
      }
   }
   return rv;
}

static void
_alsa_set_volume(_driver_t *handle, int32_t **sbuf, ssize_t offset, size_t no_frames, unsigned int no_tracks, float volume)
{
   float gain = fabsf(volume);
   float hwgain = gain;

   if (handle && HW_VOLUME_SUPPORT(handle))
   {
      long volume;

      hwgain = _MINMAX(gain, handle->volumeMin, handle->volumeMax);
      volume = (hwgain * handle->volumeMax);

      /*
       * Slowly adjust volume to dampen volume slider movement.
       * If the volume step is large, don't dampen it.
       * volume is negative for auto-gain mode.
       */
      if ((volume < 0.0f) && (handle->mode == O_RDONLY))
      {
         float dt = GMATH_E1*no_frames/handle->frequency_hz;
         float rr = _MINMAX(dt/5.0f, 0.0f, 1.0f);       /* 10 sec average */

         /* Quickly adjust for a very large step in volume */
         if (abs(volume - handle->volumeCur) > 82) rr = 0.9f;

         hwgain = (1.0f-rr)*handle->hwgain + (rr)*hwgain;
         handle->hwgain = hwgain;
      }

      volume = (hwgain * handle->volumeMax);
      if (volume != handle->volumeCur)
      {
         int vlr = volume | (volume << 8);
         int rv;

         handle->volumeCur = volume;
         if (handle->alsa_version >= ALSA_VERSION_4)
         {
            if (handle->mode == O_RDONLY) {
               rv = pioctl(handle->fd, SNDCTL_DSP_SETRECVOL, &vlr);
            } else {
               rv = pioctl(handle->fd, SNDCTL_DSP_SETPLAYVOL, &vlr);
            }
            if (rv < 0) volume = handle->volumeMax;
         }
         else
         {
            int devs = 0;
            if (handle->mode == O_RDONLY)
            {
               pioctl(handle->fd, SOUND_MIXER_READ_RECMASK, &devs);
               if (devs & SOUND_MASK_IGAIN) {
                  rv = pioctl(handle->mixfd, SOUND_MIXER_WRITE_IGAIN, &vlr);
               }
            }
            else
            {
               pioctl(handle->fd, SOUND_MIXER_READ_DEVMASK, &devs);
               if (devs & SOUND_MASK_OGAIN) {
                  rv = pioctl(handle->mixfd, SOUND_MIXER_WRITE_OGAIN, &vlr);
               }
            }
         }
      }

      hwgain = (float)volume/handle->volumeMax;
      if (hwgain) gain /= hwgain;
      else gain = 0.0f;
   }

   /* software volume fallback */
   if (sbuf && fabs(hwgain - gain) > 4e-3f)
   {
      int t;
      for (t=0; t<no_tracks; t++) {
         _batch_imul_value((void*)(sbuf[t]+offset), sizeof(int32_t),
                           no_frames, gain);
      }
   }
}


/* -------------------------------------------------------------------------- */

static unsigned int
_get_minmax(struct pcm_params *params, int param, unsigned int value)
{
   int rv = value;
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = pafram->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      rv = _MINMAX(value, itv->min, itv->max);
   }
   return rv;
}

static int
detect_devnode(_driver_t *handle, char mode)
{
   int rv = AAX_FALSE;

   if (handle)
   {
      handle->devnode = (char*)_const_alsa_default_name;
      rv = AAX_TRUE;
   }

   return rv;
}

static int
detect_nodenum(const char *devname)
{
   int version = get_alsa_version();
   int rv = _alsa_default_nodenum;

   if (!strncasecmp(devname, "/dev/dsp", 8) ) {
       rv = atoi(devname+8);
   }
   else if (devname && strcasecmp(devname, "ALSA") &&
                       strcasecmp(devname, "default"))
   {
   }

   return rv;
}

