/*
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __FILE_FMT_VORBIS_H
#define __FILE_FMT_VORBIS_H 1

#include <aax/aax.h>

#include "ext_ogg.h"

/*
 * Public domain vorbis decoder:
 *
 * http://nothings.org/stb_vorbis/
 * https://github.com/nothings/stb
 */

#define STB_VORBIS_NO_PULLDATA_API		1
#define STB_VORBIS_MAX_CHANNELS			_AAX_MAX_SPEAKERS
#ifndef TRUE
# define TRUE					true
#endif
#ifndef FALSE
# define FALSE					false
#endif
#ifdef alloca
# undef alloca
#endif

#define STB_VORBIS_HEADER_ONLY
#include <3rdparty/stb_vorbis.c> 

#undef FALSE
#undef TRUE

/** libvorbisenc */
#define OV_ECTL_RATEMANAGE2_SET		0x15

#define OV_FALSE			-1
#define OV_EOF				-2
#define OV_HOLE				-3
#define OV_EREAD			-128
#define OV_EFAULT			-129
#define OV_EIMPL			-130
#define OV_EINVAL			-131
#define OV_ENOTVORBIS			-132
#define OV_EBADHEADER			-133
#define OV_EVERSION			-134
#define OV_ENOTAUDIO			-135
#define OV_EBADPACKET			-136
#define OV_EBADLINK			-137
#define OV_ENOSEEK			-138

typedef struct {
  int version;
  int channels;
  long rate;
  long bitrate_upper;
  long bitrate_nominal;
  long bitrate_lower;
  long bitrate_window;
  void *codec_setup;
} vorbis_info;

typedef struct {
  int analysisp;
  vorbis_info *vi;
  float **pcm;
  float **pcmret;
  int pcm_storage;
  int pcm_current;
  int pcm_returned;
  int preextrapolate;
  int eofflag;
  long lW;
  long W;
  long nW;
  long centerW;
  int64_t granulepos;
  int64_t sequence;
  int64_t glue_bits;
  int64_t time_bits;
  int64_t floor_bits;
  int64_t res_bits;
  void *backend_state;
} vorbis_dsp_state;

typedef struct {
  char **user_comments;
  int *comment_lengths;
  int comments;
  char *vendor;
} vorbis_comment;

typedef struct {
  float **pcm;
  oggpack_buffer opb;
  long lW;
  long W;
  long nW;
  int pcmend;
  int mode;
  int eofflag;
  int64_t granulepos;
  int64_t sequence;
  vorbis_dsp_state *vd;
  void *localstore;
  long localtop;
  long localalloc;
  long totaluse;
  struct alloc_chain *reap;
  long glue_bits;
  long time_bits;
  long floor_bits;
  long res_bits;
  void *internal;
} vorbis_block;


typedef void (*vorbis_info_init_proc)(vorbis_info*);
typedef int (*vorbis_encode_setup_init_proc)(vorbis_info *);
typedef int (*vorbis_encode_setup_managed_proc)(vorbis_info*, long, long, long, long, long);
typedef int (*vorbis_encode_ctl_proc)(vorbis_info* ,int, void*);
typedef void  (*vorbis_info_clear_proc)(vorbis_info*);

typedef void (*vorbis_comment_init_proc)(vorbis_comment*);
typedef void (*vorbis_comment_add_tag_proc)(vorbis_comment*, const char*, const char*);
typedef void (*vorbis_comment_clear_proc)(vorbis_comment*);

typedef void (*vorbis_analysis_init_proc)(vorbis_dsp_state*, vorbis_info*);
typedef void (*vorbis_analysis_headerout_proc)(vorbis_dsp_state*, vorbis_comment*, ogg_packet*, ogg_packet*, ogg_packet*);
typedef float** (*vorbis_analysis_buffer_proc)(vorbis_dsp_state*, int);
typedef int (*vorbis_analysis_blockout_proc)(vorbis_dsp_state*,vorbis_block*);
typedef int (*vorbis_analysis_proc)(vorbis_block*, ogg_packet*);
typedef void (*vorbis_analysis_wrote_proc)(vorbis_dsp_state*, int);

typedef int (*vorbis_bitrate_addblock_proc)(vorbis_block*);
typedef int (*vorbis_bitrate_flushpacket_proc)(vorbis_dsp_state*, ogg_packet*);

typedef void (*vorbis_block_init_proc)(vorbis_dsp_state*, vorbis_block*);
typedef void (*vorbis_block_clear_proc)(vorbis_block*);

typedef void (*vorbis_dsp_clear_proc)(vorbis_dsp_state*);

#endif /* __FILE_FMT_VORBIS_H */

