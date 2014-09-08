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

   char *pcm;
   unsigned int cardnum;
   unsigned int devnum;
   int mode;
   int fd;

   float frequency_hz;
   unsigned int format;
   unsigned int no_tracks;
   ssize_t no_frames;
   char no_periods;
   char bits_sample;

   char use_mmap;
   char use_timer;
   char exclusive;
   char interleaved;

   void **ptr;
   char **scratch;
   size_t buf_len;

   char *ifname[2];

} _driver_t;

DECL_FUNCTION(ioctl);
// DECL_FUNCTION(poll);

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

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);
     
   if (TEST_FOR_FALSE(rv)) {
      audio = _aaxIsLibraryPresent(NULL, 0);
      if (audio)
      {
         TIE_FUNCTION(ioctl);
//         TIE_FUNCTION(poll);
      }
   }

   /* card 0, device 0 should always be available */
   if (audio && (access(DEFAULT_PCM_NAME, F_OK) != -1)) {
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
      handle->pcm = DEFAULT_PCM_NAME;
      handle->name = (char*)_const_alsa_default_name;
      handle->frequency_hz = (float)48000.0f;
      handle->no_tracks = 2;
      handle->use_mmap = AAX_FALSE;
      handle->mode = _mode[(mode > 0) ? 1 : 0];
      handle->exclusive = 0; // O_EXCL;
      handle->no_periods = 2;
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

         if (xmlNodeGetBool(xid, "shared")) {
            handle->exclusive = 0;
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
         handle->fd = open(handle->pcm, O_RDWR); // handle->mode|handle->exclusive);
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
                   unsigned int *channels, float *speed, int *bitrate)
{
   _driver_t *handle = (_driver_t *)id;
   int err = 0;

   if (handle->fd >= 0)
   {
      struct snd_pcm_hw_params hwparams;
      struct snd_pcm_sw_params swparams;
      int fd = handle->fd;

      _init_params(&hwparams);
      if (pioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &hwparams) >= 0)
      {
         unsigned int tracks, rate, bits, periods, format;
         snd_pcm_uframes_t no_frames;

         rate = (unsigned int)*speed;
         rate = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_RATE, rate);

         tracks = *channels;
         tracks = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS, tracks);

         no_frames = *frames;
         no_frames = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
                                            no_frames);
         periods = handle->no_periods;
         periods = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS, periods);

         bits = aaxGetBitsPerSample(*fmt);
         bits = _get_minmax(&hwparams, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, bits);

         switch (*fmt)
         {
         case AAX_PCM8S:
            format = SND_PCM_FORMAT_S8;
            break;
         case AAX_PCM24S:
            format = SND_PCM_FORMAT_S24_LE;
            break;
         case AAX_PCM32S:
            format = SND_PCM_FORMAT_S32_LE;
            break;
         case AAX_PCM16S:
         default:
            format = SND_PCM_FORMAT_S16_LE;
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

         if (handle->use_timer) {
             hwparams.flags |= SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP;
         }

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
            *speed = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_RATE);
            *channels = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_CHANNELS);
            *frames = _get_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);

            handle->no_periods =_get_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS);
            handle->frequency_hz = *speed;
            handle->no_tracks = *channels;
            handle->no_frames = *frames;
         }
      }
      else {
         _AAX_DRVLOG("unable to get the device capabilities")  ;
      }
         
      memset(&swparams, 0, sizeof(struct snd_pcm_sw_params));
      swparams.tstamp_mode = SNDRV_PCM_TSTAMP_ENABLE;
      swparams.period_step = 1;
      swparams.avail_min = 1;
      swparams.silence_size = 0;
      swparams.boundary = handle->buf_len;
      swparams.xfer_align = handle->no_periods/2;
      swparams.silence_threshold = handle->no_periods*handle->no_frames;
      if (handle->mode == O_RDONLY)
      {
         swparams.start_threshold = 1;
         swparams.stop_threshold = 10*handle->no_periods*handle->no_frames;
      }
      else
      {
         swparams.start_threshold = handle->no_periods*handle->no_frames;
         swparams.stop_threshold = handle->no_periods*handle->no_frames;
      }
      err = pioctl(handle->fd, SNDRV_PCM_IOCTL_SW_PARAMS, &swparams);

      _alsa_get_volume(handle);
   }
#if 0
 printf("driver settings:\n");
 if (handle->mode != O_RDONLY) {
    printf("  output renderer: '%s'\n", handle->name);
 } else {
    printf("  input renderer: '%s'\n", handle->name);
 }
 printf("  pcm: '%s', card: %i, device: %i\n", handle->pcm, handle->cardnum, handle->devnum);
 printf("  frequency: %f\n", handle->frequency_hz);
 printf("  no_tracks: %i\n", handle->no_tracks);
 printf("  no_frames: %zi\n", handle->no_frames);
 printf("  no_periods: %i\n", handle->no_periods);
#endif

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
 ndle->no_periods =_get_int(&hwparams, SNDRV_PCM_HW_PARAM_PERIODS);  {
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
   unsigned int no_tracks;
   struct snd_xferi xfer;
   int32_t **sbuf;
   char *data;
   size_t res;

   assert(rb);
   assert(id != 0);

   if (handle->mode == O_WRONLY)
      return 0;

   offs = rb->get_parami(rb, RB_OFFSET_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES) - offs;

   outbuf_size = no_tracks *no_samples*sizeof(int16_t);
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

   xfer.buf = data;
   xfer.frames = no_samples;
   res = pioctl(handle->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xfer);
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
         handle->fd = open(handle->pcm, handle->mode|handle->exclusive);
         if (handle->fd)
         {
            int err = 0; // , fd = handle->fd;
            if (err >= 0) {
               rv = AAX_TRUE;
            }
         }
      }
      break;
   case DRIVER_AVAILABLE:
      rv = AAX_TRUE;
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
         rv = 0.0f; // handle->latency;
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

   if (devname)
   {
      int fd, card, device;
      char *ifname;
      char fn[256];

      ifname = strstr(devname, ": ");
      if (ifname)
      {
         *ifname = 0;
         ifname += 2;
      }

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

      if (fd >= 0)
      {
         handle->cardnum = card;
         handle->devnum = device;
         handle->pcm = _aax_strdup(fn);
         rv = AAX_TRUE;
      }

      if (ifname)
      {
         *ifname-- = ' ';
         *ifname = ':';
      }
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

