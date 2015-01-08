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

#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <string.h>
# if HAVE_STRINGS_H
#  include <strings.h>   /* strcasecmp */
# endif
#endif
#include <assert.h>		/* assert */
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <aax/aax.h>

#include <base/dlsym.h>

#include <arch.h>
#include <ringbuffer.h>

#include "filetype.h"
#include "audio.h"

#define	MAX_ID3V1_GENRES	192
#define __DUP(a, b)	if ((b) != NULL && (b)->fill) a = strdup((b)->p);
#define __COPY(a, b)	do { int s = sizeof(b); \
      a = calloc(1, s+1); if (a) memcpy(a,b,s); \
   } while(0);
const char *_mp3v1_genres[MAX_ID3V1_GENRES];

static _file_detect_fn _aaxMP3Detect;
static _file_new_handle_fn _aaxMP3Setup;
static _file_get_name_fn _aaxMP3GetName;
static _file_default_fname_fn _aaxMP3Interfaces;
static _file_extension_fn _aaxMP3Extension;
static _file_get_param_fn _aaxMP3GetParam;

static _file_open_fn *_aaxMP3Open;
static _file_close_fn *_aaxMP3Close;
static _file_cvt_to_fn *_aaxMP3CvtToIntl;
static _file_cvt_from_fn *_aaxMP3CvtFromIntl;
static _file_set_param_fn *_aaxMP3SetParam;

_aaxFmtHandle*
_aaxDetectMP3File()
{
   _aaxFmtHandle* rv = NULL;

   rv = calloc(1, sizeof(_aaxFmtHandle));
   if (rv)
   {
      rv->detect = _aaxMP3Detect;
      rv->setup = _aaxMP3Setup;
      rv->open = _aaxMP3Open;
      rv->close = _aaxMP3Close;
      rv->name = _aaxMP3GetName;
      rv->update = NULL;

      rv->cvt_from_intl = _aaxMP3CvtFromIntl;
      rv->cvt_to_intl = _aaxMP3CvtToIntl;
      rv->cvt_endianness = NULL;
      rv->cvt_from_signed = NULL;
      rv->cvt_to_signed = NULL;

      rv->supported = _aaxMP3Extension;
      rv->interfaces = _aaxMP3Interfaces;

      rv->get_param = _aaxMP3GetParam;
      rv->set_param = _aaxMP3SetParam;
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

typedef struct
{
   void *id;
   char *artist;
   char *original;
   char *title;
   char *album;
   char *trackno;
   char *date;
   char *genre;
   char *composer;
   char *comments;
   char *copyright;
   char *image;

   int mode;
   int capturing;
   int id3_found;

   int frequency;
   int bitrate;
   size_t no_samples;
   size_t max_samples;
   enum aaxFormat format;
   int blocksize;
   uint8_t no_tracks;
   uint8_t bits_sample;

   size_t mp3BufSize;
   void *mp3Buffer;
   void *mp3ptr;

#ifdef WINXP
   HACMSTREAM acmStream;
   ACMSTREAMHEADER acmStreamHeader;

   LPBYTE pcmBuffer;
   unsigned long pcmBufPos;
   unsigned long pcmBufMax;
#endif

} _driver_t;

#include "mp3ext_mpg123.c"
#include "mp3ext_msacm.c"

static int
_aaxMP3Detect(void *fmt, int mode)
{
   static void *_audio[2] = { NULL, NULL };
   int m = (mode > 0) ? 1 : 0;
   int rv = AAX_FALSE;

   if (!_audio[m])
   {
#ifdef WINXP
      rv = _aaxMSACMDetect(fmt, m, _audio);
#endif
      /* if not found, try mpg123  with lame */
      if (rv == AAX_FALSE) {
         rv = _aaxMPG123Detect(fmt, m, _audio);
      }
   }
   else {
      rv = AAX_TRUE;
   }

   return rv;
}

static void*
_aaxMP3Setup(int mode, size_t *bufsize, int freq, int tracks, int format, size_t no_samples, int bitrate)
{
   _driver_t *handle = NULL;

   *bufsize = 0;
   if (1) // mode == 0)
   {
      handle = calloc(1, sizeof(_driver_t));
      if (handle)
      {
         handle->capturing = (mode == 0) ? 1 : 0;
         handle->blocksize = 4096;
         handle->frequency = freq;
         handle->no_tracks = tracks;
         handle->bitrate = bitrate;
         handle->format = format;
         handle->no_samples = no_samples;
         handle->max_samples = UINT_MAX;
         handle->bits_sample = aaxGetBitsPerSample(handle->format);

         if (mode == 0) {
            *bufsize = (no_samples*tracks*handle->bits_sample)/8;
         }
      }
      else {
         _AAX_FILEDRVLOG("MP3File: Insufficient memory");
      }
   }
   else {
      _AAX_FILEDRVLOG("MP3File: playback is not supported");
   }

   return (void*)handle;
}

static int
_aaxMP3Extension(char *ext) {
   return (ext && !strcasecmp(ext, "mp3")) ? 1 : 0;
}

static char*
_aaxMP3Interfaces(int mode)
{
   static const char *rd[2] = { "*.mp3\0", "*.mp3\0" };
   return (char *)rd[mode];
}

static char*
_aaxMP3GetName(void *id, enum _aaxFileParam param)
{
   _driver_t *handle = (_driver_t *)id;
   char *rv = NULL;

   switch(param)
   {
   case __F_ARTIST:
      rv = handle->artist;
      break;
   case __F_TITLE:
      rv = handle->title;
      break;
   case __F_COMPOSER:
      rv = handle->composer;
      break;
   case __F_GENRE:
      rv = handle->genre;
      break;
   case __F_TRACKNO:
      rv = handle->trackno;
      break;
   case __F_ALBUM:
      rv = handle->album;
      break;
   case __F_DATE:
      rv = handle->date;
      break;
   case __F_COMMENT:
      rv = handle->comments;
      break;
   case __F_COPYRIGHT:
      rv = handle->copyright;
      break;
   case __F_ORIGINAL:
      rv = handle->original;
      break;
   case __F_IMAGE:
      rv = handle->image;
      break;
   default:
      break;
   }
   return rv;
}

static off_t
_aaxMP3GetParam(void *id, int type)
{
   _driver_t *handle = (_driver_t *)id;
   off_t rv = 0;

   switch(type)
   {
   case __F_FMT:
      rv = handle->format;
      break;
   case __F_TRACKS:
      rv = handle->no_tracks;
      break;
   case __F_FREQ:
      rv = handle->frequency;
      break;
   case __F_BITS:
      rv = handle->bits_sample;
      break;
   case __F_BLOCK:
      rv = handle->blocksize;
      break;
   case __F_SAMPLES:
      rv = handle->max_samples;
      break;
   default:
      break;
   }
   return rv;
}


const char *
_mp3v1_genres[MAX_ID3V1_GENRES] = 
{
   "Blues",
   "Classic Rock",
   "Country",
   "Dance",
   "Disco",
   "Funk",
   "Grunge",
   "Hip-Hop",
   "Jazz",
   "Metal",
   "New Age",
   "Oldies",
   "Other",
   "Pop",
   "Rhythm and Blues",
   "Rap",
   "Reggae",
   "Rock",
   "Techno",
   "Industrial",
   "Alternative",
   "Ska",
   "Death Metal",
   "Pranks",
   "Soundtrack",
   "Euro-Techno",
   "Ambient",
   "Trip-Hop",
   "Vocal",
   "Jazz & Funk",
   "Fusion",
   "Trance",
   "Classical",
   "Instrumental",
   "Acid",
   "House",
   "Game",
   "Sound Clip",
   "Gospel",
   "Noise",
   "Alternative Rock",
   "Bass",
   "Soul",
   "Punk rock",
   "Space",
   "Meditative",
   "Instrumental Pop",
   "Instrumental Rock",
   "Ethnic",
   "Gothic",
   "Darkwave",
   "Techno-Industrial",
   "Electronic",
   "Pop-Folk",
   "Eurodance",
   "Dream",
   "Southern Rock",
   "Comedy",
   "Cult",
   "Gangsta",
   "Top 40",
   "Christian Rap",
   "Pop/Funk",
   "Jungle",
   "Native American",
   "Cabaret",
   "New Wave",
   "Psychedelic",
   "Rave",
   "Showtunes",
   "Trailer",
   "Lo-Fi",
   "Tribal",
   "Acid Punk",
   "Acid Jazz",
   "Polka",
   "Retro",
   "Musical",
   "Rock & Roll",
   "Hard Rock",
   "Folk",
   "Folk-Rock",
   "National Folk",
   "Swing",
   "Fast Fusion",
   "Bebop",
   "Latin",
   "Revival",
   "Celtic",
   "Bluegrass",
   "Avantgarde",
   "Gothic Rock",
   "Progressive Rock",
   "Psychedelic Rock",
   "Symphonic Rock",
   "Slow Rock",
   "Big Band",
   "Chorus",
   "Easy Listening",
   "Acoustic",
   "Humour",
   "Speech",
   "Chanson",
   "Opera",
   "Chamber Music",
   "Sonata",
   "Symphony",
   "Booty Bass",
   "Primus",
   "Porn groove",
   "Satire",
   "Slow Jam",
   "Club",
   "Tango",
   "Samba",
   "Folklore",
   "Ballad",
   "Power Ballad",
   "Rhythmic Soul",
   "Freestyle",
   "Duet",
   "Punk rock",
   "Drum Solo",
   "A capella",
   "Euro-House",
   "Dance Hall",
   "Goa Trance",
   "Drum & Bass",
   "Club-House",
   "Hardcore Techno",
   "Terror",
   "Indie",
   "BritPop",
   "Afro-punk",
   "Polsk Punk",
   "Beat",
   "Christian Gangsta Rap",
   "Heavy Metal",
   "Black Metal",
   "Crossover",
   "Contemporary Christian",
   "Christian Rock",
   "Merengue",
   "Salsa",
   "Thrash Metal",
   "Anime",
   "Jpop",
   "Synthpop",
   "Abstract",
   "Art Rock",
   "Baroque",
   "Bhangra",
   "Big Beat",
   "Breakbeat",
   "Chillout",
   "Downtempo",
   "Dub",
   "EBM",
   "Eclectic",
   "Electro",
   "Electroclash",
   "Emo",
   "Experimental",
   "Garage",
   "Global",
   "IDM",
   "Illbient",
   "Industro-Goth",
   "Jam Band",
   "Krautrock",
   "Leftfield",
   "Lounge",
   "Math Rock",
   "New Romantic",
   "Nu-Breakz",
   "Post-Punk",
   "Post-Rock",
   "Psytrance",
   "Shoegaze",
   "Space Rock",
   "Trop Rock",
   "World Music",
   "Neoclassical",
   "Audiobook",
   "Audio Theatre",
   "Neue Deutsche Welle",
   "Podcast",
   "Indie Rock",
   "G-Funk",
   "Dubstep",
   "Garage Rock",
   "Psybient"
};

