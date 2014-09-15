/*);
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

#include <stdlib.h>		// exit
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
#if HAVE_MMAN_H
# include <sys/mman.h>
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
#include "kernel.h"
#include "device.h"

#define MAX_NAME		40
#define NO_PERIODS		2
#define DEFAULT_DEVNAME		"default"
#define DEFAULT_PCM_NUM		0
#define	DEFAULT_PCM_NAME	"/dev/snd/pcmC0D0p"
#define DEFAULT_MIXER_NAME	"/dev/snd/controlC0"
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
static _aaxDriverThread _aaxALSADriverThread;
static _aaxDriverState _aaxALSADriverState;
static _aaxDriverParam _aaxALSADriverParam;
static _aaxDriverLog _aaxALSADriverLog;

static char _alsa_id_str[MAX_ID_STRLEN] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxALSADriverBackend =
{
   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_alsa_id_str,

   (_aaxDriverRingBufferCreate *)&_aaxRingBufferCreate,
   (_aaxDriverRingBufferDestroy *)&_aaxRingBufferFree,

   (_aaxDriverDetect *)&_aaxALSADriverDetect,
   (_aaxDriverNewHandle *)&_aaxALSADriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxALSADriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxALSADriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxALSADriverGetName,
   (_aaxDriverRender *)&_aaxALSADriverRender,
   (_aaxDriverThread *)&_aaxALSADriverThread,

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

   char *pcm;
   unsigned int cardnum;
   unsigned int devnum;
   int mode;
   int fd;

   float latency;
   float frequency_hz;
   unsigned int format;
   unsigned int no_tracks;
   ssize_t no_frames;
   char no_periods;
   char bits_sample;

   char use_mmap;
   char use_timer;
   char interleaved;
   char prepared;
   char pause;
   char can_pause;

   void **ptr;
   char **scratch;
   size_t buf_len;

   char *ifname[2];

   struct snd_pcm_mmap_status *status;
   struct snd_pcm_mmap_control *control;
   struct snd_pcm_sync_ptr *sync;
   void *mmap_buffer;

} _driver_t;

DECL_FUNCTION(ioctl);
DECL_FUNCTION(mmap);
DECL_FUNCTION(munmap);
DECL_FUNCTION(poll);

static int detect_cardnum(const char*);
static int detect_pcm(_driver_t*, char);
static void _init_params(struct snd_pcm_hw_params*);
static void _set_mask(struct snd_pcm_hw_params*, int, unsigned int);
static void _set_min(struct snd_pcm_hw_params*, int, unsigned int);
static unsigned int _get_int(struct snd_pcm_hw_params*, int);
static unsigned int _set_int(struct snd_pcm_hw_params*, int, unsigned int);
static unsigned int _get_minmax(struct snd_pcm_hw_params*, int, unsigned int);
static void _alsa_set_volume(_driver_t*, int32_t**, ssize_t, size_t, unsigned int, float);
static int _alsa_get_volume(_driver_t*);


static const int _mode[] = { O_RDONLY, O_WRONLY };
static const char *_const_alsa_default_name = DEFAULT_DEVNAME;
static int _alsa_default_cardnum = DEFAULT_PCM_NUM;

static int
_aaxALSADriverDetect(int mode)
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;
   char *error = 0;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);
     
   if (TEST_FOR_FALSE(rv))
   {
      audio = _aaxIsLibraryPresent(NULL, 0);
      if (audio && (access(DEFAULT_PCM_NAME, F_OK) != -1))
      {
         TIE_FUNCTION(ioctl);
         TIE_FUNCTION(mmap);
         TIE_FUNCTION(munmap);
         TIE_FUNCTION(poll);

         error = _aaxGetSymError(0);
         if (!error) {
            rv = AAX_TRUE;
         }
      }
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
      handle->pcm = DEFAULT_PCM_NAME;
      handle->name = (char*)_const_alsa_default_name;
      handle->frequency_hz = (float)48000.0f;
      handle->no_tracks = 2;
      handle->use_mmap = AAX_FALSE;
      handle->mode = _mode[(mode > 0) ? 1 : 0];
      handle->no_periods = 2;
      handle->interleaved = AAX_TRUE;
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

         if (!handle->pcm)
         {
            s = xmlAttributeGetString(xid, "name");
            if (s)
            {
               handle->cardnum = detect_cardnum(s);
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
      }

      if (renderer)
      {
         handle->cardnum = detect_cardnum(renderer);
         if (handle->name != _const_alsa_default_name) {
            free(handle->name);
         }
         handle->name = _aax_strdup(renderer);
      }
   }

   if (handle)
   {
      int m = (handle->mode == O_RDONLY) ? 0 : 1;
      if (detect_pcm(handle, m))
      {
         handle->fd = open(handle->pcm, O_RDWR);
         if (handle->fd < 0)
         {
            free(handle);
            handle = NULL;
         }
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
      int page_size = sysconf(_SC_PAGE_SIZE);

      if (handle->sync) {
         free(handle->sync);
      }
      else
      {
         if (handle->status) {
            munmap(handle->status, page_size);
         }
         if (handle->control) {
            munmap(handle->control, page_size);
         }
      }

      if (handle->name != _const_alsa_default_name) {
         free(handle->name);
      }
#if 0
      if (handle->pcm)
      {
         if (handle->pcm != _const_alsa_default_name) {
            free(handle->pcm);
         }
         handle->pcm = 0;
      }
#endif

      if (handle->render) {
         handle->render->close(handle->render->id);
      }

      close(handle->fd);
      free(handle);

      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxALSADriverSetup(const void *id, size_t *frames, int *fmt,
                   unsigned int *channels, float *speed, int *bitrate)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   if (handle->fd >= 0)
   {
      unsigned int tracks, rate, bits, periods, format;
      struct snd_pcm_hw_params hwparams;
      struct snd_pcm_sw_params swparams;
      snd_pcm_uframes_t no_frames;
      int err, fd = handle->fd;
 
      tracks = *channels;
      periods = handle->no_periods;
      no_frames = *frames / periods;
      bits = aaxGetBitsPerSample(*fmt);
      rate = (unsigned int)*speed;

      _init_params(&hwparams);
      hwparams.flags |= SNDRV_PCM_HW_PARAMS_NORESAMPLE;
      if (handle->use_timer) {
          hwparams.flags |= SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP;
      }

      if (pioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &hwparams) >= 0)
      {
         rate = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_RATE, rate);
         bits = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, bits);
         tracks = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS, tracks);
         periods = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS, periods);

         no_frames = *frames / periods;
         no_frames = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
                                            no_frames);
      }

      switch (bits)
      {
      case 8:
         format = SND_PCM_FORMAT_S8;
         *fmt = AAX_PCM8S;
         break;
      case 24:
         format = SND_PCM_FORMAT_S24_LE;
         *fmt = AAX_PCM24S;
         break;
      case 32:
         format = SND_PCM_FORMAT_S32_LE;
         *fmt = AAX_PCM32S;
         break;
      case 16:
      default:
         format = SND_PCM_FORMAT_S16_LE;
         *fmt = AAX_PCM16S;
      }
      _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_FORMAT, format);
      _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                           SNDRV_PCM_SUBFORMAT_STD);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_RATE, rate);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS, tracks);
      _set_min(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, no_frames);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, bits);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_FRAME_BITS, bits*tracks);
      _set_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS, periods);

      if (handle->use_mmap) {
         _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_ACCESS,
                              SND_PCM_ACCESS_MMAP_INTERLEAVED);
      } else {
         _set_mask(&hwparams, SNDRV_PCM_HW_PARAM_ACCESS,
                              SND_PCM_ACCESS_RW_INTERLEAVED);
      }

      err = pioctl(handle->fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hwparams);
      if (err >= 0)
      {
         if (hwparams.info & SNDRV_PCM_INFO_PAUSE) {
            handle->can_pause = AAX_TRUE;
         }

         memset(&swparams, 0, sizeof(struct snd_pcm_sw_params));
         swparams.tstamp_mode = SNDRV_PCM_TSTAMP_ENABLE;
         swparams.period_step = 1;
         swparams.avail_min = 1;
         swparams.silence_size = 0;
         swparams.silence_threshold = periods*no_frames;
         swparams.boundary = periods*no_frames;
         swparams.xfer_align = no_frames/2;
         if (handle->mode == O_RDONLY)
         {
            swparams.start_threshold = 1;
            swparams.stop_threshold = 10*periods*no_frames;
         }
         else
         {
            swparams.start_threshold = no_frames;
            swparams.stop_threshold = periods*no_frames;
         }
         err = pioctl(handle->fd, SNDRV_PCM_IOCTL_SW_PARAMS, &swparams);
         if (err >= 0)
         {
            int page_size = sysconf(_SC_PAGE_SIZE);
            handle->status = pmmap(NULL, page_size, PROT_READ,
                                         MAP_FILE|MAP_SHARED, handle->fd,
                                         SNDRV_PCM_MMAP_OFFSET_STATUS);
            if (handle->status != MAP_FAILED)
            {
               handle->control = pmmap(NULL, page_size, PROT_READ|PROT_WRITE,
                                       MAP_FILE|MAP_SHARED, handle->fd,
                                       SNDRV_PCM_MMAP_OFFSET_CONTROL);
               if (handle->control == MAP_FAILED)
               {
                  pmunmap(handle->status, page_size);
                  handle->status = NULL;
               }
            }
            else {
               handle->status = NULL;
            }

            if (handle->status == NULL)
            {
               handle->sync = calloc(1, sizeof(struct snd_pcm_sync_ptr));
               if (handle->sync)
               {
                  handle->status = &handle->sync->s.status;
                  handle->control = &handle->sync->c.control;
                  handle->sync->flags = 0;
                  pioctl(handle->fd, SNDRV_PCM_IOCTL_SYNC_PTR, handle->sync);
               }
            }

            if (handle->status)
            {
               handle->control->avail_min = 1;
               _alsa_get_volume(handle);

               rate = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_RATE);
               tracks = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS);
               periods = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS);
               no_frames=_get_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);

               handle->frequency_hz = rate;
               handle->no_tracks = tracks;
               handle->no_periods = periods;
               handle->no_frames = no_frames;
               handle->bits_sample = bits;
               handle->latency = (float)(no_frames*periods)/(float)rate;

               *speed = rate;
               *channels = tracks;
               *frames = no_frames * periods;

               handle->render = _aaxSoftwareInitRenderer(handle->latency);
               if (handle->render)
               {
                  const char *rstr = handle->render->info(handle->render->id);
#if HAVE_SYS_UTSNAME_H
                  struct utsname utsname;
                  uname(&utsname);
                  snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s %s",
                                       DEFAULT_RENDERER, utsname.release, rstr);
#else
                  snprintf(_alsa_id_str, MAX_ID_STRLEN, "%s %s",
                                            DEFAULT_RENDERER, os_name, rstr);
#endif
                  rv = AAX_TRUE;
               }
            }
         }
      }
   }

   do
   {
      char str[255];

      _AAX_SYSLOG("driver settings:");

      if (handle->mode != 0) {
         snprintf(str,255,"  output renderer: '%s'", handle->name);
      } else {
         snprintf(str,255,"  input renderer: '%s'", handle->name);
      }
      _AAX_SYSLOG(str);
      snprintf(str,255, "  devname: '%s'", handle->pcm);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  playback rate: %5.0f hz", handle->frequency_hz);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  buffer size: %u bytes", handle->no_frames*handle->no_tracks*handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  latency: %3.2f ms",  1e3*handle->latency);
      _AAX_SYSLOG(str);
      snprintf(str,255, "  no. periods: %i", handle->no_periods);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  use mmap: %s", handle->use_mmap?"yes":"no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  interleaved: %s",handle->interleaved?"yes":"no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  timer based: %s",handle->use_timer?"yes":"no");
      _AAX_SYSLOG(str);
      snprintf(str,255,"  channels: %i, bytes/sample: %i\n", handle->no_tracks, handle->bits_sample/8);
      _AAX_SYSLOG(str);
      snprintf(str,255,"  can pause: %i\n", handle->can_pause);
      _AAX_SYSLOG(str);

#if 1
 printf("driver settings:\n");
 if (handle->mode != O_RDONLY) {
    printf("  output renderer: '%s'\n", handle->name);
 } else {
    printf("  input renderer: '%s'\n", handle->name);
 }
 printf("  pcm: '%s', card: %i, device: %i\n", handle->pcm, handle->cardnum, handle->devnum);
 printf("  playback rate: %5.0f Hz\n", handle->frequency_hz);
 printf("  buffer size: %u bytes\n", handle->no_frames*handle->no_tracks*handle->bits_sample/8);
 printf("  latency:  %5.2f ms\n", 1e3*handle->latency);
 printf("  no_periods: %i\n", handle->no_periods);
 printf("  use mmap: %s\n", handle->use_mmap?"yes":"no");
 printf("  interleaved: %s\n",handle->interleaved?"yes":"no");
 printf("  timer based: %s\n",handle->use_timer?"yes":"no");
 printf("  channels: %i, bytes/sample: %i\n", handle->no_tracks, handle->bits_sample/8);
 printf("  can pause: %i\n", handle->can_pause);
#endif
   }
   while (0);

   return rv;
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
      size_t res, no_frames;
      struct snd_xferi x;

      no_frames = *frames;

      x.buf = scratch;
      x.frames = no_frames;
      res = pioctl(handle->fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x);
      if (res >= 0)
      {
         int32_t **sbuf = (int32_t**)data;

         res /= frame_size;

         _batch_cvt24_16_intl(sbuf, scratch, offs, tracks, res);
         _alsa_set_volume(handle, sbuf, offs, res, tracks, gain);
         *frames = x.frames;

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
   snd_pcm_sframes_t avail, bufsize;
   unsigned int no_tracks;
   int res, count;
   int32_t **sbuf;
   char *data;

   assert(rb);
   assert(id != 0);

   if (handle->mode != O_WRONLY)
      return 0;

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;

   outbuf_size = no_tracks *no_samples*handle->bits_sample/8;
   if (handle->ptr == 0 || (handle->buf_len < outbuf_size))
   {
      char *p = 0;

      _aax_free(handle->ptr);
      handle->buf_len = outbuf_size;

      handle->ptr = (void**)_aax_malloc(&p, outbuf_size);
      handle->scratch = (char**)p;
   }

   data = (char*)handle->scratch;

   sbuf = (int32_t**)rb->get_tracks_ptr(rb, RB_READ);
   _alsa_set_volume(handle, sbuf, offs, no_samples, no_tracks, gain);

   _batch_cvt16_intl_24(data, (const int32_t**)sbuf, offs, no_tracks, no_samples);
   rb->release_tracks_ptr(rb);

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)data, no_tracks*no_samples);
   }

   res = 0;
   count = 16;
   do
   {
      if (!handle->prepared)
      {
         res = pioctl(handle->fd, SNDRV_PCM_IOCTL_PREPARE);
         if (res >= 0) {
            handle->prepared = AAX_TRUE;
         }
      }
      if (handle->sync)
      {
         handle->sync->flags = 0;
         res = pioctl(handle->fd, SNDRV_PCM_IOCTL_SYNC_PTR, handle->sync);
      }

      bufsize = handle->no_periods*handle->no_frames;
      avail = handle->status->hw_ptr + bufsize;
      avail -= handle->control->appl_ptr;
      if (avail < 0) {
         avail +=  bufsize;
      } else if (avail > bufsize) {
         avail -= bufsize;
      }

      if (handle->prepared)
      {
         struct snd_xferi xfer;

         xfer.buf = data;
         xfer.frames = no_samples;
         res = pioctl(handle->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xfer);
         if (res < 0) {
            handle->prepared = AAX_FALSE;
         }
      }
   }
   while ((res < 0) && --count);

   if (res < 0) {
      _AAX_SYSLOG("alsa: warning: pcm write error");
   }

   return 0;
}

static char *
_aaxALSADriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && handle->name)
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
      if (handle && !handle->pause)
      {
         if (handle->can_pause)
         {
            int err = pioctl(handle->fd, SNDRV_PCM_IOCTL_PAUSE, 1);
            if (err >= 0) {
               rv = AAX_TRUE;
            }
         }
         handle->pause = 1;
      }
      break;
   case DRIVER_RESUME:
      if (handle && handle->pause) 
      {
         if (handle->can_pause)
         {
            int err = pioctl(handle->fd, SNDRV_PCM_IOCTL_PAUSE, 0);
            if (err >= 0) {
               rv = AAX_TRUE;
            }
         }
         handle->pause = 0;
      }
      break;
   case DRIVER_AVAILABLE:
#if 0
      if (handle)
      {
         int state;
         int err = pioctl(handle->fd, SNDRV_CTL_IOCTL_POWER_STATE, &state);
         if ((err >= 0) && (state != 0)) {
            rv = AAX_TRUE;
         }
      }
#else
rv = AAX_TRUE;
#endif
      break;
   case DRIVER_SHARED_MIXER:
      rv = AAX_FALSE;
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
         rv = 1.0f; // handle->hwgain;
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
//    _driver_t *handle = (_driver_t*)id;
      int fd, len = 1024;
      int card = 0;
      char *ptr;

      t_previous[m] = t_now;

      ptr = (char *)&names[m];
      *ptr = 0; *(ptr+1) = 0;
      do
      {
         char fn[256];

         snprintf(fn, 256, "/dev/snd/controlC%u", card++);
         fd = open(fn, O_RDONLY);
         if (fd >= 0)
         {
            struct snd_ctl_card_info card_info;
            if (pioctl(fd, SNDRV_CTL_IOCTL_CARD_INFO, &card_info) >= 0)
            {
               int slen;

               snprintf(ptr, len, "%s", card_info.name);
               close(fd);

               slen = strlen(ptr)+1;      /* skip the trailing 0 */
               if (slen > (len-1)) break;

               len -= slen;
               ptr += slen;
            }
         }
      }
      while (fd >= 0);

      /* always end with "\0\0" no matter what */
      names[m][1022] = 0;
      names[m][1023] = 0;
   }

   return (char *)&names[mode];
}

static char *
_aaxALSADriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char *rv = handle->ifname[m];

   if (!rv)
   {
      char devlist[1024] = "\0\0";
      int card, device, fd;
      size_t len = 1024;
      char *ptr;

      card = detect_cardnum(devname);
      ptr = devlist;
      device = 0;
      do
      {
         char fn[256];
         snprintf(fn, 256, "/dev/snd/pcmC%uD%u%c", card, device, m ? 'p' : 'c');

         fd = open(fn, O_RDONLY);
         if (fd >= 0)
         {
            struct snd_pcm_info info;
            const char *name;
            int slen;

            ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info);

            name = (const char*)info.name;
            snprintf(ptr, len, "%s", name);
            close(fd);

            slen = strlen(ptr)+1; /* skip the trailing 0 */
            if (slen > (len-1)) break;

            len -= slen;
            ptr += slen;
         }
         device++;
      }
      while (fd >= 0);

      if (ptr != devlist)
      {
         *ptr++ = '\0';
         if (handle->ifname[m]) {
            rv = realloc(handle->ifname[m], ptr-devlist);
         } else {
            rv = malloc(ptr-devlist);
         }
         if (rv)
         {
            handle->ifname[m] = rv;
            memcpy(handle->ifname[m], devlist, ptr-devlist);
         }
      }
   }

   return rv;
}

static char *
_aaxALSADriverLog(const void *id, int prio, int type, const char *str)
{
   static char _errstr[256];

   snprintf(_errstr, 256, "alsa: %s\n", str);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
}

static int
_alsa_get_volume(_driver_t *handle)
{
   int rv = 0;
   return rv;
}

static void
_alsa_set_volume(_driver_t *handle, int32_t **sbuf, ssize_t offset, size_t no_frames, unsigned int no_tracks, float volume)
{
}


/* -------------------------------------------------------------------------- */

static void
_init_params(struct snd_pcm_hw_params *params)
{
   if (params)
   {
      int n;

      memset(params, 0, sizeof(struct snd_pcm_hw_params));

      for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
           n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++)
      {
         struct snd_mask *m = &params->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK];
         m->bits[0] = ~0;
         m->bits[1] = ~0;
      }

      for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
           n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++)
      {
         struct snd_interval *i;

         i = &params->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
         i->max = ~0;
      }
      params->rmask = ~0U;
      params->info = ~0U;
   }
}

static unsigned int
_get_minmax(struct snd_pcm_hw_params *params, int param, unsigned int value)
{
   int rv = value;
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      rv = _MINMAX(value, itv->min, itv->max);
   }
   return rv;
}

static unsigned int
_set_int(struct snd_pcm_hw_params *params, int param, unsigned int value)
{
   int rv = value;
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      itv->min = value;
      itv->max = value;
      itv->integer = 1;
   }
   return rv;
}

static unsigned int
_get_int(struct snd_pcm_hw_params *params, int param)
{
   int rv = 0;
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      if (itv->integer) {
         rv = itv->max;
      }
   }
   return rv;
}

static void
_set_min(struct snd_pcm_hw_params *params, int param, unsigned int value)
{
   if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL &&
       param <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL)
   {
      struct snd_interval *itv;

      itv = &params->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
      itv->min = value;
   }
}

static void
_set_mask(struct snd_pcm_hw_params *params, int param, unsigned int bit)
{
   if ((bit < SNDRV_MASK_MAX) &&
       (param >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
       (param <= SNDRV_PCM_HW_PARAM_LAST_MASK))
   {
      struct snd_mask *mask;

      mask = &params->masks[param - SNDRV_PCM_HW_PARAM_FIRST_MASK];
      mask->bits[0] = mask->bits[1] = 0;
      mask->bits[bit >> 5] |= (1 << (bit & 31));
   }
}

static int
detect_pcm(_driver_t *handle, char m)
{
   const char *devname = handle->name;
   int rv = AAX_FALSE;

   if (devname && strcasecmp(devname, "default"))
   {
      int fd, card = 0, device = 0;
      char *ifname;
      char fn[256];

      ifname = strstr(devname, ": ");
      if (ifname)
      {
         *ifname = 0;
         ifname += 2;

         card = detect_cardnum(devname);
         device = 0;
         do
         {
            snprintf(fn, 256, "/dev/snd/pcmC%uD%u%c", card, device, m ? 'p' : 'c');

            fd = open(fn, O_RDONLY);
            if (fd >= 0)
            {
               struct snd_pcm_info info;
               const char *name;
               int found;

               ioctl(fd, SNDRV_PCM_IOCTL_INFO, &info);

               name = (const char*)info.name;
               found = !strcasecmp(ifname, name) ? AAX_TRUE : AAX_FALSE;
               close(fd);

               if (found) break;
            }
            device++;
         }
         while (fd >= 0);

         *ifname-- = ' ';
         *ifname = ':';
      }

      handle->cardnum = card;
      handle->devnum = device;
      handle->pcm = _aax_strdup(fn);
      rv = AAX_TRUE;
   }
   else
   {
      handle->pcm = DEFAULT_PCM_NAME;
      handle->cardnum = 0;
      handle->devnum = 0;
      rv = AAX_TRUE;
   }

   return rv;
}

static int
detect_cardnum(const char *devname)
{
   int rv = _alsa_default_cardnum;

   if (!strncasecmp(devname, "/dev/snd/pcmC", 13) ) {
       rv = atoi(devname+13);
   }
   else if (devname && strcasecmp(devname, "ALSA") &&
                       strcasecmp(devname, "default"))
   {
      int fd, card = -1;
      do
      {
         char fn[256];

         snprintf(fn, sizeof(fn), "/dev/snd/controlC%u", ++card);
         fd = open(fn, O_RDONLY);
         if (fd >= 0)
         {
            struct snd_ctl_card_info card_info;
            if (pioctl(fd, SNDRV_CTL_IOCTL_CARD_INFO, &card_info) >= 0)
            {
               const char *name = (const char*)card_info.name;
               int res = strcasecmp(devname, name);
               close(fd);
   
               if (!res) 
               {
                  rv = card;
                  break;
               }
            }
         }
      }
      while (fd >= 0);
   }

   return rv;
}

void *
_aaxALSADriverThread(void* config)
{
   _handle_t *handle = (_handle_t *)config;
   float delay_sec, wait_us;
   _intBufferData *dptr_sensor;
   snd_pcm_sframes_t no_samples;
   const _aaxDriverBackend *be;
   _aaxRingBuffer *dest_rb;
   _aaxAudioFrame *mixer;
   const void *id;
   int stdby_time;
   char state;

   if (!handle || !handle->sensors || !handle->backend.ptr
       || !handle->info->no_tracks) {
      return NULL;
   }

   delay_sec = 1.0f/handle->info->refresh_rate;

   be = handle->backend.ptr;
   id = handle->backend.handle;		// Required for _AAX_DRVLOG

   dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor)
   {
      _sensor_t* sensor = _intBufGetDataPtr(dptr_sensor);

      mixer = sensor->mixer;

      dest_rb = be->get_ringbuffer(REVERB_EFFECTS_TIME, mixer->info->mode);
      if (dest_rb)
      {
         dest_rb->set_format(dest_rb, AAX_PCM24S, AAX_TRUE);
         dest_rb->set_parami(dest_rb, RB_NO_TRACKS, mixer->info->no_tracks);
         dest_rb->set_paramf(dest_rb, RB_FREQUENCY, mixer->info->frequency);
         dest_rb->set_paramf(dest_rb, RB_DURATION_SEC, delay_sec);
         dest_rb->init(dest_rb, AAX_TRUE);
         dest_rb->set_state(dest_rb, RB_STARTED);

         handle->ringbuffer = dest_rb;
      }
      _intBufReleaseData(dptr_sensor, _AAX_SENSOR);

      if (!dest_rb) {
         return NULL;
      }
   }
   else {
      return NULL;
   }

   be->state(handle->backend.handle, DRIVER_PAUSE);
   state = AAX_SUSPENDED;

   wait_us = delay_sec*1000000.0f;
   no_samples = dest_rb->get_parami(dest_rb, RB_NO_SAMPLES);
   stdby_time = (int)(delay_sec*1000);
   _aaxMutexLock(handle->thread.signal.mutex);
   while TEST_FOR_TRUE(handle->thread.started)
   {
      _driver_t *be_handle = (_driver_t *)handle->backend.handle;
      snd_pcm_sframes_t avail, bufsize;
      int res;

      _aaxMutexUnLock(handle->thread.signal.mutex);

      if (be_handle->sync)
      {
         be_handle->sync->flags = SNDRV_PCM_SYNC_PTR_HWSYNC;
         pioctl(be_handle->fd, SNDRV_PCM_IOCTL_SYNC_PTR, be_handle->sync);
      }

      bufsize = be_handle->no_periods*be_handle->no_frames;
      avail = be_handle->status->hw_ptr + bufsize;
      avail -= be_handle->control->appl_ptr;
      if (avail < 0) {
         avail +=  bufsize;
      } else if (avail > bufsize) {
         avail -= bufsize;
      }

      if (avail < no_samples)
      {
         if (_IS_PLAYING(handle))
         {
            // TIMER_BASED
            if (be_handle->use_timer) {
               usecSleep(wait_us);
            }
				/* timeout is in ms */
            else
            {
               struct pollfd pfd;
               pfd.fd = be_handle->fd;
               pfd.events = POLLOUT|POLLERR|POLLNVAL;
               do
               {
                  errno = 0;
                  res = ppoll(&pfd, 1, 2*stdby_time);
                  if (res <= 0) break;
                  if (errno == -EINTR) continue;
                  if (pfd.revents & (POLLERR|POLLNVAL))
                  {
                     _AAX_DRVLOG("snd_pcm_wait polling error");
                     switch(be_handle->status->state)
                     {
                     case SND_PCM_STATE_XRUN:
                         be_handle->prepared = AAX_FALSE;
                         break;
                      case SND_PCM_STATE_SUSPENDED:
                      case SND_PCM_STATE_DISCONNECTED:
                         break;
                      default:
                         break;
                     }
                  }
               }
               while (!(pfd.revents & (POLLIN|POLLOUT)));
            }
         }
         else {
            msecSleep(stdby_time);
         }
      }
      _aaxMutexLock(handle->thread.signal.mutex);
      if TEST_FOR_FALSE(handle->thread.started) {
         break;
      }

      if (be->state(be_handle, DRIVER_AVAILABLE) == AAX_FALSE) {
         _SET_PROCESSED(handle);
      }

      if (state != handle->state)
      {
         if (_IS_PAUSED(handle) ||
             (!_IS_PLAYING(handle) && _IS_STANDBY(handle))) {
            be->state(handle->backend.handle, DRIVER_PAUSE);
         }
         else if (_IS_PLAYING(handle) || _IS_STANDBY(handle)) {
            be->state(handle->backend.handle, DRIVER_RESUME);
         }
         state = handle->state;
      }

      if (_IS_PLAYING(handle)) {
         _aaxSoftwareMixerThreadUpdate(handle, dest_rb);
      }
#if 0
 printf("state: %i, paused: %i\n", state, _IS_PAUSED(handle));
 printf("playing: %i, standby: %i\n", _IS_PLAYING(handle), _IS_STANDBY(handle));
#endif
   }

   handle->ringbuffer = NULL;
   be->destroy_ringbuffer(dest_rb);
   _aaxMutexUnLock(handle->thread.signal.mutex);

   return handle;
}
