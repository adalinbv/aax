/*
 * Copyright 2012-2015 by Erik Hofman.
 * Copyright 2012-2015 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef __FILE_EXT_MPG123_H
#define __FILE_EXT_MPG123_H 1

/** libmpg123 */

enum    mpg123_parms {
  MPG123_VERBOSE = 0, MPG123_FLAGS,
  MPG123_ADD_FLAGS, MPG123_FORCE_RATE,
  MPG123_DOWN_SAMPLE, MPG123_RVA,
  MPG123_DOWNSPEED, MPG123_UPSPEED,
  MPG123_START_FRAME, MPG123_DECODE_FRAMES,
  MPG123_ICY_INTERVAL, MPG123_OUTSCALE,
  MPG123_TIMEOUT, MPG123_REMOVE_FLAGS,
  MPG123_RESYNC_LIMIT, MPG123_INDEX_SIZE,
  MPG123_PREFRAMES, MPG123_FEEDPOOL,
  MPG123_FEEDBUFFER
};

enum    mpg123_param_flags {
  MPG123_FORCE_MONO = 0x7, MPG123_MONO_LEFT = 0x1,
  MPG123_MONO_RIGHT = 0x2, MPG123_MONO_MIX = 0x4,
  MPG123_FORCE_STEREO = 0x8, MPG123_FORCE_8BIT = 0x10,
  MPG123_QUIET = 0x20, MPG123_GAPLESS = 0x40,
  MPG123_NO_RESYNC = 0x80, MPG123_SEEKBUFFER = 0x100,
  MPG123_FUZZY = 0x200, MPG123_FORCE_FLOAT = 0x400,
  MPG123_PLAIN_ID3TEXT = 0x800, MPG123_IGNORE_STREAMLENGTH = 0x1000,
  MPG123_SKIP_ID3V2 = 0x2000, MPG123_IGNORE_INFOFRAME = 0x4000,
  MPG123_AUTO_RESAMPLE = 0x8000,
  MPG123_PICTURE = 1<<17
};

enum    mpg123_param_rva {
  MPG123_RVA_OFF = 0, MPG123_RVA_MIX = 1,
  MPG123_RVA_ALBUM = 2, MPG123_RVA_MAX = MPG123_RVA_ALBUM
};

enum    mpg123_feature_set {
  MPG123_FEATURE_ABI_UTF8OPEN = 0, MPG123_FEATURE_OUTPUT_8BIT,
  MPG123_FEATURE_OUTPUT_16BIT, MPG123_FEATURE_OUTPUT_32BIT,
  MPG123_FEATURE_INDEX, MPG123_FEATURE_PARSE_ID3V2,
  MPG123_FEATURE_DECODE_LAYER1, MPG123_FEATURE_DECODE_LAYER2,
  MPG123_FEATURE_DECODE_LAYER3, MPG123_FEATURE_DECODE_ACCURATE,
  MPG123_FEATURE_DECODE_DOWNSAMPLE, MPG123_FEATURE_DECODE_NTOM,
  MPG123_FEATURE_PARSE_ICY, MPG123_FEATURE_TIMEOUT_READ
};

enum   mpg123_enc_enum {
  MPG123_ENC_8 = 0x00f, MPG123_ENC_16 = 0x040,
  MPG123_ENC_24 = 0x4000, MPG123_ENC_32 = 0x100,
  MPG123_ENC_SIGNED = 0x080, MPG123_ENC_FLOAT = 0xe00,
  MPG123_ENC_SIGNED_16 = (MPG123_ENC_16|MPG123_ENC_SIGNED|0x10),
  MPG123_ENC_UNSIGNED_16 = (MPG123_ENC_16|0x20),
  MPG123_ENC_UNSIGNED_8 = 0x01,
  MPG123_ENC_SIGNED_8 = (MPG123_ENC_SIGNED|0x02),
  MPG123_ENC_ULAW_8 = 0x04, MPG123_ENC_ALAW_8 = 0x08,
  MPG123_ENC_SIGNED_32 = MPG123_ENC_32|MPG123_ENC_SIGNED|0x1000,
  MPG123_ENC_UNSIGNED_32 = MPG123_ENC_32|0x2000,
  MPG123_ENC_SIGNED_24 = MPG123_ENC_24|MPG123_ENC_SIGNED|0x1000,
  MPG123_ENC_UNSIGNED_24 = MPG123_ENC_24|0x2000,
  MPG123_ENC_FLOAT_32 = 0x200, MPG123_ENC_FLOAT_64 = 0x400,
  MPG123_ENC_ANY
};

enum   mpg123_channelcount {
  MPG123_MONO = 1,
  MPG123_STEREO = 2
};

enum mpg123_errors
{
  MPG123_DONE=-12,
  MPG123_NEW_FORMAT=-11,
  MPG123_NEED_MORE=-10,
  MPG123_ERR=-1,
  MPG123_OK=0,
  MPG123_BAD_OUTFORMAT,
  MPG123_BAD_CHANNEL,
  MPG123_BAD_RATE,
  MPG123_ERR_16TO8TABLE,
  MPG123_BAD_PARAM,
  MPG123_BAD_BUFFER,
  MPG123_OUT_OF_MEM,
  MPG123_NOT_INITIALIZED,
  MPG123_BAD_DECODER,
  MPG123_BAD_HANDLE,
  MPG123_NO_BUFFERS,
  MPG123_BAD_RVA,
  MPG123_NO_GAPLESS,
  MPG123_NO_SPACE,
  MPG123_BAD_TYPES,
  MPG123_BAD_BAND,
  MPG123_ERR_NULL,
  MPG123_ERR_READER,
  MPG123_NO_SEEK_FROM_END,
  MPG123_BAD_WHENCE,
  MPG123_NO_TIMEOUT,
  MPG123_BAD_FILE,
  MPG123_NO_SEEK,
  MPG123_NO_READER,
  MPG123_BAD_PARS,
  MPG123_BAD_INDEX_PAR,
  MPG123_OUT_OF_SYNC,
  MPG123_RESYNC_FAIL,
  MPG123_NO_8BIT,
  MPG123_BAD_ALIGN,
  MPG123_NULL_BUFFER,
  MPG123_NO_RELSEEK,
  MPG123_NULL_POINTER,
  MPG123_BAD_KEY,
  MPG123_NO_INDEX,
  MPG123_INDEX_FAIL,
  MPG123_BAD_DECODER_SETUP,
  MPG123_MISSING_FEATURE,
  MPG123_BAD_VALUE,
  MPG123_LSEEK_FAILED,
  MPG123_BAD_CUSTOM_IO,
  MPG123_LFS_OVERFLOW,
  MPG123_INT_OVERFLOW
};

#define	MPG123_ID3	0x3
#define MPG123_NEW_ID3	0x1

typedef struct {
   char* p;
   size_t size;
   size_t fill;
} mpg123_string;

typedef struct {
   char lang[3];
   char id[4];
   mpg123_string description;
   mpg123_string text;
} mpg123_text;

typedef struct {
   char type;
   mpg123_string description;
   mpg123_string mime_type;
   size_t size;
   unsigned char *data;
} mpg123_picture;

typedef struct {
   unsigned char version;
   mpg123_string *title;
   mpg123_string *artist;
   mpg123_string *album;
   mpg123_string *year;
   mpg123_string *genre;
   mpg123_string *comment;
   /* Encountered ID3v2 fields are appended to these lists. */
   mpg123_text *comment_list;
   size_t comments;
   mpg123_text *text;
   size_t texts;
   mpg123_text *extra;
   size_t extras;
   mpg123_picture *picture;
   size_t pictures;
} mpg123_id3v2;

typedef struct {
   char tag[3];
   char title[30];
   char artist[30];
   char album[30];
   char year[4];
   char comment[30];
   unsigned char genre;
} mpg123_id3v1;

typedef int (*mpg123_init_proc)(void);
typedef void (*mpg123_exit_proc)(void);
typedef void* (*mpg123_new_proc)(const char*, int*);
typedef void (*mpg123_delete_proc)(void*);
typedef int (*mpg123_open_fd_proc)(void*, int);
typedef int (*mpg123_open_feed_proc)(void*);
typedef int (*mpg123_read_proc)(void*, unsigned char*, size_t, size_t*);
typedef int (*mpg123_feed_proc)(void*, const unsigned char*,  	size_t);
typedef int (*mpg123_decode_proc)(void*, const unsigned char*, size_t, unsigned char*, size_t, size_t*);
typedef int (*mpg123_param_proc)(void*, enum mpg123_parms, long, double);
typedef int (*mpg123_getparam_proc)(void*, enum mpg123_parms, long*, double*);
typedef int (*mpg123_feature_proc)(const enum mpg123_feature_set);
typedef int (*mpg123_format_proc)(void*, long, int, int);
typedef int (*mpg123_getformat_proc)(void*, long*, int*, int*);
typedef int (*mpg123_set_filesize_proc)(void*, off_t);
typedef off_t (*mpg123_length_proc)(void*);
typedef off_t (*mpg123_feedseek_proc)(void*, off_t, int, off_t*);
typedef int (*mpg123_meta_check_proc)(void*);
typedef int (*mpg123_id3_proc)(void*, mpg123_id3v1**, mpg123_id3v2**);

/* libmpg123 */

/** lame */

typedef enum vbr_mode_e {
  VBR_OFF=0,
  VBR_MT,		/* obsolete, same as vbr_mtrh */
  VBR_RH,
  VBR_ABR,
  VBR_MTRH,
  VBR_MAX_INDICATOR,	/* Don't use this! It's used for sanity checks.       */
  VBR_DEFAULT=VBR_MTRH	/* change this to change the default VBR mode of LAME */
} vbr_mode;

typedef void* (*lame_init_proc)(void);
typedef void* (*lame_init_params_proc)(void*);
typedef int (*lame_close_proc)(void*);
typedef int (*lame_set_num_samples_proc)(void*, int);
typedef int (*lame_set_in_samplerate_proc)(void*, int);
typedef int (*lame_set_num_channels_proc)(void*, int);
typedef int (*lame_set_quality_proc)(void*, int);
typedef int (*lame_set_brate_proc)(void*, int);

typedef int (*lame_set_VBR_proc)(void*, enum vbr_mode_e);
typedef int (*lame_set_VBR_quality_proc)(void*, float);

typedef int (*lame_encode_buffer_interleaved_proc)(void*, short int[], int, unsigned char*, int);
typedef int (*lame_encode_buffer_proc)(void*, short int[], short int[], int, unsigned char*, int);
typedef int (*lame_encode_flush_proc)(void*, unsigned char*, int);
typedef void (*lame_mp3_tags_fid_proc)(void*, void*);

/* lame */

#endif /* __FILE_EXT_MPG123_H */

