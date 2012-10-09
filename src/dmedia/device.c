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

/* 
 * Use "audiopanel -print" to get the list of audio device names.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#if HAVE_STRINGS_H
# include <strings.h>	/* strcasecmp */
#endif

#include <aax/aax.h>
#include <xml.h>

#include <api.h>
#include <arch.h>
#include <devices.h>
#include <ringbuffer.h>
#include <base/dlsym.h>
#include <base/logging.h>

#include "audio.h"
#include "device.h"

#ifndef _MIPS_SIM
#define _MIPS_SIM	1
#define	_MIPS_SIM_ABI64	0
#endif

#if defined (sgi)
#define DMEDIA_ID_STRING	"Irix 6.5"
#elif defined(__linux__) || (__linux)
#define DMEDIA_ID_STRING	"Linux ProPack 4/SP2+"
#else
#define DMEDIA_ID_STRING	"Unknown"
#endif

#ifndef _DEBUG
# define _DEBUG			0
#endif
#define MAX_ID_STRLEN		32

#define DEFAULT_OUTPUT_RATE	48000
#define DEFAULT_DEVNAME		"Analog Out"
#define DEFAULT_RENDERER	"DMedia"

static _aaxDriverDetect _aaxDMediaDriverDetect;
static _aaxDriverNewHandle _aaxDMediaDriverNewHandle;
static _aaxDriverGetDevices _aaxDMediaDriverGetDevices;
static _aaxDriverGetInterfaces _aaxDMediaDriverGetInterfaces;
static _aaxDriverConnect _aaxDMediaDriverConnect;
static _aaxDriverDisconnect _aaxDMediaDriverDisconnect;
static _aaxDriverSetup _aaxDMediaDriverSetup;
static _aaxDriverState _aaxDMediaDriverPause;
static _aaxDriverState _aaxDMediaDriverResume;
static _aaxDriverCaptureCallback _aaxDMediaDriverCapture;
static _aaxDriverCallback _aaxDMediaDriverPlayback;
static _aaxDriverGetName _aaxDMediaGetName;
static _aaxDriverState _aaxDMediaDriverAvailable;
static _aaxDriver3dMixerCB _aaxDMediaDriver3dMixer;
static _aaxDriverParam _aaxDMediaDriverGetLatency;

char _dmedia_id_str[MAX_ID_STRLEN+1] = DEFAULT_RENDERER;
const _aaxDriverBackend _aaxDMediaDriverBackend =
{
   1.0,
   AAX_PCM16S,
   DEFAULT_OUTPUT_RATE,
   2,

   AAX_VERSION_STR,
   DEFAULT_RENDERER,
   AAX_VENDOR_STR,
   (char *)&_dmedia_id_str,

   (_aaxCodec **)&_oalRingBufferCodecs,

   (_aaxDriverDetect *)&_aaxDMediaDriverDetect,
   (_aaxDriverNewHandle *)&_aaxDMediaDriverNewHandle,
   (_aaxDriverGetDevices *)&_aaxDMediaDriverGetDevices,
   (_aaxDriverGetInterfaces *)&_aaxDMediaDriverGetInterfaces,

   (_aaxDriverGetName *)&_aaxDMediaGetName,
   (_aaxDriverThread *)&_aaxSoftwareMixerThread,

   (_aaxDriverConnect *)&_aaxDMediaDriverConnect,
   (_aaxDriverDisconnect *)&_aaxDMediaDriverDisconnect,
   (_aaxDriverSetup *)&_aaxDMediaDriverSetup,
   (_aaxDriverState *)&_aaxDMediaDriverPause,
   (_aaxDriverState *)&_aaxDMediaDriverResume,
   (_aaxDriverCaptureCallback *)&_aaxDMediaDriverCapture,
   (_aaxDriverCallback *)&_aaxDMediaDriverPlayback,

   (_aaxDriver2dMixerCB *)&_aaxFileDriverStereoMixer,
   (_aaxDriver3dMixerCB *)&_aaxDMediaDriver3dMixer,
   (_aaxDriverPrepare3d *)&_aaxFileDriver3dPrepare,
   (_aaxDriverPostProcess *)&_aaxSoftwareMixerPostProcess,
   (_aaxDriverPrepare *)&_aaxSoftwareMixerApplyEffects,

   (_aaxDriverState *)&_aaxDMediaDriverAvailable,
   (_aaxDriverState *)&_aaxDMediaDriverAvailable,
   (_aaxDriverState *)&_aaxDMediaDriverAvailable,

   (_aaxDriverParam *)&_aaxDMediaDriverGetLatency
};


#define BUF_SIZE	64
#define MAX_DELTA	8
#define CHECK_FRAMES	4
#define MAX_PORTS	1

typedef struct
{
   ALport port;
   ALconfig config;

   char *name;
   int device;
   unsigned int bytes_sample;
   unsigned int no_channels;
   stamp_t offset;
   float frequency_hz;
   float latency;

   unsigned int _no_channels_init;
   unsigned int _no_channels_avail;

} _port_t;

typedef struct
{
   int mode;
   unsigned int noPorts;
   _port_t *port;

   void **scratch;
   int16_t **data;
#ifndef NDEBUG
   unsigned int buf_len;
#endif

   _oalRingBufferMix1NFunc *mix_mono3d;

} _driver_t;

#define DM_TIE_FUNCTION(d, f)	p##f = (f##_proc)_oalGetProcAddress((d), #f)

DECL_FUNCTION(alSetParams);
DECL_FUNCTION(alGetResourceByName);
DECL_FUNCTION(alSetConfig);
DECL_FUNCTION(alSetDevice);
DECL_FUNCTION(alOpenPort);
DECL_FUNCTION(alSetWidth);
DECL_FUNCTION(alSetSampFmt);
DECL_FUNCTION(alSetQueueSize);
DECL_FUNCTION(alSetChannels);
DECL_FUNCTION(alNewConfig);
DECL_FUNCTION(alFreeConfig);
DECL_FUNCTION(alClosePort);
DECL_FUNCTION(alGetParams);
DECL_FUNCTION(alGetFrameTime);
DECL_FUNCTION(alGetFrameNumber);
DECL_FUNCTION(alDiscardFrames);
DECL_FUNCTION(alGetParamInfo);
DECL_FUNCTION(alWriteFrames);
DECL_FUNCTION(alReadFrames);
DECL_FUNCTION(alGetErrorString);

DECL_FUNCTION(dmGetError);
DECL_FUNCTION(dmG711AlawDecode);
DECL_FUNCTION(dmG711MulawDecode);
DECL_FUNCTION(dmDVIAudioDecoderCreate);
DECL_FUNCTION(dmDVIAudioDecode);

static char *__mode[2] = { "r", "w" };
const char *_dmedia_default_name = DEFAULT_DEVNAME;

static int detect_devnum(const char *);
static void sync_ports(const void *);
#ifndef oserror
static int oserror(void);
#endif

#ifndef NDEBGUG
#define __REPORT_ERROR() \
  if (palGetErrorString) { \
   fprintf(stderr, "alGetParams failed: %s\n", palGetErrorString(oserror())); \
   fprintf(stderr, "at line %i\n", __LINE__); \
 }
#else
#define __REPORT_ERROR()
#endif


static int
_aaxDMediaDriverDetect(int mode)
{
   static int rv = AAX_FALSE;
   void *audio = NULL;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if TEST_FOR_FALSE(rv) {
     audio = _oalIsLibraryPresent("audio", 0);
   }

   if (audio)
   {
      const char *hwstr = _aaxGetSIMDSupportString();
      char *error;

      snprintf(_dmedia_id_str, MAX_ID_STRLEN, "%s %s %s",
               DEFAULT_RENDERER, DMEDIA_ID_STRING, hwstr);

      _oalGetSymError(0);

      TIE_FUNCTION(alSetParams);
      if (palSetParams)
      {
         TIE_FUNCTION(alGetResourceByName);
         TIE_FUNCTION(alSetConfig);
         TIE_FUNCTION(alSetDevice);
         TIE_FUNCTION(alOpenPort);
         TIE_FUNCTION(alSetWidth);
         TIE_FUNCTION(alSetSampFmt);
         TIE_FUNCTION(alSetQueueSize);
         TIE_FUNCTION(alSetChannels);
         TIE_FUNCTION(alNewConfig);
         TIE_FUNCTION(alFreeConfig);
         TIE_FUNCTION(alClosePort);
         TIE_FUNCTION(alGetParams);
         TIE_FUNCTION(alGetFrameTime);
         TIE_FUNCTION(alGetFrameNumber);
         TIE_FUNCTION(alDiscardFrames);
         TIE_FUNCTION(alGetParamInfo);
         TIE_FUNCTION(alWriteFrames);
         TIE_FUNCTION(alReadFrames);
         TIE_FUNCTION(alGetErrorString);
      }

      error = _oalGetSymError(0);
      if (!error)
      {
         void *dmedia = _oalIsLibraryPresent("dmedia", 0);
         if (dmedia)
         {
            DM_TIE_FUNCTION(dmedia, dmGetError);
            DM_TIE_FUNCTION(dmedia, dmG711AlawDecode);
            DM_TIE_FUNCTION(dmedia, dmG711MulawDecode);
            DM_TIE_FUNCTION(dmedia, dmDVIAudioDecoderCreate);
            DM_TIE_FUNCTION(dmedia, dmDVIAudioDecode);

            error = _oalGetSymError(0);
            if (!error) {
               rv = AAX_TRUE;
            }
         }
      }
   }

   return rv;
}

static void *
_aaxDMediaDriverNewHandle(enum aaxRenderMode mode)
{
   _driver_t *handle;
   size_t size;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   size = sizeof(_driver_t) + MAX_PORTS*sizeof(_port_t);
   handle = (_driver_t *)calloc(1, size);
   if (handle)
   {
      handle->noPorts = 1;
      handle->port = (_port_t *)((char*)handle + sizeof(_driver_t));
      handle->port[0]._no_channels_avail = 2;
      handle->port[0].frequency_hz = 44100;
      handle->port[0].bytes_sample = 2;
      handle->port[0].no_channels = 2;
      handle->mode = (mode > 0) ? 1 : 0;
   }

   return handle;
}

static void *
_aaxDMediaDriverConnect(const void *id, void *xid, const char *renderer, enum aaxRenderMode mode)
{
   _driver_t *handle = (_driver_t *)id;
   ALpv params[2];
   unsigned int i;
   int res;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(mode < AAX_MODE_WRITE_MAX);

   if (!handle) {
      handle = _aaxDMediaDriverNewHandle(mode);
   }

   if (handle)
   {
      if (renderer) {
         handle->port[0].name = (char *)renderer;
      }

      if (xid)
      {
         float f;
         char *s;
         int i;

         if (!handle->port[0].name)
         {
            s = xmlNodeGetString(xid, "renderer");
            if (s && strcmp(s, "default")) handle->port[0].name = s;
         }

         f = (float)xmlNodeGetDouble(xid, "frequency-hz");
         if (f)
         {
            if (f < (float)_AAX_MIN_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("dmedia; frequency too small.");
               f = (float)_AAX_MIN_MIXER_FREQUENCY;
            }
            else if (f > (float)_AAX_MAX_MIXER_FREQUENCY)
            {
               _AAX_SYSLOG("dmedia; frequency too large.");
               f = (float)_AAX_MAX_MIXER_FREQUENCY;
            }
            handle->port[0].frequency_hz = f;
         }

         if (mode != AAX_MODE_READ)
         {
            i = xmlNodeGetInt(xid, "channels");
            if (i)
            {
               if (i < 1)
               {
                  _AAX_SYSLOG("dmedia; no. tracks too small.");
                  i = 1;
               }
               else if (i > _AAX_MAX_SPEAKERS)
               {
                  _AAX_SYSLOG("dmedia; no. tracks too great.");
                  i = _AAX_MAX_SPEAKERS;
               }
               handle->port[0].no_channels = i;
            }
         }

         i = xmlNodeGetInt(xid, "bits-per-sample");
         if (i)
         {
            if (i != 16)
            {
               _AAX_SYSLOG("dmedia; unsopported bits-per-sample");
               i = 16;
            }
            handle->port[0].bytes_sample = i/8;
         }
      }
   }

   if (handle)
   {
      handle->port[0].device = detect_devnum(handle->port[0].name);
      if (handle->port[0].device <= 0)
      {
         if (handle->port[0].device < 0)
         {
            /* free memory allocations */
            _aaxDMediaDriverDisconnect(handle);
            return NULL;
         }

         handle->port[0].device = AL_DEFAULT_OUTPUT;
      }

#if MAX_PORTS > 1
      handle->port[1].name = _aax_strdup("Analog Out 2");
      handle->port[1].device = detect_devnum(handle->port[1].name);
      if (handle->port[1].device < 0) {
         return NULL;
      }

      if (handle->port[1].device > 0) {
         handle->noPorts++;
      }
#endif

      for (i=0; i < handle->noPorts; i++)
      {
         /*
          * Store the default configuration to restore it when finished.
          */
         params[0].param = AL_CHANNEL_MODE;
         res = palGetParams(handle->port[i].device, params, 1);
         if ((res >= 0) && (params[0].sizeOut >= 0)) {
            handle->port[i]._no_channels_init = params[0].value.i;
         } else {
            handle->port[i]._no_channels_init = 2;
         }

         /*
          * find out how many channels the device has.
          */
         params[0].param = AL_CHANNELS_SGIS;
         res = palGetParams(handle->port[i].device, params, 1);
         if ((res >= 0) && (params[0].sizeOut >= 0))
         {
            handle->port[i]._no_channels_avail = params[0].value.i;
            handle->port[i].no_channels = params[0].value.i;
         }
         else
         {
            handle->port[i]._no_channels_avail = 2;
            handle->port[i].no_channels = 2;
         }
      }


      /*
       * Create a new config that remains available until exit.
       * Only the master port has a configurtion assigned to it.
       * All slave ports share the same config.
       */
      handle->port[0].config = palNewConfig();
      if (handle->port[0].config == NULL)
      {
         _AAX_SYSLOG(palGetErrorString(oserror()));
         return NULL;
      }

      /*
       * Now attempt to open an audio port port using this config
       */
      for (i=0; i < handle->noPorts; i++)
      {
         palSetDevice(handle->port[0].config, handle->port[i].device);

         handle->port[i].port = palOpenPort(handle->port[i].name,
                                    __mode[handle->mode],
                                    handle->port[0].config);

         if (handle->port[i].port == NULL)
         {
            _AAX_LOG(LOG_ERR, palGetErrorString(oserror()));
            return NULL;
         }
      }

      handle->mix_mono3d = _oalRingBufferMixMonoGetRenderer(mode);
   }
   
   return (void *)handle;
}

static int
_aaxDMediaDriverDisconnect(void *id)
{
   _driver_t *handle = (_driver_t *)id;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   if (handle)
   {
      unsigned int i;
      ALpv params;

      if (handle->port[0].config != NULL)
      {
         palFreeConfig(handle->port[0].config);
         handle->port[0].config = NULL;
      }

      for (i=0; i < handle->noPorts; i++)
      {
         params.param = AL_CHANNEL_MODE;
         params.value.i = handle->port[i]._no_channels_init;
         if(handle->port[i]._no_channels_init > 0) {
            palSetParams(handle->port[i].device, &params, 1);
         }

         if (handle->port[i].name) {
           free(handle->port[i].name);
         }

         if (handle->port[i].port != NULL)
         {
            palClosePort(handle->port[i].port);
            handle->port[i].port = NULL;
         }
      }

      free(handle->scratch);

      if (handle->port != NULL)
      {
         free(handle->port);
         handle->port = NULL;
      }

      if (handle != NULL)
      {
         free(handle);
         handle = NULL;
      }
   }

   return AAX_TRUE;
}

static int
_aaxDMediaDriverSetup(const void *id, size_t *frames, int *fmt, unsigned int *tracks, float *speed)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int channels, data_format;
   ALpv params[2];
   unsigned int i;
   int result;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert (id != 0);

   channels = *tracks;
   if (handle->port[0]._no_channels_avail >= channels)
      handle->noPorts = 1;

   switch(*fmt)
   {
   case AAX_PCM8S:
      data_format = AL_SAMPLE_8;
      break;
   case AAX_PCM16S:
      data_format = AL_SAMPLE_16;
      break;
   default:
      _AAX_SYSLOG("dmedia: Unsopported playback format\n");
      return AAX_FALSE;
   }
   handle->port[0].bytes_sample = aaxGetBytesPerSample(*fmt);

   /*
    * Change the playback sample rate
    */
   handle->port[0].frequency_hz = *speed;
   params[0].param = AL_MASTER_CLOCK;
   params[0].value.i = AL_MCLK_TYPE;
   params[1].param = AL_RATE;
   params[1].value.i = (int)handle->port[0].frequency_hz;
   for(i=0; i < handle->noPorts; i++) {
      result = palSetParams(handle->port[i].device, params, 2);
   }

   if (handle->port[0].config != NULL)
   {
      palFreeConfig(handle->port[0].config);
      handle->port[0].config = palNewConfig();
      if (handle->port[0].config == NULL)
      {
         _AAX_SYSLOG(palGetErrorString(oserror()));
         return AAX_FALSE;
      }
   }

   if (handle->port[0].port == NULL)   /* Only if port not initialized */
    {
      unsigned int queuesize;

      handle->port[0].no_channels = channels/handle->noPorts;
      if (handle->port[0].no_channels <= 1) {
         handle->port[0].no_channels = 2;
      }

      for (i=0; i < handle->noPorts; i++)
      {
         params[0].param = AL_CHANNEL_MODE;
         params[0].value.i = handle->port[0].no_channels;
         palSetParams(handle->port[i].device,params, 1);
      }

      /*
       * Change the size of the audio queue
       * and the number of audio channels.
       */
      result = palSetChannels(handle->port[0].config,
                        handle->port[0].no_channels);

      if (frames && (*frames > 0)) {
         queuesize = *frames * channels * handle->port[0].bytes_sample;
      }
      else {
         queuesize = (unsigned int)(handle->port[0].frequency_hz * handle->port[0].no_channels/10.0f);
      }

      palSetQueueSize(handle->port[0].config, queuesize);

      handle->port[0].latency = 0.0f;
      if (frames)
      {
         *frames = queuesize/(channels*handle->port[0].bytes_sample);
         handle->port[0].latency = 2.0f*(float)*frames;
         handle->port[0].latency /= (float)handle->port[0].frequency_hz;
      }
   }

   palSetSampFmt(handle->port[0].config, AL_SAMPFMT_TWOSCOMP);
   palSetWidth(handle->port[0].config, data_format);

   /*
    * Alter configuration parameters, if possible
    */
   assert (handle->port[0].port != NULL);
   for (i=0; i < handle->noPorts; i++)
   {
      palSetDevice(handle->port[0].config, handle->port[i].device);
      result = palSetConfig(handle->port[i].port, handle->port[0].config);
      if (result == -1)
      {
         _AAX_SYSLOG(palGetErrorString(oserror()));
         return AAX_FALSE;
      }
   }

   if (handle->noPorts > 1) {
      sync_ports(handle);
   }

   return AAX_TRUE;
}

static int
_aaxDMediaDriverPause(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   ALpv params;
   unsigned int i;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert (id != 0);

   params.param = AL_RATE;
   params.value.i = 0;
   for (i=0; i < handle->noPorts; i++) {
      palSetParams(handle->port[i].device, &params, 1);
   }

   if (handle->noPorts > 1) {
      sync_ports(id);
   }

   return AAX_TRUE;
}

static int
_aaxDMediaDriverResume(const void *id)
{
   static const int __rate[] = { AL_INPUT_RATE, AL_OUTPUT_RATE };
   _driver_t *handle = (_driver_t *)id;
   ALpv params;
   unsigned int i;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert (id != 0);

   if (handle->noPorts > 1) {
      sync_ports(id);
   }

   params.param = __rate[handle->mode];
   params.value.i = (int)handle->port[0].frequency_hz;
   for (i=0; i < handle->noPorts; i++) {
      palSetParams(handle->port[i].device, &params, 1);
   }

   return AAX_TRUE;
}

static int
_aaxDMediaDriverAvailable(const void *id)
{
   return AAX_TRUE;
}

int
_aaxDMediaDriver3dMixer(const void *id, void *d, void *s, void *p, void *m, int n, unsigned char ctr, unsigned int nbuf)
{
   _driver_t *handle = (_driver_t *)id;
   float gain;
   int ret;

   assert(s);
   assert(d);
   assert(p);

   gain = _aaxDMediaDriverBackend.gain;
   ret = handle->mix_mono3d(d, s, p, m, gain, n, ctr, nbuf);

   return ret;
}


static int
_aaxDMediaDriverCapture(const void *id, void **data, int offs, size_t *frames, void *scratch, size_t scratchlen)
{
   _driver_t *handle = (_driver_t *)id;
   unsigned int nframes = *frames;

   if ((handle->mode != 0) || (frames == 0) || (data == 0)) {
     return AAX_FALSE;
   }

   if (nframes == 0) {
      return AAX_TRUE;
   }

   *frames = 0;
   palReadFrames(handle->port[0].port, scratch, nframes);
   _batch_cvt24_16_intl((int32_t**)data, scratch, 0, 2, nframes);
   *frames = nframes;
   return AAX_TRUE;
}

static int
_aaxDMediaDriverPlayback(const void *id, void *s, float pitch, float volume)
{
#if MAX_PORTS > 1
   static int check_ = CHECK_FRAMES;
#endif
   _oalRingBuffer *rb = (_oalRingBuffer *)s;
   _driver_t *handle = (_driver_t *)id;
   unsigned int no_tracks, no_samples;
   unsigned int offs, outbuf_size;
   _oalRingBufferSample *rbd;
   int16_t *data;

   _AAX_LOG(LOG_DEBUG, __FUNCTION__);

   assert(rb);
   assert(rb->sample);
   assert(id != 0);

   if (handle->mode == 0)
      return 0;

   rbd = rb->sample;
   offs = _oalRingBufferGetOffsetSamples(rb);
   no_samples = _oalRingBufferGetNoSamples(rb) - offs;
   no_tracks = _oalRingBufferGetNoTracks(rb);

   outbuf_size = no_tracks * no_samples*sizeof(int16_t);
   if (handle->scratch == 0)
   {
      char *p = 0;
      handle->scratch = (void**)_aax_malloc(&p, outbuf_size);
      handle->data = (int16_t**)p;
#ifndef NDEBUG
      handle->buf_len = outbuf_size;
#endif
   }
   data = (int16_t*)handle->data;
   assert(outbuf_size <= handle->buf_len);

   _batch_cvt16_intl_24(data, (const int32_t**)rbd->track, offs, no_tracks, no_samples);

   if (is_bigendian()) {
      _batch_endianswap16((uint16_t*)data, no_tracks*no_samples);
   }

   if (handle->noPorts == 1) {
      palWriteFrames(handle->port[0].port, data, no_samples);
   }
#if MAX_PORTS > 1
   else
   {
      /*
       * Setup a buffer for each audio port.
       */
      frames_to_write = bytesToWrite / handle->noPorts;

      buf = (char **)calloc(handle->noPorts, sizeof(char *));
      for (i=0; i < handle->noPorts; i++) {
         buf[i] = malloc(frames_to_write);
      }

      /*
       * Fill the buffers in the appropriate format.
       */
      for(frame_no = 0; frame_no < frames_to_write; frame_no += frame_size)
      {
         for (i=0; i < handle->noPorts; i++)
         {
            memcpy(&buf[i][frame_no], ptr, frame_size);
            ptr += frame_size;
         }
      }

      /*
       * Sent the data to the appropriate port port and clean up the buffers.
       */
      for (i=0; i < handle->noPorts; i++)
      {
         palWriteFrames(handle->port[i].port,buf[i],frames_to_write/frame_size);
         free(buf[i]);
      }
      free(buf);

      if (! --check_)
      {
         check_ = CHECK_FRAMES;
         check_sync(handle);
      }
   }
#endif

   free(data);

   return 0;
}

static char *
_aaxDMediaGetName(const void *id, int playback)
{
   _driver_t *handle = (_driver_t *)id;
   char *ret = (char *)_dmedia_default_name;

#if 0
   ALpv pvs;

   assert (id != 0);

   pvs.param = AL_NAME;
   pvs.value.ptr = name;
   pvs.sizeIn = BUF_SIZE;

   assert(handle->port[0].device !=  0);
   /* TODO: distinguish between playback and record */
   if (palGetParams(handle->port[0].device, &pvs, 1) < 0) __REPORT_ERROR();

   return name;
#else

   /* TODO: distinguish between playback and record */
   if (handle && handle->port[0].name) {
      ret = handle->port[0].name;
   }

   return ret;
#endif
}

static float
_aaxDMediaDriverGetLatency(const void *id)
{
   _driver_t *handle = (_driver_t *)id;
   return handle ? handle->port[0].latency : 0.0f;
}

static char *
_aaxDMediaDriverGetDevices(const void *id, int mode)
{
   static char *renderers[2] = { "\0\0", "\0\0" };
   return (char *)renderers[mode];
}

static char *
_aaxDMediaDriverGetInterfaces(const void *id, const char*devname, int mode)
{
   static char *renderers[2] = { "\0\0", "\0\0" };
   return (char *)renderers[mode];
}

/*-------------------------------------------------------------------------- */

#ifndef HAVE_OSERROR
static int oserror(void)
{
   return 0;
}
#endif

static void
sync_ports(const void *_handle)
{
   _driver_t *handle = (_driver_t *)_handle;
   double nanosec_per_frame = (1000000000.0)/handle->port[0].frequency_hz;
   stamp_t buf_delta_msc;
   stamp_t msc[MAX_PORTS];
   stamp_t ust[MAX_PORTS];
   int corrected;
   unsigned int i;

   /*
    * Get UST/MSC (time & sample frame number) pairs for the
    * device associated with each port.
    */
   for (i = 0; i < handle->noPorts; i++) {
      palGetFrameTime(handle->port[i].port, &msc[i], &ust[i]);
   }

   /*
    * We consider the first device to be the "master" and we
    * calculate, for each other device, the sample frame number
    * on that device which has the same time as ust[0] (the time
    * at which sample frame msc[0] went out on device 0).
    *
    * From that, we calculate the sample frame offset between
    * contemporaneous samples on the two devices. This remains
    * constant as long as the devices don't drift.
    *
    * If the port i is connected to the same device as port 0, you should
    * see offset[i] = 0.
    */

   /* handle->port[0].offset = 0;       / * by definition */
   for (i = 0; i < handle->noPorts; i++) {
      stamp_t msc0 =
         msc[i] + (stamp_t)((double) (ust[0] - ust[i]) / (nanosec_per_frame));      handle->port[i].offset = msc0 - msc[0];
   }

   do {
      stamp_t max_delta_msc;
      corrected = 0;

      /*
       * Now get the sample frame number associated with
       * each port.
       */
      for (i = 0; i < handle->noPorts; i++) {
         palGetFrameNumber(handle->port[i].port, &msc[i]);
      }

      max_delta_msc = msc[0];
      for (i = 0; i < handle->noPorts; i++) {
         /*
          * For each port, see how far ahead or behind
          * the furthest port we are, and keep track of the
          * minimum. in a moment, we'll bring all the ports to this
          * number.
          */
         buf_delta_msc = msc[i] - handle->port[i].offset;
         if (max_delta_msc < buf_delta_msc) {
            max_delta_msc = buf_delta_msc;
         }
      }

      for (i = 0; i < handle->noPorts; i++) {
         buf_delta_msc = msc[i] - handle->port[i].offset;
         if (abs((unsigned int)buf_delta_msc - (unsigned int)max_delta_msc)
                 > MAX_DELTA )
         {
            palDiscardFrames(handle->port[i].port,
                        (int)(max_delta_msc-buf_delta_msc));
            corrected++;
         }
      }
   } while (corrected);
}

static int
detect_devnum(const char *devname)
{
   char *name = (char *)devname;
   ALpv params[2];
   int device = 0;

   if (!name || !strcasecmp(name, DEFAULT_RENDERER)
       || !strcasecmp(name, "default"))
   {
      name = (char *)_dmedia_default_name;
   }

   device = palGetResourceByName(AL_SYSTEM, name, AL_DEVICE_TYPE);
   if (device)
   {
      int res;
      /*
       * if the name refers to an port, select that port
       * on the device
       */
      res = palGetResourceByName(AL_SYSTEM, name, AL_INTERFACE_TYPE);
      if (res)
      {
         params[0].param = AL_INTERFACE;
         params[0].value.i = res;
         palSetParams(device, params, 1);
      }
   }
   else
   {
      device = -1;
      _AAX_LOG(LOG_ERR, palGetErrorString(oserror()));
   }

   return device;
}
