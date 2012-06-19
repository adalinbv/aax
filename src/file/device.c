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
#include "filetype.h"

#define DEFAULT_RENDERER	AAX_NAME_STR""
#define DEFAULT_OUTPUT_RATE	22050
#define WAVE_HEADER_SIZE	11
#define WAVE_EXT_HEADER_SIZE	17
#ifndef O_BINARY
# define O_BINARY	0
#endif

static _aaxDriverDetect _aaxFileDriverDetect;
static _aaxDriverNewHandle _aaxFileDriverNewHandle;
static _aaxDriverGetDevices _aaxFileDriverGetDevices;
static _aaxDriverGetInterfaces _aaxFileDriverGetInterfaces;
static _aaxDriverConnect _aaxFileDriverConnect;
static _aaxDriverDisconnect _aaxFileDriverDisconnect;
static _aaxDriverSetup _aaxFileDriverSetup;
static _aaxDriverState _aaxFileDriverAvailable;
static _aaxDriverCallback _aaxFileDriverPlayback;
static _aaxDriverCaptureCallback _aaxFileDriverCapture;
static _aaxDriverGetName _aaxFileDriverGetName;
static _aaxDriver3dMixerCB _aaxFileDriver3dMixer;

char _file_default_renderer[100] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxFileDriverBackend =
{
   1.0,
   AAX_PCM16S,
   DEFAULT_OUTPUT_RATE,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_file_default_renderer,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxFileDriverDetect,
   (_aaxDriverNewHandle *)&_aaxFileDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxFileDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxFileDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxFileDriverGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxFileDriverConnect,
   (_aaxDriverDisconnect *)&_aaxFileDriverDisconnect,
   (_aaxDriverSetup *)&_aaxFileDriverSetup,
   (_aaxDriverState *)&_aaxFileDriverAvailable,
   (_aaxDriverState *)&_aaxFileDriverAvailable,
   (_aaxDriverCaptureCallback *)&_aaxFileDriverCapture,
   (_aaxDriverCallback *)&_aaxFileDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxFileDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxFileDriverAvailable,
   (_aaxDriverState *)&_aaxFileDriverAvailable,
   (_aaxDriverState *)&_aaxFileDriverAvailable
};

typedef struct
{
   int fd;
   int mode;
   char *name;

   float frequency_hz;
   uint16_t no_channels;
   enum aaxFormat format;
   uint8_t bytes_sample;
   char sse_level;

   char *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

   _aaxFileHandle* file;
   _oalRingBufferMix1NFunc *mix_mono3d;

} _driver_t;

const char *default_renderer = "File: /tmp/AWaveOutput.wav";
#ifndef HAVE_STRDUP
char *strdup(const char *);
#endif

static int
_aaxFileDriverDetect(int mode)
{
   _aaxExtensionDetect* ftype;
   int i, rv = AAX_FALSE;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   i = 0;
   do
   {
      if ((ftype = _aaxFileTypes[i++]) != NULL)
      {
         _aaxFileHandle* type = ftype();
         if (type)
         {
            rv = type->detect(mode);
            free(type);
         }
         if (rv) break;
      }
   }
   while (ftype);

   return rv;
}

static void *
_aaxFileDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)calloc(1, sizeof(_driver_t));
   if (handle)
   {
      _aaxExtensionDetect *ftype = _aaxFileTypes[0];

      handle->mode = mode;
      handle->sse_level = _aaxGetSSELevel();
      handle->mix_mono3d = _oalRingBufferMixMonoGetRenderer(mode);
      if (ftype)
      {
         _aaxFileHandle* type = ftype();
         if (type && type->detect(mode)) {
            handle->file = type;
         }
         else
         {
            free(type);
            free(handle);
            handle = NULL;
         }
      }
   }

   return handle;
}

static void *
_aaxFileDriverConnect(const void *id, void *xid, const char *device, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *renderer = (char *)device;
   char *s = NULL;

   if (!renderer) {
      renderer = (char*)default_renderer;
   }

   if (xid || renderer)
   {
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
         const char *home = userHomeDir();
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
   }

   if (!handle) {
      handle = _aaxFileDriverNewHandle(mode);
   }

   if (handle)
   {
      if (s)
      {
         char *ext = strrchr(s, '.');
         if (ext)
         {
            _aaxExtensionDetect* ftype;
            int i = 0;

            ext++;
            do
            {
               if ((ftype = _aaxFileTypes[i++]) != NULL)
               {
                  _aaxFileHandle* type = ftype();
                  if (type && type->detect(mode) && type->supported(ext))
                  {
                     handle->file = type;
                     break;
                  }
                  free(type);
               }
            }
            while (ftype);
         }
      }

      if (handle->file)
      {
         handle->name = s;

         s = (char*)_aaxGetSIMDSupportString();
         snprintf(_file_default_renderer, 99, "%s %s", DEFAULT_RENDERER, s);

         if (xid)
         {
            float f;
            int i;

            f = (float)xmlNodeGetDouble(xid, "frequency-hz");
            if (f)
            {
               if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
               {
                  _AAX_SYSLOG("file; frequency too small.");
                  f = (float)_AAX_MIN_MIXER_FREQUENCY;
               }
               else if (f > _AAX_MAX_MIXER_FREQUENCY)
               {
                  _AAX_SYSLOG("file; frequency too large.");
                  f = (float)_AAX_MAX_MIXER_FREQUENCY;
               }
               handle->frequency_hz = f;
            }

            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_SYSLOG("file; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("file; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_channels = i;
            }

            i = xmlNodeGetInt(xid, "bits-per-sample");
            if (i)
            {
               if (i != 16)
               {
                  _AAX_SYSLOG("file; unsopported bits-per-sample");
                  i = 16;
               }
               handle->bytes_sample = i/8;
            }
         }
      }
      else
      {
         free(handle);
         handle = 0;
      }
   }

   return handle;
}

static int
_aaxFileDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;
   int ret = AAX_TRUE;

   if (handle)
   {
      free(handle->ptr);
      if (handle->name && handle->name != default_renderer) {
         free(handle->name);
      }
      handle->file->close(handle->file->id);
      free(handle->file);
      free(handle);
   }

   return ret;
}

static int
_aaxFileDriverSetup(const void *id, size_t *frames, int *fmt,
                        unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   int rv = AAX_FALSE;

   assert(handle);

   handle->format = *fmt;
   handle->bytes_sample = aaxGetBytesPerSample(*fmt);
   handle->frequency_hz = *speed;


   handle->file->id=handle->file->setup(handle->mode, *speed, *tracks, *fmt, 1);
   if (handle->file->id)
   {
      int res = handle->file->open(handle->file->id, handle->name);
      if (res)
      {
         handle->frequency_hz = handle->file->get_frequency(handle->file->id);
         handle->no_channels = handle->file->get_no_tracks(handle->file->id);
         if (!handle->mode) { /* file data is always converted to PCM24S */
            handle->format = AAX_PCM24S;
         }

         *fmt = handle->format;
         *speed = handle->frequency_hz;
         *tracks = handle->no_channels;

         rv = AAX_TRUE;
      }
   }

   return rv;
}

static int
_aaxFileDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

static int
_aaxFileDriverPlayback(const void *id, void *d, void *s, float pitch, float volume)
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

   outbuf_size = no_tracks * no_samples*sizeof(int16_t); //handle->bytes_sample;
   if (handle->ptr == 0)
   {
      char *p = 0;
      handle->ptr = (char *)_aax_malloc(&p, outbuf_size);
      handle->scratch = (char *)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
   data = (int16_t *)handle->scratch;
   assert(outbuf_size <= handle->buf_len);

   /* TODO:
    * different file formats: 8, 16, 32 bit, floats, adpcm, ulaw, etc
    */
   _batch_cvt16_intl_24(data, (const int32_t**)rbd->track,
                        offs, no_tracks, no_samples);

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)data, no_tracks*no_samples);
   }

   res = handle->file->update(handle->file->id, data, no_samples);

   return res-res; // (res - no_samples);
}

static int
_aaxFileDriverCapture(const void *id, void **tracks, size_t *frames, void *scratch)
{
   _driver_t *handle = (_driver_t *)id;
   int file_framesz, file_bps, file_fmt;
   int bytes, bufsz;

   if ((frames == 0) || (tracks == 0)) {
      return AAX_FALSE;
   }

   if (*frames == 0) {
      return AAX_TRUE;
   }

   file_framesz = handle->file->get_frame_size(handle->file->id);
   file_fmt = handle->file->get_format(handle->file->id);
   file_bps = aaxGetBytesPerSample(file_fmt);

   bytes = handle->file->update(handle->file->id, scratch, *frames);

   bufsz = *frames * file_framesz;		/* clear the remaining buffer */
   if (bytes != bufsz) {
      memset((char*)scratch+bytes, 0, bufsz-bytes);
      bytes = bufsz;
   }

   if (bytes)
   {
      int file_no_tracks = handle->file->get_no_tracks(handle->file->id);
      unsigned int file_no_frames = bytes / file_bps;
      unsigned int no_frames = bytes / file_framesz;
      void *data = scratch;

      if (handle->ptr == 0)
      {
         unsigned int outbuf_size;
         char *p = 0;

         outbuf_size = handle->no_channels * no_frames*sizeof(int32_t);
         handle->ptr = (char *)_aax_malloc(&p, outbuf_size);
         handle->scratch = (char *)p;
#ifndef NDEBUG
         handle->buf_len = outbuf_size;
#endif
      }

      if (no_frames != *frames) {
         memset(data+file_no_frames, 0, *frames*file_framesz-bytes);
      }
      no_frames = *frames;
					/* first convert to native endianness */
      if (is_bigendian())	/* WAV is little endian */
      {
         switch (file_fmt)
         {
         case AAX_PCM16S:
            _batch_endianswap16(data, file_no_frames);
            break;
         case AAX_PCM24S:
         case AAX_PCM32S:
         case AAX_FLOAT:
            _batch_endianswap32(data, file_no_frames);
            break;
         case AAX_DOUBLE:
            _batch_endianswap64(data, file_no_frames);
            break;
         default:
            break;
         }
      }
					/* then convert to proper signedness */
      if (file_fmt == AAX_PCM8U)
      {
         _batch_cvt8u_8s(data, file_no_frames);
         file_fmt = AAX_PCM8S;
      }
					/* then convert to signed 24-bit */
      if (file_fmt != AAX_PCM24S)
      {
         char *ndata = handle->scratch;
         bufConvertDataToPCM24S(ndata, data, file_no_frames, file_fmt);
         data = ndata;
      }
					/* last resample and de-interleave */
     _batch_cvt24_24_intl((int32_t**)tracks, data, 0, file_no_tracks, no_frames);
   }

   return AAX_TRUE;
}

int
_aaxFileDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr)
{
   _driver_t *handle = (_driver_t *)id;
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxFileDriverBackend.gain;
   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr);

   return ret;
}

void
_aaxFileDriver3dPrepare(void* sp3d, void* fp3d, const void* info, const void* p2d, void* src)
{
   assert(sp3d);
   assert(info);
   assert(p2d);
   assert(src);

   _oalRingBufferPrepare3d(sp3d, fp3d, info, p2d, src);
}

int
_aaxFileDriverStereoMixer(const void *id, void *d, void *s, void *p, void *m, float pitch, float volume, unsigned char ctr)
{
   int ret;

   assert(s);
   assert(d);

   volume *= _aaxFileDriverBackend.gain;
   ret = _oalRingBufferMixMulti16(d, s, p, m, pitch, volume, ctr);

   return ret;
}

char *
_aaxFileDriverGetName(const void *id, int playback)
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
_aaxFileDriverGetDevices(const void *id, int mode)
{
   static const char *rd[2] = { "File\0\0", "File\0\0" };
   return (char *)rd[mode];
}

static char *
_aaxFileDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;

   if (handle && handle->file && handle->file->detect(mode)) {
      rv = handle->file->interfaces(mode);
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
_aaxFileDriverWrite(const char *file, enum aaxProcessingType type,
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

