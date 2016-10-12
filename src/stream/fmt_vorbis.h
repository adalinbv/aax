/*
 * Copyright 2016-2017 by Erik Hofman.
 * Copyright 2016-2017. by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef __FILE_FMT_VORBIS_H
#define __FILE_FMT_VORBIS_H 1

typedef struct vorbis_info{
  int version;
  int channels;
  long rate;
  
  long bitrate_upper;
  long bitrate_nominal;
  long bitrate_lower;
  long bitrate_window;

  void *codec_setup;

} vorbis_info;

typedef struct vorbis_comment{
  /* unlimited user comment fields. */
  char **user_comments;
  int  *comment_lengths;
  int  comments;
  char *vendor;

} vorbis_comment;

typedef struct vorbis_dsp_state{
  int analysisp;
  vorbis_info *vi;

  float **pcm;
  float **pcmret;
  int      pcm_storage;
  int      pcm_current;
  int      pcm_returned;

  int  preextrapolate;
  int  eofflag;

  long lW;
  long W;
  long nW;
  long centerW;

  ogg_int64_t granulepos;
  ogg_int64_t sequence;

  ogg_int64_t glue_bits;
  ogg_int64_t time_bits;
  ogg_int64_t floor_bits;
  ogg_int64_t res_bits;

  void       *backend_state;
} vorbis_dsp_state;

typedef struct vorbis_block{
  /* necessary stream state for linking to the framing abstraction */
  float  **pcm;       /* this is a pointer into local storage */
  oggpack_buffer opb;

  long  lW;
  long  W;
  long  nW;
  int   pcmend;
  int   mode;

  int         eofflag;
  ogg_int64_t granulepos;
  ogg_int64_t sequence;
  vorbis_dsp_state *vd; /* For read-only access of configuration */

  /* local storage to avoid remallocing; it's up to the mapping to
     structure it */
  void               *localstore;
  long                localtop;
  long                localalloc;
  long                totaluse;
  struct alloc_chain *reap;

  /* bitmetrics for the frame */
  long glue_bits;
  long time_bits;
  long floor_bits;
  long res_bits;

  void *internal;

} vorbis_block;


typedef void (*vorbis_info_init_proc)(vorbis_info*);
typedef void (*vorbis_comment_init_proc)(vorbis_comment*);
typedef int (*vorbis_block_init_proc)(vorbis_dsp_state*, vorbis_block*);
typedef int (*vorbis_synthesis_init_proc)(vorbis_dsp_state*, vorbis_info *);

typedef void (*vorbis_info_clear_proc)(vorbis_info*);
typedef void (*vorbis_comment_clear_proc)(vorbis_comment*);
typedef int  (*vorbis_block_clear_proc)(vorbis_block*);
typedef void (*vorbis_dsp_clear_proc)(vorbis_dsp_state*);

typedef int (*vorbis_synthesis_proc)(vorbis_block*, ogg_packet*);
typedef int (*vorbis_synthesis_headerin_proc)(vorbis_info*, vorbis_comment*, ogg_packet*);
typedef int (*vorbis_synthesis_blockin_proc)(vorbis_dsp_state*, vorbis_block*);
typedef int (*vorbis_synthesis_pcmout_proc)(vorbis_dsp_state*, float ***);
typedef int (*vorbis_synthesis_read_proc)(vorbis_dsp_state*, int);


#endif /* __FILE_FMT_VORBIS_H */

