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

#include <string.h>		/* strlen */
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
#include "audio.h"

#define BACKEND_NAME_OLD	"File"
#define BACKEND_NAME		"Audio Files"
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
static _aaxDriverCaptureCallback _aaxFileDriverCapture;
static _aaxDriverCallback _aaxFileDriverPlayback;
static _aaxDriverGetName _aaxFileDriverGetName;
static _aaxDriver3dMixerCB _aaxFileDriver3dMixer;
static _aaxDriverState _aaxFileDriverState;
static _aaxDriverParam _aaxFileDriverParam;

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
   (_aaxDriverCaptureCallback *)&_aaxFileDriverCapture,
   (_aaxDriverCallback *)&_aaxFileDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxFileDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxFileDriverState,
   (_aaxDriverParam *)&_aaxFileDriverParam,
   (_aaxDriverLog *)&_aaxFileDriverLog
};

typedef struct
{
   int fd;
   int mode;
   char *name;

   float latency;
   float frequency;
   enum aaxFormat format;
   uint8_t no_channels;
   uint8_t bits_sample;
   char sse_level;

   char *ptr, *scratch;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

   _aaxFileHandle* file;
   _oalRingBufferMix1NFunc *mix_mono3d;
   char *interfaces;

} _driver_t;

const char *default_renderer = BACKEND_NAME": /tmp/AeonWaveOut.wav";

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
         unsigned int devlenold = 5; /* strlen(BACKEND_NAME_OLD":"); */
         unsigned int devlen = 12;   /* strlen(BACKEND_NAME":");     */
         if (!strncasecmp(renderer, BACKEND_NAME":", devlen))
         {
            renderer += devlen;
            while (*renderer == ' ' && *renderer != '\0') renderer++;
         }
         else if (!strncasecmp(renderer, BACKEND_NAME_OLD":", devlenold))
         {
            renderer += devlenold;
            while (*renderer == ' ' && *renderer != '\0') renderer++;
         }

         if (!strcasecmp(renderer, "default")) {
            s = (char *)default_renderer;
         }
         else {
            s = _aax_strdup(renderer);
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
                  _AAX_FILEDRVLOG("File: frequency too small.");
                  f = (float)_AAX_MIN_MIXER_FREQUENCY;
               }
               else if (f > _AAX_MAX_MIXER_FREQUENCY)
               {
                  _AAX_FILEDRVLOG("File: frequency too large.");
                  f = (float)_AAX_MAX_MIXER_FREQUENCY;
               }
               handle->frequency = f;
            }

            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_FILEDRVLOG("File: no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_FILEDRVLOG("File: no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->no_channels = i;
            }

            i = xmlNodeGetInt(xid, "bits-per-sample");
            if (i)
            {
               if (i != 16)
               {
                  _AAX_FILEDRVLOG("File: unsopported bits-per-sample");
                  i = 16;
               }
               handle->bits_sample = i;
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
      free(handle->interfaces);
      free(handle);
   }

   return ret;
}

static int
_aaxFileDriverSetup(const void *id, size_t *frames, int *fmt,
                        unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   int freq, rv = AAX_FALSE;

   assert(handle);

   handle->format = *fmt;
   handle->bits_sample = aaxGetBitsPerSample(*fmt);
#if 0
   if (!handle->frequency) {
      handle->frequency = *speed;
   }
#else
   handle->frequency = *speed;
#endif
   freq = (int)handle->frequency;

   handle->file->id = handle->file->setup(handle->mode, freq, *tracks, *fmt);
   if (handle->file->id)
   {
      int res = handle->file->open(handle->file->id, handle->name);
      if (res)
      {
         unsigned int no_frames = *frames;
         float pitch;

         freq = handle->file->get_frequency(handle->file->id);
         pitch = freq / *speed;

         handle->frequency = (float)freq;
         handle->no_channels = handle->file->get_no_tracks(handle->file->id);

         *fmt = handle->format;
         *speed = handle->frequency;
         *tracks = handle->no_channels;

         no_frames = (unsigned int)ceilf(no_frames * pitch);
         if (no_frames & 0xF)
         {
            no_frames |= 0xF;
            no_frames++;
         }
         *frames = no_frames;

         handle->latency = (float)no_frames / (float)handle->frequency;
         rv = AAX_TRUE;
      }
      else
      {
//       _AAX_FILEDRVLOG("File: Unable to open the requested file");
         free(handle->file->id);
         handle->file->id = 0;
      }
   }
   else {
      _AAX_FILEDRVLOG("File: Unable to intiialize the handler");
   }

   return rv;
}

static int
_aaxFileDriverPlayback(const void *id, void *s, float pitch, float gain)
{
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_tracks, no_samples;
   unsigned int offs, outbuf_size;
   _oalRingBufferSample *rbd;
   const int32_t** sbuf;
   char *data, *ndata;
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

   outbuf_size = no_tracks * no_samples*(sizeof(int32_t)+handle->bits_sample);
   if (handle->ptr == 0)
   {
      char *p = 0;
      handle->ptr = (char *)_aax_malloc(&p, outbuf_size);
      handle->scratch = (char *)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
   data = (char *)handle->scratch;
   ndata = data + no_tracks*no_samples*sizeof(int32_t);
   assert(outbuf_size <= handle->buf_len);

   sbuf = (const int32_t**)rbd->track;
// Software Volume, need to convert to Hardware Volume for gain < 1.0f
   if (gain < 0.99f)
   {
      int t;
      for (t=0; t<no_tracks; t++) {
         _batch_mul_value((void*)(sbuf[t]+offs), sizeof(int32_t), no_samples, gain);
      }
   }
   _batch_cvt24_intl_24(data, sbuf, offs, no_tracks, no_samples);
   bufConvertDataFromPCM24S(ndata, data, no_tracks, no_samples,
                            handle->format, 1); // blocksize);
   data = ndata;

   if (handle->format == AAX_PCM8U) {
      _batch_cvt8u_8s(data, no_samples*no_tracks);
   }
   else if (is_bigendian())
   {
      switch (handle->format)
      {  
      case AAX_PCM16S:
         _batch_endianswap16(data, no_samples*no_tracks);
         break;
      case AAX_PCM24S:
      case AAX_PCM32S:
      case AAX_FLOAT:
         _batch_endianswap32(data, no_samples*no_tracks);
         break;
      case AAX_DOUBLE:
         _batch_endianswap64(data, no_samples*no_tracks);
         break;
      default:
         break;
      }
   }

   res = handle->file->update(handle->file->id, data, no_samples);

   return (res >= 0) ? (res-res) : INT_MAX; // (res - no_samples);
}

static int
_aaxFileDriverCapture(const void *id, void **tracks, int offs, size_t *frames, void *scratch, size_t scratchlen, float gain)
{
   _driver_t *handle = (_driver_t *)id;
   int bytes = 0;

   if ((frames == 0) || (tracks == 0)) {
      return AAX_FALSE;
   }

   if (*frames)
   {
      int file_no_tracks = handle->file->get_no_tracks(handle->file->id);
      int file_bits = handle->file->get_bits_per_sample(handle->file->id);
      int file_fmt = handle->file->get_format(handle->file->id);
      unsigned int no_frames, file_no_samples;
      unsigned int abytes, bufsz;
      void *data = scratch;
      int32_t **sbuf;

      no_frames = *frames;
      bufsz = (file_no_tracks * no_frames * file_bits)/8;

      if (handle->ptr == 0)
      {
         unsigned int outbuf_size;
         char *p = 0;

         outbuf_size = handle->no_channels * no_frames * sizeof(int32_t);
         handle->ptr = (char *)_aax_malloc(&p, outbuf_size);
         handle->scratch = (char *)p;
#ifndef NDEBUG
         handle->buf_len = outbuf_size;
#endif
      }
						/* read the frames */
      bytes = handle->file->update(handle->file->id, scratch, no_frames);
      if (bytes <= 0) return bytes;

      abytes = abs(bytes);
      file_no_samples = abytes*8/file_bits;		/* still interleaved */
					/* then convert to proper signedness */
      if (file_fmt == AAX_PCM8U)
      {
         _batch_cvt8u_8s(data, file_no_samples);
         file_fmt = AAX_PCM8S;
      }
      else if (is_bigendian())	/* WAV is little endian */
      {				/* convert to native endianness */
         switch (file_fmt)
         {
         case AAX_PCM16S:
            _batch_endianswap16(data, file_no_samples);
            break;
         case AAX_PCM24S:
         case AAX_PCM32S:
         case AAX_FLOAT:
            _batch_endianswap32(data, file_no_samples);
            break;
         case AAX_DOUBLE:
            _batch_endianswap64(data, file_no_samples);
            break;
         default:
            break;
         }
      }
					/* then convert to signed 24-bit */
      if (file_fmt != AAX_PCM24S)
      {
         char *ndata = handle->scratch;
         bufConvertDataToPCM24S(ndata, data, file_no_samples, file_fmt);
         data = ndata;
      }
				/* clear the rest of the buffer if required */
      if (abytes != bufsz)
      {
         size_t bufsize = (bufsz-abytes)*32/file_bits;
         unsigned int samples = abytes*8/file_bits;

         memset((int32_t*)data+samples, 0, bufsize);
      }
					/* last resample and de-interleave */
      sbuf = (int32_t**)tracks;
      _batch_cvt24_24_intl(sbuf, data, offs, file_no_tracks, no_frames);

      if (gain < 0.99f || gain > 1.01f)
      {
         int t;
         for (t=0; t<file_no_tracks; t++) {
            _batch_mul_value((void*)(sbuf[t]+offs), sizeof(int32_t), no_frames,
                             gain);
         }
      }
   }

   return bytes;
}

int
_aaxFileDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   _driver_t *handle = (_driver_t *)id;
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxFileDriverBackend.gain;
   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr, nbuf);

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
_aaxFileDriverStereoMixer(const void *id, void *d, void *s, void *p, void *m, float pitch, float volume, unsigned char ctr, unsigned int nbuf)
{
   int ret;

   assert(s);
   assert(d);

   volume *= _aaxFileDriverBackend.gain;
   ret = _oalRingBufferMixMulti16(d, s, p, m, pitch, volume, ctr, nbuf);

   return ret;
}

static char *
_aaxFileDriverGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = NULL;

   if (handle)
   {
      if (handle->name) {
         ret = _aax_strdup(handle->name);
      } else {
         ret = _aax_strdup("default");
      }
   }

   return ret;
}

static int
_aaxFileDriverState(const void *id, enum _aaxDriverState state)
{
   int rv = AAX_FALSE;
   switch(state)
   {
   case DRIVER_AVAILABLE:
   case DRIVER_PAUSE:
   case DRIVER_RESUME:
   case DRIVER_SUPPORTS_PLAYBACK:
   case DRIVER_SUPPORTS_CAPTURE:
      rv = AAX_TRUE;
      break;
    case DRIVER_SHARED_MIXER:
   default:
      break;
   }
   return rv;
}

static float
_aaxFileDriverParam(const void *id, enum _aaxDriverParam param)
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
      default:
         break;
      }
   }
   return rv;
}

static char *
_aaxFileDriverGetDevices(const void *id, int mode)
{
   static const char *rd[2] = { BACKEND_NAME"\0\0", BACKEND_NAME"\0\0" };
   return (char *)rd[mode];
}

static char *
_aaxFileDriverGetInterfaces(const void *id, const char *devname, int mode)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;

   if (handle && !handle->interfaces)
   {
      _aaxExtensionDetect* ftype;
      char interfaces[2048];
      unsigned int buflen;
      char *ptr;
      int i = 0;

      ptr = interfaces;
      buflen = 2048;

      do
      {
         if ((ftype = _aaxFileTypes[i++]) != NULL)
         {
            _aaxFileHandle* type = ftype();
            if (type)
            {
               if (type->detect(mode))
               {
                  char *ifs = type->interfaces(mode);
                  unsigned int len = strlen(ifs);
                  if (ifs && len)
                  {
                     snprintf(ptr, buflen, "%s ", ifs);
                     buflen -= len+1;
                     ptr += len+1;
                  }
               }
               free(type);
            }
         }
      }
      while (ftype);
      
      if (ptr != interfaces)
      {
         *(ptr-1) = '\0';
         ptr++;
         rv = handle->interfaces = malloc(ptr-interfaces);
         if (rv) {
            memcpy(rv, interfaces, ptr-interfaces);
         }
      }
   }
   else if (handle) {
      rv = handle->interfaces;
   }

   return rv;
}

char *
_aaxFileDriverLog(const char *str)
{
   static char _errstr[256];
   int len = _MIN(strlen(str)+1, 256);

   memcpy(_errstr, str, len);
   _errstr[255] = '\0';  /* always null terminated */

   __aaxErrorSet(AAX_BACKEND_ERROR, (char*)&_errstr);
   _AAX_SYSLOG(_errstr);

   return (char*)&_errstr;
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
      _AAX_FILEDRVLOG("File: unsupported format");
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
      _AAX_FILEDRVLOG(strerror(errno));
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
      _AAX_FILEDRVLOG(strerror(errno));
   }

   close(fd);
}

