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

static _aaxDriverDetect _aaxOSSDriverDetect;
static _aaxDriverNewHandle _aaxOSSDriverNewHandle;
static _aaxDriverGetDevices _aaxOSSDriverGetDevices;
static _aaxDriverGetInterfaces _aaxOSSDriverGetInterfaces;
static _aaxDriverConnect _aaxOSSDriverConnect;
static _aaxDriverDisconnect _aaxOSSDriverDisconnect;
static _aaxDriverSetup _aaxOSSDriverSetup;
static _aaxDriverState _aaxOSSDriverPause;
static _aaxDriverState _aaxOSSDriverResume;
static _aaxDriverCaptureCallback _aaxOSSDriverCapture;
static _aaxDriverCallback _aaxOSSDriverPlayback;
static _aaxDriverGetName _aaxOSSDriverGetName;
static _aaxDriverState _aaxOSSDriverIsReachable;
static _aaxDriverState _aaxOSSDriverAvailable;
static _aaxDriver3dMixerCB _aaxOSSDriver3dMixer;
static _aaxDriverParam _aaxOSSDriverGetLatency;

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
   (_aaxDriverState *)&_aaxOSSDriverPause,
   (_aaxDriverState *)&_aaxOSSDriverResume,
   (_aaxDriverCaptureCallback *)&_aaxOSSDriverCapture,
   (_aaxDriverCallback *)&_aaxOSSDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxOSSDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)*_aaxOSSDriverAvailable,
   (_aaxDriverState *)*_aaxOSSDriverAvailable,
   (_aaxDriverState *)*_aaxOSSDriverIsReachable,

   (_aaxDriverParam *)&_aaxOSSDriverGetLatency
};

typedef struct
{
   char *name;
   int devnum;

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

   _oalRingBufferMix1NFunc *mix_mono3d;

} _driver_t;

DECL_FUNCTION(ioctl);

static int get_oss_version();
static char *detect_devname(int, unsigned int, char);
static int detect_devnum(const char *);

static const int _mode[] = { O_RDONLY, O_WRONLY };
static int _oss_default_devnum = DEFAULT_DEVNUM;
static char *_oss_default_name = DEFAULT_DEVNAME;
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

         if (!handle->name)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s) handle->devnum = detect_devnum(s);
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

         if (xmlNodeGetBool(xid, "virtual-mixer")) {
            handle->exclusive = 0;
         }
      }

      if (renderer) {
         handle->devnum = detect_devnum(renderer);
      }
#if 0
 printf("frequency-hz: %f\n", handle->frequency_hz);
 printf("channels: %i\n", handle->no_tracks);
 printf("device number: %i\n", handle->devnum);
#endif
   }


   if (handle)
   {
      int fd, m = handle->mode;

      handle->name = detect_devname(handle->devnum, handle->no_tracks, m);
      fd = open(handle->name, handle->mode|handle->exclusive);
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
      if (handle->name)
      {
         if (handle->name != _oss_default_name) {
            free(handle->name);
         }
         handle->name = 0;
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
   float pitch;
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

   /* disable sample conversion */
   if (handle->oss_version >= OSS_VERSION_4)
   {
      int enable = 0;
      err = pioctl(fd, SNDCTL_DSP_COOKEDMODE, &enable);
   }

   err = pioctl(fd, SNDCTL_DSP_SPEED, &freq);
   pitch = freq / *speed;
   *speed = freq;

   no_samples = ceilf(no_samples*pitch);
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
      
   }

   return (err >= 0) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxOSSDriverPause(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   
   if (handle)
   {
      close(handle->fd);
      handle->fd = -1;
      rv = AAX_TRUE;
   }
   return rv;
}

static int
_aaxOSSDriverResume(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;
   if (handle)
   {
      handle->fd = open(handle->name, handle->mode|handle->exclusive);
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
   return rv;
}

static int
_aaxOSSDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxOSSDriverIsReachable(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   if (handle)
   {
      if (handle->oss_version >= OSS_VERSION_4)
      {
         oss_audioinfo ainfo;
         int err;

         ainfo.dev = handle->devnum;
         err = pioctl(handle->fd, SNDCTL_AUDIOINFO_EX, &ainfo);
         if (err >= 0 && ainfo.enabled) {
           rv = AAX_TRUE;
         }
      }
      else {
         rv = AAX_TRUE;
      }
   }

   return rv;
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
_aaxOSSDriverCapture(const void *id, void **data, int off, size_t *frames, void *scratch, size_t scratchlen)
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
      _batch_cvt24_16_intl((int32_t**)data, scratch, 0, 2, res);

      return AAX_TRUE;
   }

   return AAX_FALSE;
}

static int
_aaxOSSDriverPlayback(const void *id, void *s, float pitch, float volume)
{
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_tracks, no_samples;
   unsigned int offs, outbuf_size;
   _oalRingBufferSample *rbd;
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
   offs = _oalRingBufferGetOffsetSamples(rb);
   no_tracks = _oalRingBufferGetNoTracks(rb);
   no_samples = _oalRingBufferGetNoSamples(rb) - offs;

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

   _batch_cvt16_intl_24(data, (const int32_t**)rbd->track, offs, no_tracks, no_samples);

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
   char *ret = (char *)_oss_default_name;

   if (handle && handle->name)
      ret = handle->name;

   return ret;
}

static float
_aaxOSSDriverGetLatency(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle ? handle->latency : 0.0f;
}

static char *
_aaxOSSDriverGetDevices(const void *id, int mode)
{
   static char names[2][256] = { "\0\0", "\0\0" };
   int fd, err;


   fd = open(_default_mixer, O_RDWR);
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
         err = pioctl(fd, SNDCTL_SYSINFO, &info);

         if (err >= 0)
         {
            int i, len;
            char *ptr;

            len = 256;
            ptr = (char *)&names[mode];
            for (i = 0; i < info.numcards; i++)
            {
               oss_card_info cinfo;
               int slen;

               cinfo.card = i;
               if ( (err = pioctl (fd, SNDCTL_CARDINFO, &cinfo)) < 0) {
                  break;
               }

               if (*cinfo.hw_info != 0)
               {
                   snprintf(ptr, len, "%s", cinfo.longname);
                   slen = strlen(ptr)+1;
                   len -= slen;
                   ptr += slen;
               }
            }
            *ptr = 0;
         }
      }
      close(fd);
      fd = -1;
   }

   return (char *)&names[mode];
}

static char *
_aaxOSSDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static const char *rd[2] = { "\0\0", "\0\0" };
   return (char *)rd[mode];

}


/* -------------------------------------------------------------------------- */

static int
get_oss_version()
{
   static int version = -1;

   if (version < 0)
   {
      int fd = open(_oss_default_name, O_WRONLY);  /* open /dev/dsp */
      if (fd < 0)                          /* test for /dev/dsp0 instead */
      {
         char *name = _aax_strdup(_oss_default_name);

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

static char *
detect_devname(int devnum, unsigned int tracks, char mode)
{
   char *rv = (char*)_oss_default_name;

   if (devnum > 0)
   {
      int len = strlen(rv)+4;
      char *name = malloc(len);
      if (name)
      {
         snprintf(name, len, "/dev/dsp%i", devnum);
         rv = name;
      }
   }
   else {
      rv = _aax_strdup("/dev/dsp");
   }

   return rv;
}

static int
detect_devnum(const char *devname)
{
   int version = get_oss_version();
   int devnum = _oss_default_devnum;
   char *name = (char *)devname;

   if (!strncmp(name, "/dev/dsp", 8) ) {
       devnum = atoi(name+8);
   }
   else
   {
      int fd, err;

       if (!strcasecmp(name, "OSS") || !strcasecmp(name, "default")) {
          name = NULL;
       }

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
            err = pioctl(fd, SNDCTL_SYSINFO, &info);

            if (err >= 0)
            {
               int i;
               for (i = 0; i < info.numcards; i++)
               {
                  oss_audioinfo ainfo;
                  oss_card_info cinfo;

                  cinfo.card = i;
                  if ( (err = pioctl (fd, SNDCTL_CARDINFO, &cinfo)) < 0) {
                     break;
                  }

                  memset(&ainfo, 0, sizeof(oss_audioinfo));
                  err = pioctl(fd, SNDCTL_AUDIOINFO_EX, &ainfo);
#if 0
                  printf("ainfo.busy: %i\n", ainfo.busy);
                  printf("ainfo.enabled: %i\n", ainfo.enabled);
                  printf("INPUT: %i\n", ainfo.caps & PCM_CAP_INPUT);
                  printf("OUTPUT: %i\n", ainfo.caps & PCM_CAP_OUTPUT);
#endif
                  if (name && !strcasecmp(name, cinfo.longname))
                  {
                     if (ainfo.enabled && !ainfo.busy) {
                        devnum = cinfo.card;
                     }
                     break;
                  }
                  else if (!name && ainfo.enabled && !ainfo.busy)
                  {
                     devnum = cinfo.card;
                     break;
                  }
               }
            }
         }
         close(fd);
         fd = -1;
      }
   }

   return devnum;
}

