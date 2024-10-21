/*
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2012-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __FILE_EXT_MP3_H
#define __FILE_EXT_MP3_H 1

#if 1
# define PDMP3_HEADER_ONLY
# include <3rdparty/pdmp3.c>
#else
# define MINIMP3_IMPLEMENTATION
# include <3rdparty/minimp3.h>
#endif

enum mp3_parms {
  MP3_VERBOSE = 0, MP3_FLAGS,
  MP3_ADD_FLAGS, MP3_FORCE_RATE,
  MP3_DOWN_SAMPLE, MP3_RVA,
  MP3_DOWNSPEED, MP3_UPSPEED,
  MP3_START_FRAME, MP3_DECODE_FRAMES,
  MP3_ICY_INTERVAL, MP3_OUTSCALE,
  MP3_TIMEOUT, MP3_REMOVE_FLAGS,
  MP3_RESYNC_LIMIT, MP3_INDEX_SIZE,
  MP3_PREFRAMES, MP3_FEEDPOOL,
  MP3_FEEDBUFFER
};

enum mp3_param_flags {
  MP3_FORCE_MONO = 0x7, MP3_MONO_LEFT = 0x1,
  MP3_MONO_RIGHT = 0x2, MP3_MONO_MIX = 0x4,
  MP3_FORCE_STEREO = 0x8, MP3_FORCE_8BIT = 0x10,
  MP3_QUIET = 0x20, MP3_GAPLESS = 0x40,
  MP3_NO_RESYNC = 0x80, MP3_SEEKBUFFER = 0x100,
  MP3_FUZZY = 0x200, MP3_FORCE_FLOAT = 0x400,
  MP3_PLAIN_ID3TEXT = 0x800, MP3_IGNORE_STREAMLENGTH = 0x1000,
  MP3_SKIP_ID3V2 = 0x2000, MP3_IGNORE_INFOFRAME = 0x4000,
  MP3_AUTO_RESAMPLE = 0x8000,
  MP3_PICTURE = 1<<17
};

enum mp3_param_rva {
  MP3_RVA_OFF = 0, MP3_RVA_MIX = 1,
  MP3_RVA_ALBUM = 2, MP3_RVA_MAX = MP3_RVA_ALBUM
};

enum mpg123_mode {
  MPG123_M_STEREO = 0, MPG123_M_JOINT,
  MPG123_M_DUAL, MPG123_M_MONO
};

enum mpg123_version {
  MPG123_1_0 = 0, MPG123_2_0,
  MPG123_2_5
};

enum mpg123_flags {
  MPG123_CRC = 0x1, MPG123_COPYRIGHT = 0x2,
  MPG123_PRIVATE = 0x4, MPG123_ORIGINAL = 0x8
};

enum mpg123_vbr {
  MPG123_CBR =0, MPG123_VBR,
  MPG123_ABR
};

enum mp3_feature_set {
  MP3_FEATURE_ABI_UTF8OPEN = 0, MP3_FEATURE_OUTPUT_8BIT,
  MP3_FEATURE_OUTPUT_16BIT, MP3_FEATURE_OUTPUT_32BIT,
  MP3_FEATURE_INDEX, MP3_FEATURE_PARSE_ID3V2,
  MP3_FEATURE_DECODE_LAYER1, MP3_FEATURE_DECODE_LAYER2,
  MP3_FEATURE_DECODE_LAYER3, MP3_FEATURE_DECODE_ACCURATE,
  MP3_FEATURE_DECODE_DOWNSAMPLE, MP3_FEATURE_DECODE_NTOM,
  MP3_FEATURE_PARSE_ICY, MP3_FEATURE_TIMEOUT_READ
};

enum mp3_enc_enum {
  MP3_ENC_8 = 0x00f, MP3_ENC_16 = 0x040,
  MP3_ENC_24 = 0x4000, MP3_ENC_32 = 0x100,
  MP3_ENC_SIGNED = 0x080, MP3_ENC_FLOAT = 0xe00,
  MP3_ENC_SIGNED_16 = (MP3_ENC_16|MP3_ENC_SIGNED|0x10),
  MP3_ENC_UNSIGNED_16 = (MP3_ENC_16|0x20),
  MP3_ENC_UNSIGNED_8 = 0x01,
  MP3_ENC_SIGNED_8 = (MP3_ENC_SIGNED|0x02),
  MP3_ENC_ULAW_8 = 0x04, MP3_ENC_ALAW_8 = 0x08,
  MP3_ENC_SIGNED_32 = MP3_ENC_32|MP3_ENC_SIGNED|0x1000,
  MP3_ENC_UNSIGNED_32 = MP3_ENC_32|0x2000,
  MP3_ENC_SIGNED_24 = MP3_ENC_24|MP3_ENC_SIGNED|0x1000,
  MP3_ENC_UNSIGNED_24 = MP3_ENC_24|0x2000,
  MP3_ENC_FLOAT_32 = 0x200, MP3_ENC_FLOAT_64 = 0x400,
  MP3_ENC_ANY
};

enum mp3_channelcount {
  MP3_MONO = 1,
  MP3_STEREO = 2
};

enum mp3_errors
{
  MP3_DONE=-12,
  MP3_NEW_FORMAT=-11,
  MP3_NEED_MORE=-10,
  MP3_ERR=-1,
  MP3_OK=0,
  MP3_BAD_OUTFORMAT,
  MP3_BAD_CHANNEL,
  MP3_BAD_RATE,
  MP3_ERR_16TO8TABLE,
  MP3_BAD_PARAM,
  MP3_BAD_BUFFER,
  MP3_OUT_OF_MEM,
  MP3_NOT_INITIALIZED,
  MP3_BAD_DECODER,
  MP3_BAD_HANDLE,
  MP3_NO_BUFFERS,
  MP3_BAD_RVA,
  MP3_NO_GAPLESS,
  MP3_NO_SPACE,
  MP3_BAD_TYPES,
  MP3_BAD_BAND,
  MP3_ERR_NULL,
  MP3_ERR_READER,
  MP3_NO_SEEK_FROM_END,
  MP3_BAD_WHENCE,
  MP3_NO_TIMEOUT,
  MP3_BAD_FILE,
  MP3_NO_SEEK,
  MP3_NO_READER,
  MP3_BAD_PARS,
  MP3_BAD_INDEX_PAR,
  MP3_OUT_OF_SYNC,
  MP3_RESYNC_FAIL,
  MP3_NO_8BIT,
  MP3_BAD_ALIGN,
  MP3_NULL_BUFFER,
  MP3_NO_RELSEEK,
  MP3_NULL_POINTER,
  MP3_BAD_KEY,
  MP3_NO_INDEX,
  MP3_INDEX_FAIL,
  MP3_BAD_DECODER_SETUP,
  MP3_MISSING_FEATURE,
  MP3_BAD_VALUE,
  MP3_LSEEK_FAILED,
  MP3_BAD_CUSTOM_IO,
  MP3_LFS_OVERFLOW,
  MP3_INT_OVERFLOW
};

#define	MP3_ID3	0x3
#define MP3_NEW_ID3	0x1

#define LAME_MAXALBUMART	(128 * 1024)
#define LAME_MAXMP3BUFFER	(16384 + LAME_MAXALBUMART)

struct mp3_frameinfo
{
  enum mpg123_version version;
  int layer;
  long rate;
  enum mpg123_mode mode;
  int mode_ext;
  int framesize;
  enum mpg123_flags flags;
  int emphasis;
  int bitrate;
  int abr_rate;
  enum mpg123_vbr vbr;
};

typedef struct {
   char* p;
   size_t size;
   size_t fill;
} mp3_string;

typedef struct {
   char lang[3];
   char id[4];
   mp3_string description;
   mp3_string text;
} mp3_text;

typedef struct {
   char type;
   mp3_string description;
   mp3_string mime_type;
   size_t size;
   unsigned char *data;
} mp3_picture;

typedef struct {
   unsigned char version;
   mp3_string *title;
   mp3_string *artist;
   mp3_string *album;
   mp3_string *year;
   mp3_string *genre;
   mp3_string *comment;
   /* Encountered ID3v2 fields are appended to these lists. */
   mp3_text *comment_list;
   size_t comments;
   mp3_text *text;
   size_t texts;
   mp3_text *extra;
   size_t extras;
   mp3_picture *picture;
   size_t pictures;
} mp3_id3v2;

typedef struct {
   char tag[3];
   char title[30];
   char artist[30];
   char album[30];
   char year[4];
   char comment[30];
   unsigned char genre;
} mp3_id3v1;

typedef int (*mp3_init_proc)(void);
typedef void (*mp3_exit_proc)(void);
typedef void* (*mp3_new_proc)(const char*, int*);
typedef void (*mp3_delete_proc)(void*);
typedef int (*mp3_open_fd_proc)(void*, int);
typedef int (*mp3_open_feed_proc)(void*);
typedef int (*mp3_read_proc)(void*, unsigned char*, size_t, size_t*);
typedef int (*mp3_feed_proc)(void*, const unsigned char*,  	size_t);
typedef int (*mp3_decode_proc)(void*, const unsigned char*, size_t, unsigned char*, size_t, size_t*);
typedef int (*mp3_param_proc)(void*, enum mp3_parms, long, double);
typedef int (*mp3_getparam_proc)(void*, enum mp3_parms, long*, double*);
typedef int (*mp3_feature_proc)(const enum mp3_feature_set);
typedef int (*mp3_format_proc)(void*, long, int, int);
typedef int (*mp3_info_proc)(void*, struct mp3_frameinfo*);
typedef int (*mp3_getformat_proc)(void*, long*, int*, int*);
typedef int (*mp3_set_filesize_proc)(void*, off_t);
typedef off_t (*mp3_length_proc)(void*);
typedef off_t (*mp3_feedseek_proc)(void*, off_t, int, off_t*);
typedef int (*mp3_meta_check_proc)(void*);
typedef int (*mp3_id3_proc)(void*, mp3_id3v1**, mp3_id3v2**);

typedef const char* (*mp3_plain_strerror_proc)(int);

/** libmpg123 */

typedef int (*mpg123_init_proc)(void);
typedef void (*mpg123_exit_proc)(void);
typedef void* (*mpg123_new_proc)(const char*, int*);
typedef void (*mpg123_delete_proc)(void*);
typedef int (*mpg123_open_fd_proc)(void*, int);
typedef int (*mpg123_open_feed_proc)(void*);
typedef int (*mpg123_read_proc)(void*, unsigned char*, size_t, size_t*);
typedef int (*mpg123_feed_proc)(void*, const unsigned char*,       size_t);
typedef int (*mpg123_decode_proc)(void*, const unsigned char*, size_t, unsigned char*, size_t, size_t*);
typedef int (*mpg123_param_proc)(void*, enum mp3_parms, long, double);
typedef int (*mpg123_getparam_proc)(void*, enum mp3_parms, long*, double*);
typedef int (*mpg123_feature_proc)(const enum mp3_feature_set);
typedef int (*mpg123_format_proc)(void*, long, int, int);
typedef int (*mpg123_info_proc)(void*, struct mp3_frameinfo*);
typedef int (*mpg123_getformat_proc)(void*, long*, int*, int*);
typedef int (*mpg123_set_filesize_proc)(void*, off_t);
typedef off_t (*mpg123_length_proc)(void*);
typedef off_t (*mpg123_feedseek_proc)(void*, off_t, int, off_t*);
typedef int (*mpg123_meta_check_proc)(void*);
typedef int (*mpg123_id3_proc)(void*, mp3_id3v1**, mp3_id3v2**);

typedef const char* (*mpg123_plain_strerror_proc)(int);

/** lame */

typedef enum vbr_mode_e {
  vbr_off=0,
  vbr_mt,               /* obsolete, same as vbr_mtrh */
  vbr_rh,
  vbr_abr,
  vbr_mtrh,
  vbr_max_indicator,    /* Don't use this! It's used for sanity checks.       */
  vbr_default=vbr_mtrh  /* change this to change the default VBR mode of LAME */
} vbr_mode;

typedef enum MPEG_mode_e {
   STEREO = 0,
   JOINT_STEREO,
   DUAL_CHANNEL,
   MONO,
   NOT_SET,
   MAX_INDICATOR
} MPEG_mode;

typedef void* (*lame_init_proc)(void);
typedef int (*lame_init_params_proc)(void*);
typedef int (*lame_close_proc)(void*);
typedef int (*lame_set_num_samples_proc)(void*, int);
typedef int (*lame_set_in_samplerate_proc)(void*, int);
typedef int (*lame_set_num_channels_proc)(void*, int);
typedef int (*lame_set_quality_proc)(void*, int);
typedef int (*lame_set_brate_proc)(void*, int);
typedef int (*lame_set_VBR_proc)(void*, enum vbr_mode_e);
typedef int (*lame_set_VBR_quality_proc)(void*, float);
typedef int (*lame_set_mode_proc)(void*, enum MPEG_mode_e);
typedef int (*lame_set_scale_proc)(void*, float);

typedef int (*lame_encode_buffer_interleaved_proc)(void*, short int[], int, unsigned char*, int);
typedef int (*lame_encode_buffer_proc)(void*, short int[], short int[], int, unsigned char*, int);
typedef int (*lame_encode_flush_proc)(void*, unsigned char*, int);
typedef int (*lame_encode_flush_nogap_proc)(void*, unsigned char*, int);
typedef void (*lame_mp3_tags_fid_proc)(void*, void*);

typedef void (*lame_set_write_id3tag_automatic_proc)(void*, int);
typedef size_t (*lame_get_lametag_frame_proc)(const void*, void*, size_t);
typedef size_t (*lame_get_id3v2_tag_proc)(void*, void*, size_t);
typedef size_t (*lame_get_id3v1_tag_proc)(void*, void*, size_t);
typedef void (*id3tag_add_v2_proc)(void*);
typedef void (*id3tag_set_title_proc)(void*, const char*);
typedef void (*id3tag_set_artist_proc)(void*, const char*);
typedef void (*id3tag_set_album_proc)(void*, const char*);
typedef void (*id3tag_set_year_proc)(void*, const char*);
typedef void (*id3tag_set_comment_proc)(void*, const char*);
typedef void (*id3tag_set_track_proc)(void*, const char*);
typedef void (*id3tag_set_genre_proc)(void*, const char*);
typedef void (*id3tag_set_albumart_proc)(void*, const char*, size_t);

typedef void (*lame_report_function)(const char*, va_list);
typedef int (*lame_set_errorf_proc)(void*, lame_report_function);

/* lame */

#endif /* __FILE_EXT_MP3_H */

