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

#if HAVE_STRINGS_H
# include <strings.h>		/* strncasecmp */
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <errno.h>		/* for ETIMEDOUT, errno */
#include <fcntl.h>		/* SEEK_*, O_* */
#include <assert.h>		/* assert */

#include <xml.h>

#include <base/types.h>
#include <api.h>
#include <arch.h>

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
static _aaxDriverState _aaxSoftwareDriverNotAvailable;
static _aaxDriverCallback _aaxSoftwareDriverPlayback;
static _aaxDriverCaptureCallback _aaxSoftwareDriverCapture;
static _aaxDriverGetName _aaxSoftwareDriverGetName;

char _wave_default_renderer[100] = DEFAULT_RENDERER;
_aaxDriverBackend _aaxSoftwareDriverBackend =
{
   1.0,
   AAX_PCM16S,
   DEFAULT_OUTPUT_RATE,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_wave_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxSoftwareDriverDetect,
   (_aaxDriverGetDevices *)&_aaxSoftwareDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxSoftwareDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxSoftwareDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxSoftwareDriverConnect,
   (_aaxDriverDisconnect *)&_aaxSoftwareDriverDisconnect,
   (_aaxDriverSetup *)&_aaxSoftwareDriverSetup,
   (_aaxDriverState *)&_aaxSoftwareDriverAvailable,
   (_aaxDriverState *)&_aaxSoftwareDriverAvailable,
   (_aaxDriverCaptureCallback *)&_aaxSoftwareDriverCapture,
   (_aaxDriverCallback *)&_aaxSoftwareDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxSoftwareDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxSoftwareDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxSoftwareDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxSoftwareDriverAvailable,
   (_aaxDriverState *)&_aaxSoftwareDriverNotAvailable,
   (_aaxDriverState *)&_aaxSoftwareDriverAvailable
};

typedef struct {
   unsigned bps;
   unsigned blocksize;
   int frequency;
   int no_tracks;
   unsigned int no_samples;
   enum aaxFormat format;
   size_t size_bytes;
} _file_info_t;

typedef struct
{
   int fd;
   int mode;
   char *name;
   float frequency_hz;
   float update_dt;
   uint32_t size_bytes;
   uint16_t no_channels;
   uint8_t bytes_sample;
   unsigned char capture;
   char sse_level;

   int16_t *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

   union
   {
      uint32_t header[WAVE_EXT_HEADER_SIZE];	/* playback */
      _file_info_t file;			/* record   */
   } info;

} _driver_t;

static enum aaxFormat getFormatFromFileFormat(unsigned int, int);
static int _aaxSoftwareDriverUpdateHeader(_driver_t *);
static int _aaxSoftwareDriverReadHeader(_driver_t *);

static uint32_t _aaxDefaultWaveHeader[WAVE_EXT_HEADER_SIZE];
const char *default_renderer = "File: /tmp/AWaveOutput.wav";
#ifndef HAVE_STRDUP
char *strdup(const char *);
#endif

static int
_aaxSoftwareDriverDetect(int mode)
{
   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   return AAX_TRUE;
}

static void *
_aaxSoftwareDriverConnect(const void *id, void *xid, const char *device, enum aaxRenderMode mode)
{
   const int _mode[] = {O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, O_RDONLY|O_BINARY};
   _driver_t *handle = (_driver_t *)id;
   char *renderer = (char *)device;

   if (!renderer) {
      renderer = (char*)default_renderer;
   }

   if (xid || renderer)
   {
      char *s = NULL;

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
#if defined(WIN32)
         char *home = getenv("HOMEPATH");
#else
         char *home = getenv("HOME");
#endif
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

      if (!handle) {
         handle = (_driver_t *)calloc(1, sizeof(_driver_t));
      }

      if (handle)
      {
         handle->capture = (mode > 0) ? 0 : 1;
         handle->mode = _mode[handle->capture];

         if (s) {
            handle->fd = open(s, handle->mode, 0644);
         }

         if (handle->fd >= 0)
         {
            const char *hwstr = _aaxGetSIMDSupportString();
            snprintf(_wave_default_renderer, 99, "%s %s", DEFAULT_RENDERER, hwstr);
            handle->sse_level = _aaxGetSSELevel();

            if (xid)
            {
               float f;
               int i;

               f = (float)xmlNodeGetDouble(xid, "frequency-hz");
               if (f)
               {
                  if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
                  {
                     _AAX_SYSLOG("waveout; frequency too small.");
                     f = (float)_AAX_MIN_MIXER_FREQUENCY;
                  }
                  else if (f > _AAX_MAX_MIXER_FREQUENCY)
                  {
                     _AAX_SYSLOG("waveout; frequency too large.");
                     f = (float)_AAX_MAX_MIXER_FREQUENCY;
                  }
                  handle->frequency_hz = f;
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
      if (!handle->capture) {
         ret = _aaxSoftwareDriverUpdateHeader(handle);
      }
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

   if (handle->fd < 0) {
      handle->fd = open(handle->name, handle->mode, 0644);
   }
   if (handle->fd < 0) return AAX_FALSE;

   if (!handle->capture)
   {
      memcpy(handle->info.header,_aaxDefaultWaveHeader, 4*WAVE_EXT_HEADER_SIZE);
      if (is_bigendian())
      {
         int i;
         for (i=0; i<WAVE_EXT_HEADER_SIZE; i++) {
            handle->info.header[i] = _bswap32(handle->info.header[i]);
         }
      }

      _aaxSoftwareDriverUpdateHeader(handle);
      rv = write(handle->fd, handle->info.header, WAVE_HEADER_SIZE*4);
   }
   else
   {
      /*
       * read the file information and set the file-pointer to
       * the start of the data section
       */
      rv = _aaxSoftwareDriverReadHeader(handle);
   }

   return (rv != -1) ? AAX_TRUE : AAX_FALSE;
}

static int
_aaxSoftwareDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxSoftwareDriverNotAvailable(const void *id)
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
// if (*ptr > 0x007fffff || -(*ptr) > 0x007ffff) printf("! ptr; %08X (%08X)\n", *ptr, -(*ptr));
         ptr++;
      } while (--j);
   }
}
#endif
   _batch_cvt16_intl_24(data, (const int32_t**)rbd->track,
                        offs, no_tracks, no_samples);

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
      _aaxSoftwareDriverUpdateHeader(handle);
      handle->update_dt -= 1.0f;
   }

   return 0;
}

static int
_aaxSoftwareDriverCapture(const void *id, void **data, size_t *frames, void *scratch)
{
   _driver_t *handle = (_driver_t *)id;
   size_t buflen, frame_size;

   if (handle->mode != O_RDONLY || (frames == 0) || (data == 0))
      return AAX_FALSE;

   if (*frames == 0)
      return AAX_TRUE;

   frame_size = handle->info.file.bps * handle->info.file.no_tracks;
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

      if (is_bigendian()) {
         _batch_endianswap16((uint16_t*)scratch, res);
      }
      _batch_cvt24_16_intl((int32_t**)data, scratch, 0, 2, res);

// TODO: possibly adjust format or resample since the requested specifications
//       probably difffer from the data in the file.

      return AAX_TRUE;
   }

   return AAX_FALSE;
}

int
_aaxSoftwareDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr)
{
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxSoftwareDriverBackend.gain;
   ret = _oalRingBufferMixMono16(d, s, p, m, gain, n, ctr);

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

int
_aaxSoftwareDriverStereoMixer(const void *id, void *d, void *s, void *p, void *m, float pitch, float volume, unsigned char ctr)
{
   int ret;

   assert(s);
   assert(d);

   volume *= _aaxSoftwareDriverBackend.gain;
   ret = _oalRingBufferMixMulti16(d, s, p, m, pitch, volume, ctr);

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
    "File\0\0",
    "File\0\0"
   };
   return (char *)rd[mode];
}

static char *
_aaxSoftwareDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   static const char *rd[2] = {
    "/tmp/"AAX_NAME_STR"In.wav\0\0",
    "~/"AAX_NAME_STR"Out.wav\0/tmp/"AAX_NAME_STR"Out.wav\0\0"
   };
   return (char *)rd[mode];
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

int
_aaxSoftwareDriverReadHeader(_driver_t *handle)
{
   uint32_t header[WAVE_EXT_HEADER_SIZE];
   size_t buflen;
   char buf[4];
   int i, res;

// lseek(handle->fd, 0L, SEEK_SET);
   res = read(handle->fd, &header, WAVE_EXT_HEADER_SIZE*4);
   if (res <= 0) return -1;

   if (is_bigendian())
   {
      for (i=0; i<WAVE_EXT_HEADER_SIZE; i++) {
         header[i] = _bswap32(header[i]);
      }
   }

   handle->info.file.frequency = header[6];
   handle->info.file.no_tracks = header[5] >> 16;
   handle->info.file.bps = header[8] >> 16;
   handle->info.file.blocksize = header[8] & 0xFFFF;

   res = header[5] & 0xFFFF;
   i = handle->info.file.bps;
   handle->info.file.format = getFormatFromFileFormat(res, i);

   /* search for the data chunk */
   buflen = 0;
   if (lseek(handle->fd, 32L, SEEK_SET) > 0)
   {
      do
      {
         res = read(handle->fd, buf, 1);
         if ((res > 0) && buf[0] == 'd')
         {
            res = read(handle->fd, buf+1, 3);
            if ((res > 0) && 
                (buf[0] == 'd' && buf[1] == 'a' &&
                 buf[2] == 't' && buf[3] == 'a'))
            {
               res = read(handle->fd, &buflen, 4); /* chunk size */
               if (is_bigendian()) buflen = _bswap32(buflen);
               break;
            }
         }
      }
      while (1);
      handle->info.file.no_samples = (buflen * 8)
                                      / (handle->info.file.no_tracks
                                         * handle->info.file.bps);
   }
   else {
      res = -1;
   }

   return res;
}

static int
_aaxSoftwareDriverUpdateHeader(_driver_t *handle)
{
   int res = 0;

   if (handle->size_bytes != 0)
   {
      unsigned int fmt, size = handle->size_bytes;
      uint32_t s;
      off_t floc;

      s =  WAVE_HEADER_SIZE*4 - 8 + size;
      handle->info.header[1] = s;

      fmt = handle->info.header[5] & 0xFFF;
      s = (handle->no_channels << 16) | fmt;	/* PCM */
      handle->info.header[5] = s;

      s = (uint32_t)handle->frequency_hz;
      handle->info.header[6] = s;

      s *= handle->no_channels * handle->bytes_sample;
      handle->info.header[7] = s;

      s = size;
      handle->info.header[10] = s;

      if (is_bigendian())
      {
         handle->info.header[1] = _bswap32(handle->info.header[1]);
         handle->info.header[5] = _bswap32(handle->info.header[5]);
         handle->info.header[6] = _bswap32(handle->info.header[6]);
         handle->info.header[7] = _bswap32(handle->info.header[7]);
         handle->info.header[10] = _bswap32(handle->info.header[10]);
      }

      floc = lseek(handle->fd, 0L, SEEK_CUR);
      lseek(handle->fd, 0L, SEEK_SET);
      res = write(handle->fd, handle->info.header, WAVE_HEADER_SIZE*4);
      lseek(handle->fd, floc, SEEK_SET);
      if (res == -1) {
         _AAX_SYSLOG(strerror(errno));
      }

#if 0
// printf("Write:\n");
// printf(" 0: %08x\n", handle->info.header[0]);
// printf(" 1: %08x\n", handle->info.header[1]);
// printf(" 2: %08x\n", handle->info.header[2]);
// printf(" 3: %08x\n", handle->info.header[3]);
// printf(" 4: %08x\n", handle->info.header[4]);
// printf(" 5: %08x\n", handle->info.header[5]);
// printf(" 6: %08x\n", handle->info.header[6]);
// printf(" 7: %08x\n", handle->info.header[7]);
// printf(" 8: %08x\n", handle->info.header[8]);
// printf(" 9: %08x\n", handle->info.header[9]);
// printf("10: %08x\n", handle->info.header[10]);
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

   bps = _oalRingBufferFormat[format].bits/8;

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

enum aaxFormat
getFormatFromFileFormat(unsigned int format, int  bps)
{
   enum aaxFormat rv = AAX_FORMAT_NONE;
   switch (format)
   {
   case 1:
      if (bps == 8) rv = AAX_PCM8U;
      else if (bps == 16) rv = AAX_PCM16S_LE;
      else if (bps == 32) rv = AAX_PCM32S_LE;
      break;
   case 3:
      if (bps == 32) rv = AAX_FLOAT_LE;
      else if (bps == 64) rv = AAX_DOUBLE_LE;
      break;
   case 6:
      rv = AAX_ALAW;
      break;
   case 7:
      rv = AAX_MULAW;
      break;
   case 17:
      rv = AAX_IMA4_ADPCM;
      break;
   default:
      break;
   }
   return rv;
}
