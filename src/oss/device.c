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

#include <api.h>
#include <arch.h>
#include <driver.h>
#include <devices.h>
#include <ringbuffer.h>
#include <base/types.h>
#include <base/logging.h>
#include <base/dlsym.h>

#include "audio.h"
#include "device.h"

#define MAX_NAME		40
#define NO_FRAGMENTS		2
#define DEFAULT_DEVNUM		0
#define	DEFAULT_DEVNAME		"/dev/dsp0"
#define DEFAULT_MIXER		"/dev/mixer0"
#define DEFAULT_RENDERER	"OSS"
#define OSS_VERSION_4		0x040002

#define _AAX_DRVLOG(a)		_aaxOSSDriverLog(a)
#define HW_VOLUME_SUPPORT(a)	((a->mixfd >= 0) && (a->_volume >= 0))

static _aaxDriverDetect _aaxOSSDriverDetect;
static _aaxDriverNewHandle _aaxOSSDriverNewHandle;
static _aaxDriverGetDevices _aaxOSSDriverGetDevices;
static _aaxDriverGetInterfaces _aaxOSSDriverGetInterfaces;
static _aaxDriverConnect _aaxOSSDriverConnect;
static _aaxDriverDisconnect _aaxOSSDriverDisconnect;
static _aaxDriverSetup _aaxOSSDriverSetup;
static _aaxDriverCaptureCallback _aaxOSSDriverCapture;
static _aaxDriverCallback _aaxOSSDriverPlayback;
static _aaxDriverGetName _aaxOSSDriverGetName;
static _aaxDriver3dMixerCB _aaxOSSDriver3dMixer;
static _aaxDriverState _aaxOSSDriverState;
static _aaxDriverParam _aaxOSSDriverParam;
static _aaxDriverLog _aaxOSSDriverLog;

char _oss_default_renderer[100] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxOSSDriverBackend =
{
   1.0,
   AAX_PCM16S,
   48000,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_oss_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxOSSDriverDetect,
   (_aaxDriverNewHandle *)&_aaxOSSDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxOSSDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxOSSDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxOSSDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxOSSDriverConnect,
   (_aaxDriverDisconnect *)&_aaxOSSDriverDisconnect,
   (_aaxDriverSetup *)&_aaxOSSDriverSetup,
   (_aaxDriverCaptureCallback *)&_aaxOSSDriverCapture,
   (_aaxDriverCallback *)&_aaxOSSDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxOSSDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxOSSDriverState,
   (_aaxDriverParam *)&_aaxOSSDriverParam,
   (_aaxDriverLog *)&_aaxOSSDriverLog
};

typedef struct
{
   char *name;
   char *devnode;
   char *ifname[2];
   int nodenum;

   int fd;
   float latency;
   float frequency_hz;
   unsigned int format;
   unsigned int no_tracks;
   unsigned int buffer_size;

   int mode;
   int oss_version;
   int exclusive;
   char sse_level;
   char bytes_sample;

   int16_t *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

   /* initial values, reset them wehn exiting */
   int mixfd;
   int _volume;

   _oalRingBufferMix1NFunc *mix_mono3d;

} _driver_t;

DECL_FUNCTION(ioctl);

static int get_oss_version();
static int detect_devnode(_driver_t*, char);
static int detect_nodenum(const char *);
static int _oss_get_volume(_driver_t *);
static int _oss_set_volume(_driver_t*, const int32_t**, int, unsigned int, unsigned int, float);

static const int _mode[] = { O_RDONLY, O_WRONLY };
static const char *_const_oss_default_name = DEFAULT_DEVNAME;
static int _oss_default_nodenum = DEFAULT_DEVNUM;
static char *_default_mixer = DEFAULT_MIXER;

static int
_aaxOSSDriverDetect(int mode)
{
   static void *audio = NULL;
   static int rv = AAX_FALSE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);
     
   if (TEST_FOR_FALSE(rv)) {
      audio = _oalIsLibraryPresent(NULL, 0);
      if (audio) {
         TIE_FUNCTION(ioctl);
      }
   }

   if (audio && (get_oss_version() > 0)) {
      rv = AAX_TRUE;
   }

   return rv;
}

static void *
_aaxOSSDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (handle)
   {
      handle->name = (char*)_const_oss_default_name;
      handle->sse_level = _aaxGetSSELevel();
      handle->frequency_hz = (float)_aaxOSSDriverBackend.rate;
      handle->no_tracks = _aaxOSSDriverBackend.tracks;
      handle->mode = _mode[(mode > 0) ? 1 : 0];
      handle->mix_mono3d = _oalRingBufferMixMonoGetRenderer(mode);
      handle->exclusive = O_EXCL;
   }

   return handle;
}


static void *
_aaxOSSDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxOSSDriverNewHandle(mode);
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
            s = xmlNodeGetString(xid, "renderer");
            if (s)
            {
               handle->nodenum = detect_nodenum(s);
               if (handle->name != _const_oss_default_name) {
                  free(handle->name);
               }
               handle->name = _aax_strdup(s);
            }
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("oss; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("oss; frequency too large.");
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
                  _AAX_SYSLOG("oss; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("oss; no. tracks too great.");
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
               _AAX_SYSLOG("oss; unsopported bits-per-sample");
               i = 16;
            }
         }

         if (xmlNodeGetBool(xid, "virtual-mixer") ||
             xmlNodeGetBool(xid, "shared")) {
            handle->exclusive = 0;
         }
      }

      if (renderer)
      {
         handle->nodenum = detect_nodenum(renderer);
         if (handle->name != _const_oss_default_name) {
            free(handle->name);
         }
         handle->name = _aax_strdup(renderer);
      }
#if 0
 printf("frequency-hz: %f\n", handle->frequency_hz);
 printf("channels: %i\n", handle->no_tracks);
 printf("device number: %i\n", handle->nodenum);
#endif
   }


   if (handle)
   {
      int fd, m = handle->mode;

      detect_devnode(handle, m);
      fd = open(handle->devnode, handle->mode|handle->exclusive);
      if (fd)
      {
         const char *hwstr = _aaxGetSIMDSupportString();
         int version = get_oss_version();
         char *os_name = "";
#if HAVE_SYS_UTSNAME_H
         struct utsname utsname;

         uname(&utsname);
         os_name = utsname.sysname;
#endif
         snprintf(_oss_default_renderer, 99,"%s %x.%x.%x %s %s",
                   DEFAULT_RENDERER,(version>>16), (version>>8 & 0xFF),
                   (version & 0xFF), os_name, hwstr);

         if (version > 0) handle->oss_version = version;
         handle->fd = fd;

         /* test for /dev/mixer0 */
         handle->mixfd = open(_default_mixer, O_RDWR);
         if (handle->mixfd < 0)	/* test for /dev/mixer instead */
         {
            char *mixer = _aax_strdup(_default_mixer);

            *(mixer+strlen(mixer)-1) = '\0';
            handle->mixfd = open(mixer, O_WRONLY);
            free(mixer);
         }
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
_aaxOSSDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   if (handle)
   {
      free(handle->ifname[0]);
      free(handle->ifname[1]);

      if (handle->name != _const_oss_default_name) {
         free(handle->name);
      }
      if (handle->devnode)
      {
         if (handle->devnode != _const_oss_default_name) {
            free(handle->devnode);
         }
         handle->devnode = 0;
      }

      if (handle->mixfd >= 0)
      {
         _oss_set_volume(handle, NULL, 0, 0, 0, handle->_volume);
         close(handle->mixfd);
      }

      close(handle->fd);
      free(handle->ptr);
      free(handle);

      return AAX_TRUE;
   }
   return AAX_FALSE;
}

static int
_aaxOSSDriverSetup(const void *id, size_t *frames, int *fmt,
                   unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int channels, format, freq;
   int frag, no_samples = 1024;
   audio_buf_info info;
   int fd, err;

   if (*frames) {
      no_samples = *frames;
   }

   assert(handle);

   if (handle->no_tracks > *tracks) {
      handle->no_tracks = *tracks;
   }
   *tracks = handle->no_tracks;

   if (*tracks > 2)
   {
      char str[255];
      snprintf((char *)&str, 255, "oss; Unable to output to %i speakers in "
                "this setup (2 is the maximum)", *tracks);
      _AAX_SYSLOG(str);
      return AAX_FALSE;
   }

   fd = handle->fd;
   freq = (unsigned int)*speed;
   channels = *tracks; // handle->no_tracks;

   switch(*fmt)
   {
   case AAX_PCM8S:	
      format = AFMT_S8;
      break;
   case AAX_PCM16S:
      format = AFMT_S16_LE;
      break;
   default:
      _AAX_SYSLOG("oss; unsupported audio format.");
      return AAX_FALSE;
   }
   handle->bytes_sample = aaxGetBytesPerSample(*fmt);

   no_samples = (unsigned int)ceilf(no_samples);
   if (no_samples & 0xF)
   {
      no_samples |= 0xF;
      no_samples++;
   }

   frag = log2i(no_samples*channels*handle->bytes_sample);
// frag = log2i(channels*freq); // 1 second buffer
   if (frag < 4) {
      frag = 4;
   }

   frag |= NO_FRAGMENTS << 16;
   err = pioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag);

   err = pioctl(fd, SNDCTL_DSP_SETFMT, &format);
   if (err >= 0) {
      err = pioctl(fd, SNDCTL_DSP_CHANNELS, &channels);
   }
   if ((err >= 0) && (handle->mode == O_WRONLY)) {
      err = pioctl(fd, SNDCTL_DSP_GETOSPACE, &info);
   }
   if (err >= 0)
   {
      err = pioctl(fd, SNDCTL_DSP_SPEED, &freq);
      *speed = (float)freq;
   }

   /* disable sample conversion */
   if (handle->oss_version >= OSS_VERSION_4)
   {
      int enable = 0;
      err = pioctl(fd, SNDCTL_DSP_COOKEDMODE, &enable);
   }

   _oss_get_volume(handle);

   if (err >= 0)
   {
      int delay;

      handle->format = format;
      handle->no_tracks = channels;
      handle->frequency_hz = (float)freq;
      handle->buffer_size = info.fragsize;
      if (frames) *frames = info.fragsize/(channels*handle->bytes_sample);

      handle->latency = 0.0f;
      err = pioctl(fd, SNDCTL_DSP_GETODELAY, &delay);
      if (err >= 0)
      {
         handle->latency = (float)delay;
         handle->latency /= (float)(freq*channels*handle->bytes_sample);
      }
      err = 0;
   }

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}


int
_aaxOSSDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   _driver_t *handle = (_driver_t *)id;
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxOSSDriverBackend.gain;
   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr, nbuf);

   return ret;
}

static int
_aaxOSSDriverCapture(const void *id, void **data, int offs, size_t *frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   size_t buflen, frame_size;

   if (handle->mode != O_RDONLY || (frames == 0) || (data == 0))
      return AAX_FALSE;

   if (*frames == 0)
      return AAX_TRUE;

   frame_size = handle->bytes_sample * handle->no_tracks;
   buflen = *frames * frame_size;

   *frames = 0;
   if (data)
   {
      int res;

      res = read(handle->fd, scratch, buflen);
      if (res == -1)
      {
         _AAX_SYSLOG(strerror(errno));
         return AAX_FALSE;
      }
      *frames = res / frame_size;
      _batch_cvt24_16_intl((int32_t**)data, scratch, offs, 2, res);

      if (gain < 0.99f || gain > 1.01f)
      {
         if (HW_VOLUME_SUPPORT(handle))
         {
            int volume = (int)(gain * 100);

            volume |= volume<<8;
            if (handle->oss_version >= OSS_VERSION_4)
            {
               pioctl(handle->mixfd, SNDCTL_DSP_SETRECVOL, &volume);
               gain = -1.0f;
            }
            else
            {
               int devs = 0;
               pioctl(handle->fd, SOUND_MIXER_READ_RECMASK, &devs);
               if (devs & SOUND_MASK_IGAIN)
               {
                  pioctl(handle->mixfd, SOUND_MIXER_WRITE_IGAIN, &volume);
                  gain = -1.0f;
               }
            }
         }

         if (gain >= 0.0f)	/* software fallback */
         {
            int t;
            for (t=0; t<2; t++) {
               _batch_mul_value((int32_t**)data[t]+offs, sizeof(int32_t), res,
                                gain);
            }
         }
      }

      return AAX_TRUE;
   }

   return AAX_FALSE;
}

static int
_aaxOSSDriverPlayback(const void *id, void *s, float pitch, float gain)
{
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_tracks, no_samples;
   unsigned int offs, outbuf_size;
   _oalRingBufferSample *rbd;
   const int32_t** sbuf;
   audio_buf_info info;
   audio_errinfo err;
   int16_t *data;
   int res;

   assert(rb);
   assert(rb->sample);
   assert(id != 0);

   if (pioctl (handle->fd, SNDCTL_DSP_GETERROR, &err) >= 0)
   {
      if (err.play_underruns > 0)
      {
         char str[128];
         snprintf(str, 128, "oss: %d underruns\n", err.play_underruns);
         _AAX_SYSLOG(str);
      }
      if (err.rec_overruns > 0)
      {
          char str[128];
          snprintf(str, 128, "oss: %d overruns\n", err.rec_overruns);
          _AAX_SYSLOG(str);
      }
   }

   if (handle->mode == 0)
      return 0;

   rbd = rb->sample;
   sbuf = (const int32_t**)rbd->track;
   offs = _oalRingBufferGetOffsetSamples(rb);
   no_tracks = _oalRingBufferGetNoTracks(rb);
   no_samples = _oalRingBufferGetNoSamples(rb) - offs;

   if (gain < 0.99f)
   {
      if (HW_VOLUME_SUPPORT(handle))
      {
         int volume = (int)(gain * 100);

         volume |= volume<<8;
         if (handle->oss_version >= OSS_VERSION_4) 
         {
            pioctl(handle->mixfd, SNDCTL_DSP_SETPLAYVOL, &volume);
            gain = -1.0f;
         }
         else
         {
            int devs = 0;
            pioctl(handle->fd, SOUND_MIXER_READ_DEVMASK, &devs);
            if (devs & SOUND_MASK_OGAIN)
            {
               pioctl(handle->mixfd, SOUND_MIXER_WRITE_OGAIN, &volume);
               gain = -1.0f;
            }
         }
      }

      if (gain >= 0.0f)		/* software fallback */
      {
         int t;
         for (t=0; t<no_tracks; t++) {
            _batch_mul_value((void*)(sbuf[t]+offs), sizeof(int32_t), no_samples,
                             gain);
         }
      }
   }


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

   _batch_cvt16_intl_24(data, sbuf, offs, no_tracks, no_samples);

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
         snprintf(errstr, 1024, "oss: %s", strerror(errno));
         _AAX_SYSLOG(errstr);
      }
      else if (res != outbuf_size) {
         _AAX_SYSLOG("oss: warning: pcm write error");
      }
   }

   /* return the number of samples offset to the expected value */
   /* zero would be spot on                                     */
   outbuf_size = info.fragstotal*info.fragsize - outbuf_size;

   return 0; // (info.bytes-outbuf_size)/(no_tracks*sizeof(int16_t));
}

static char *
_aaxOSSDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle && handle->devnode)
      ret = _aax_strdup(handle->name);

   return ret;
}

static int
_aaxOSSDriverState(const void *id, enum _aaxDriverState state)
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

            if (handle->oss_version >= OSS_VERSION_4)
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
      if (handle && handle->oss_version >= OSS_VERSION_4)
      {
         oss_audioinfo ainfo;
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
   default:
      break;
   }

   return rv;
}

static float
_aaxOSSDriverParam(const void *id, enum _aaxDriverParam param)
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
      case DRIVER_MAX_VOLUME_DB:
         rv = _lin2db(1.0f);
         break;
      case DRIVER_MIN_VOLUME_DB:
         rv = _lin2db(0.0f);
         break;
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxOSSDriverGetDevices(const void *id, int mode)
{
   static char names[2][1024] = { "\0\0", "\0\0" };
   static time_t t_previous[2] = { 0, 0 };
   int m = (mode > 0) ? 1 : 0;
   time_t t_now;

   t_now = time(NULL);
   if (t_now > (t_previous[m]+5))
   {
      _driver_t *handle = (_driver_t*)id;
      int fd;

      t_previous[m] = t_now;

      if (handle && handle->mixfd >= 0) {
         fd = handle->mixfd;
      }
      else
      {
         fd = open(_default_mixer, O_RDWR);
         if (fd < 0)                          /* test for /dev/mixer0 instead */
         {
            char *mixer = _aax_strdup(_default_mixer);

            *(mixer+strlen(mixer)-1) = '\0';
            fd = open(mixer, O_WRONLY);
            free(mixer);
         }
      }

      if (fd >= 0)
      {
         int version = get_oss_version();

         if (version >= OSS_VERSION_4)
         {
            oss_sysinfo info;
            int err = pioctl(fd, SNDCTL_SYSINFO, &info);

            if (err >= 0)
            {
               oss_audioinfo ainfo;
	       char name[64] = "";
               int i, j, len;
               char *ptr;

               len = 1024;
               ptr = (char *)&names[mode];
               for (i = 0; i < info.numcards; i++)
               {
                  int slen;
                  char *p;

                  ainfo.dev = i;
                  err = pioctl (fd, SNDCTL_AUDIOINFO_EX, &ainfo);
                  if (err < 0) continue;

                  if (!ainfo.enabled) continue;
                  if (ainfo.pid != -1) continue;		/* in use */
                  if (ainfo.caps & PCM_CAP_VIRTUAL) continue;
                  if (((ainfo.caps & PCM_CAP_OUTPUT) && !m) ||
                      ((ainfo.caps & PCM_CAP_INPUT) && m)) continue;

                  slen = strlen(name);
                  if (slen && !strncmp(name, ainfo.name, slen)) continue;

                  strcpy(name, ainfo.name);
                  p = strstr(name, " rec");
                  if (!p) p = strstr(name, " play");
                  if (!p) p = strstr(name, " pcm");
                  if (p) *p = 0;

                  for (j=0; j<info.numcards; j++)
                  {
                     oss_card_info cinfo;

                     cinfo.card = j;
                     err = pioctl (fd, SNDCTL_CARDINFO, &cinfo);
                     if (err < 0) continue;

                     if (strstr(cinfo.longname, name))
                     {
                        snprintf(ptr, len, "%s", cinfo.longname);
                        slen = strlen(ptr)+1;	/* skip the trailing 0 */
                        if (slen > (len-1)) break;

                        len -= slen;
                        ptr += slen;
                        break;
                     }
                  }
               }
               *ptr = 0;
            }
         }

         if (!handle)
         {
            close(fd);
            fd = -1;
         }
      }
   }

   return (char *)&names[mode];
}

static char *
_aaxOSSDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   int m = (mode > 0) ? 1 : 0;
   char *rv = handle->ifname[m];

   if (!rv)
   {
      int fd = open(_default_mixer, O_RDWR);
      if (fd < 0)                          /* test for /dev/mixer0 instead */
      {
         char *mixer = _aax_strdup(_default_mixer);

         *(mixer+strlen(mixer)-1) = '\0';
         fd = open(mixer, O_WRONLY);
         free(mixer);
      }

      if (fd >= 0)
      {
         int version = get_oss_version();

         if (version >= OSS_VERSION_4)
         {
            oss_sysinfo info;
            int err = pioctl(fd, SNDCTL_SYSINFO, &info);

            if (err >= 0)
            {
               char interfaces[2048];
               unsigned int buflen;
               oss_audioinfo ainfo;
               char *ptr;
               int i = 0;

               ptr = interfaces;
               buflen = 2048;

               for (i=0; i<info.numcards; i++)
               {
                  unsigned int len;
                  char name[128];
                  char *p;

                  ainfo.dev = i;
                  err = pioctl (fd, SNDCTL_AUDIOINFO_EX, &ainfo);
                  if (err < 0) continue;

                  if (!ainfo.enabled) continue;
                  if (ainfo.pid != -1) continue;		/* in use */
                  if (ainfo.caps & PCM_CAP_VIRTUAL) continue;
                  if (((ainfo.caps & PCM_CAP_OUTPUT) && !m) ||
                      ((ainfo.caps & PCM_CAP_INPUT) && m)) continue;

                  snprintf(name, 128, "%s", ainfo.name);
                  p = strstr(name, " rec");
                  if (!p) p = strstr(name, " play");
                  if (!p) p = strstr(name, " pcm");
                  if (!p) continue;

                  *p = 0;
                  if (!strstr(devname, name)) continue;

                  *p++ = ' ';
                  snprintf(ptr, buflen, "%s", p);
                  len = strlen(ptr)+1;	/* skip the trailing 0 */
                  if (len > (buflen-1)) break;
                  buflen -= len;
                  ptr += len;
               }

               if (ptr != interfaces)
               {
                  *ptr++ = '\0';
                  rv = handle->ifname[m] = malloc(ptr-interfaces);
                  if (rv) {
                     memcpy(handle->ifname[m], interfaces, ptr-interfaces);
                  }
               }
            }
         }
      }
   }

   return rv;
}

static char *
_aaxOSSDriverLog(const char *str)
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
_oss_get_volume(_driver_t *handle)
{
   int volume = -1;

   if (handle && handle->mixfd >= 0)
   {
      if (handle->oss_version >= OSS_VERSION_4)
      {
         if (handle->mode == O_RDONLY) {
            pioctl(handle->mixfd, SNDCTL_DSP_GETRECVOL, &volume);
         } else {
            pioctl(handle->mixfd, SNDCTL_DSP_GETPLAYVOL, &volume);
         }
      }
      else
      {
         int devs = 0;
         if (handle->mode == O_RDONLY)
         {
            pioctl(handle->fd, SOUND_MIXER_READ_RECMASK, &devs);
            if (devs & SOUND_MASK_IGAIN) {
               pioctl(handle->mixfd, SOUND_MIXER_READ_IGAIN, &volume);
            }
         }
         else
         {
            pioctl(handle->fd, SOUND_MIXER_READ_DEVMASK, &devs);
            if (devs & SOUND_MASK_OGAIN) {
               pioctl(handle->mixfd, SOUND_MIXER_READ_OGAIN, &volume);
            }
         }
      }
   }

   if (handle) {
      handle->_volume = volume;
   }

   return volume;
}

static int
_oss_set_volume(_driver_t *handle, const int32_t **sbuf, int offset, unsigned int no_frames, unsigned int no_tracks, float gain)
{
   int rv = 0;

   if (handle && HW_VOLUME_SUPPORT(handle))
   {
      int volume = (int)(gain * 100);

      volume |= volume<<8;
      if (handle->oss_version >= OSS_VERSION_4)
      {
         if (handle->mode == O_RDONLY) {
            pioctl(handle->mixfd, SNDCTL_DSP_SETRECVOL, &volume);
         } else {
            pioctl(handle->mixfd, SNDCTL_DSP_SETPLAYVOL, &volume);
         }
         gain = -1.0f;
      }
      else
      {
         int devs = 0;
         if (handle->mode == O_RDONLY)
         {
            pioctl(handle->fd, SOUND_MIXER_READ_RECMASK, &devs);
            if (devs & SOUND_MASK_IGAIN)
            {
               pioctl(handle->mixfd, SOUND_MIXER_WRITE_IGAIN, &volume);
               gain = -1.0f;
            }
         }
         else
         {
            pioctl(handle->fd, SOUND_MIXER_READ_DEVMASK, &devs);
            if (devs & SOUND_MASK_OGAIN)
            {
               pioctl(handle->mixfd, SOUND_MIXER_WRITE_OGAIN, &volume);
               gain = -1.0f;
            }
         }
      }
   }

   if (gain >= 0.0f)		/* software fallback */
   {
      int t;
      for (t=0; t<2; t++) {
         _batch_mul_value((int32_t**)sbuf[t]+offset, sizeof(int32_t),
                          no_frames, gain);
      }
   }

   return rv;
}


/* -------------------------------------------------------------------------- */

static int
get_oss_version()
{
   static int version = -1;

   if (version < 0)
   {
      int fd = open(_const_oss_default_name, O_WRONLY);  /* open /dev/dsp */
      if (fd < 0)                          /* test for /dev/dsp0 instead */
      {
         char *name = _aax_strdup(_const_oss_default_name);

         *(name+strlen(name)-1) = '\0';
         fd = open(name, O_WRONLY);
         free(name);
      }
      if (fd >= 0)
      {
         int err = pioctl(fd, OSS_GETVERSION, &version);
         if (err < 0) version = -1;
         close(fd);
         fd = -1;
      }
   }
   return version;
}

static int
detect_devnode(_driver_t *handle, char mode)
{
   int version = get_oss_version();
   int rv = AAX_FALSE;

   if (version >= OSS_VERSION_4)
   {
      oss_sysinfo info;
      int err, fd = -1;

      fd = open(_default_mixer, O_RDWR);
      if (fd < 0)			/* test for /dev/mixer0 instead */
      {
         char *mixer = _aax_strdup(_default_mixer);

         *(mixer+strlen(mixer)-1) = '\0';
         fd = open(mixer, O_WRONLY);
         free(mixer);
      }

      err = pioctl(fd, SNDCTL_SYSINFO, &info);
      if (err >= 0)
      {
         oss_audioinfo ainfo;

         ainfo.dev = handle->nodenum;
         err = pioctl (fd, SNDCTL_AUDIOINFO_EX, &ainfo);
         if (err >= 0)
         {
            handle->devnode = _aax_strdup(ainfo.devnode);
            rv = AAX_TRUE;
         }
      }
   }
   else if (handle->nodenum > 0)
   {
      int len = strlen(_const_oss_default_name)+4;
      char *name = malloc(len);
      if (name)
      {
         snprintf(name, len, "/dev/dsp%i", handle->nodenum);
         handle->devnode = name;
         rv = AAX_TRUE;
      }
   }
   else
   {
      handle->devnode = (char*)_const_oss_default_name;
      rv = AAX_TRUE;
   }

   return rv;
}

static int
detect_nodenum(const char *devname)
{
   int version = get_oss_version();
   int rv = _oss_default_nodenum;

   if (!strncmp(devname, "/dev/dsp", 8) ) {
       rv = atoi(devname+8);
   }
   else if (devname && strcasecmp(devname, "OSS") &&
                       strcasecmp(devname, "default"))
   {
      int fd, err;

      fd = open(_default_mixer, O_RDWR);
      if (fd < 0)			/* test for /dev/mixer0 instead */
      {
         char *mixer = _aax_strdup(_default_mixer);

         *(mixer+strlen(mixer)-1) = '\0';
         fd = open(_default_mixer, O_WRONLY);
         free(mixer);
      }

      if (fd >= 0)
      {
         if (version >= OSS_VERSION_4)
         {
            oss_sysinfo info;
            char name[255];
            char *ptr;

            snprintf(name, 255, "%s", devname);
            ptr = strstr(name, ": ");
            if (ptr) {
               int slen = strlen(ptr+1);
               memmove(ptr, ptr+1, slen);
               *(ptr+slen) = 0;
            }

            err = pioctl(fd, SNDCTL_SYSINFO, &info);
            if (err >= 0)
            {
               int i;
               for (i = 0; i < info.numcards; i++)
               {
#if 1
                  oss_audioinfo ainfo;

                  ainfo.dev = i;
                  if ((err = pioctl(fd, SNDCTL_AUDIOINFO_EX, &ainfo)) < 0) {
                     continue;
                  }

                  if (strstr(name, ainfo.name))
                  {
                     rv = i;
                     break;
                  }
#else
                  oss_card_info cinfo;

                  cinfo.card = i;
                  if ( (err = pioctl (fd, SNDCTL_CARDINFO, &cinfo)) < 0) {
                     continue;
                  }

                  if (!strcasecmp(name, cinfo.longname))
                  {
                     rv = cinfo.card;
                     break;
                  }
#endif
               }
            }
         }

         close(fd);
         fd = -1;
      }
   }

   return rv;
}

