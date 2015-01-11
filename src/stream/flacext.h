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

#ifndef __FILE_EXT_FLAC_H
#define __FILE_EXT_FLAC_H 1

#include <base/types.h>

// https://xiph.org/flac/api/group__flac__stream__decoder.html
// FLAC__bool: int
// FLAC__byte: uint8_t
// FLAC__int32: int32_t
// FLAC__uint32: uint32_t
// FLAC__uint64: uint64_t

#define	FLAC__MAX_METADATA_TYPE_CODE	(126u)
#define FLAC__MAX_FIXED_ORDER 		(4u)
#define FLAC__MAX_LPC_ORDER		(32u)
#define FLAC__MAX_CHANNELS		(8u)

extern const char *const FLAC__StreamDecoderErrorStatusString[];

typedef enum
{
  FLAC__STREAM_DECODER_READ_STATUS_CONTINUE = 0,
  FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM,
  FLAC__STREAM_DECODER_READ_STATUS_ABORT
} FLAC__StreamDecoderReadStatus;

typedef enum
{
  FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE = 0,
  FLAC__STREAM_DECODER_WRITE_STATUS_ABORT
} FLAC__StreamDecoderWriteStatus;

typedef enum
{
  FLAC__METADATA_TYPE_STREAMINFO = 0,
  FLAC__METADATA_TYPE_PADDING,
  FLAC__METADATA_TYPE_APPLICATION,
  FLAC__METADATA_TYPE_SEEKTABLE,
  FLAC__METADATA_TYPE_VORBIS_COMMENT,
  FLAC__METADATA_TYPE_CUESHEET,
  FLAC__METADATA_TYPE_PICTURE,
  FLAC__METADATA_TYPE_UNDEFINED,
  FLAC__MAX_METADATA_TYPE = FLAC__MAX_METADATA_TYPE_CODE
} FLAC__MetadataType;

typedef enum {
  FLAC__STREAM_METADATA_PICTURE_TYPE_OTHER = 0,
  FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON_STANDARD,
  FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON,
  FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER,
  FLAC__STREAM_METADATA_PICTURE_TYPE_BACK_COVER,
  FLAC__STREAM_METADATA_PICTURE_TYPE_LEAFLET_PAGE,
  FLAC__STREAM_METADATA_PICTURE_TYPE_MEDIA,
  FLAC__STREAM_METADATA_PICTURE_TYPE_LEAD_ARTIST,
  FLAC__STREAM_METADATA_PICTURE_TYPE_ARTIST,
  FLAC__STREAM_METADATA_PICTURE_TYPE_CONDUCTOR,
  FLAC__STREAM_METADATA_PICTURE_TYPE_BAND,
  FLAC__STREAM_METADATA_PICTURE_TYPE_COMPOSER,
  FLAC__STREAM_METADATA_PICTURE_TYPE_LYRICIST,
  FLAC__STREAM_METADATA_PICTURE_TYPE_RECORDING_LOCATION,
  FLAC__STREAM_METADATA_PICTURE_TYPE_DURING_RECORDING,
  FLAC__STREAM_METADATA_PICTURE_TYPE_DURING_PERFORMANCE,
  FLAC__STREAM_METADATA_PICTURE_TYPE_VIDEO_SCREEN_CAPTURE,
  FLAC__STREAM_METADATA_PICTURE_TYPE_FISH,
  FLAC__STREAM_METADATA_PICTURE_TYPE_ILLUSTRATION,
  FLAC__STREAM_METADATA_PICTURE_TYPE_BAND_LOGOTYPE,
  FLAC__STREAM_METADATA_PICTURE_TYPE_PUBLISHER_LOGOTYPE,
  FLAC__STREAM_METADATA_PICTURE_TYPE_UNDEFINED
} FLAC__StreamMetadata_Picture_Type;

typedef enum
{
  FLAC__SUBFRAME_TYPE_CONSTANT = 0,
  FLAC__SUBFRAME_TYPE_VERBATIM,
  FLAC__SUBFRAME_TYPE_FIXED,
  FLAC__SUBFRAME_TYPE_LPC
} FLAC__SubframeType;

typedef enum
{
  FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER = 0,
  FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER
} FLAC__FrameNumberType;

typedef enum
{
  FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT = 0,
  FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE,
  FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE,
  FLAC__CHANNEL_ASSIGNMENT_MID_SIDE
} FLAC__ChannelAssignment;

typedef enum
{
  FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE = 0,
  FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2,
} FLAC__EntropyCodingMethodType;

typedef enum
{
  FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC = 0,
  FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER,
  FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH,
  FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM
} FLAC__StreamDecoderErrorStatus;

typedef enum
{
  FLAC__STREAM_DECODER_INIT_STATUS_OK = 0,
  FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER,
  FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS,
  FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR,
  FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE,
  FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED
} FLAC__StreamDecoderInitStatus;

typedef enum
{
  FLAC__STREAM_DECODER_SEARCH_FOR_METADATA = 0,
  FLAC__STREAM_DECODER_READ_METADATA, 
  FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC,
  FLAC__STREAM_DECODER_READ_FRAME,
  FLAC__STREAM_DECODER_END_OF_STREAM,
  FLAC__STREAM_DECODER_OGG_ERROR,
  FLAC__STREAM_DECODER_SEEK_ERROR,
  FLAC__STREAM_DECODER_ABORTED,
  FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR,
  FLAC__STREAM_DECODER_UNINITIALIZED
} FLAC__StreamDecoderState;

typedef struct
{
  unsigned min_blocksize, max_blocksize;
  unsigned min_framesize, max_framesize;
  unsigned sample_rate;
  unsigned channels;
  unsigned bits_per_sample;
  uint64_t total_samples;
  uint8_t md5sum[16];
} FLAC__StreamMetadata_StreamInfo;

typedef struct
{
  int dummy;
} FLAC__StreamMetadata_Padding;

typedef struct
{
  uint8_t id[4];
  uint8_t *data;
} FLAC__StreamMetadata_Application;

typedef struct
{
  uint64_t sample_number;
  uint64_t stream_offset;
  unsigned frame_samples;
} FLAC__StreamMetadata_SeekPoint;
typedef struct
{
  unsigned num_points;
  FLAC__StreamMetadata_SeekPoint *points;
} FLAC__StreamMetadata_SeekTable;

typedef struct
{
  uint32_t length;
  uint8_t *entry;
} FLAC__StreamMetadata_VorbisComment_Entry;
typedef struct
{
  FLAC__StreamMetadata_VorbisComment_Entry vendor_string;
  uint32_t num_comments;
  FLAC__StreamMetadata_VorbisComment_Entry *comments;
} FLAC__StreamMetadata_VorbisComment;

typedef struct
{
  uint64_t offset;
  uint8_t number;
} FLAC__StreamMetadata_CueSheet_Index;
typedef struct
{
  uint64_t offset;
  uint8_t number;
  char isrc[13];
  unsigned type:1;
  unsigned pre_emphasis:1;
  uint8_t num_indices;
  FLAC__StreamMetadata_CueSheet_Index *indices;
} FLAC__StreamMetadata_CueSheet_Track;
typedef struct
{
  char media_catalog_number[129];
  uint64_t lead_in;
  int is_cd;
  unsigned num_tracks;
  FLAC__StreamMetadata_CueSheet_Track *tracks;
} FLAC__StreamMetadata_CueSheet;

typedef struct
{
  FLAC__StreamMetadata_Picture_Type type;
  char *mime_type;
  uint8_t *description;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t colors;
  uint32_t data_length;
  uint8_t *data;
} FLAC__StreamMetadata_Picture;

typedef struct
{
  uint8_t *data;
} FLAC__StreamMetadata_Unknown;

typedef struct
{
  FLAC__MetadataType type;
  int is_last;
  unsigned length;
  union {
    FLAC__StreamMetadata_StreamInfo stream_info;
    FLAC__StreamMetadata_Padding padding;
    FLAC__StreamMetadata_Application application;
    FLAC__StreamMetadata_SeekTable seek_table;
    FLAC__StreamMetadata_VorbisComment vorbis_comment;
    FLAC__StreamMetadata_CueSheet cue_sheet;
    FLAC__StreamMetadata_Picture picture;
    FLAC__StreamMetadata_Unknown unknown;
  } data;
} FLAC__StreamMetadata;

typedef struct
{
  unsigned blocksize;
  unsigned sample_rate;
  unsigned channels;
  FLAC__ChannelAssignment channel_assignment;
  unsigned bits_per_sample;
  FLAC__FrameNumberType number_type;
  union {
    uint32_t frame_number;
    uint64_t sample_number;
  } number;
  uint8_t crc;
} FLAC__FrameHeader;
typedef struct
{
  int32_t value;
} FLAC__Subframe_Constant;
typedef struct
{
  unsigned *parameters;
  unsigned *raw_bits;
  unsigned capacity_by_order;
} FLAC__EntropyCodingMethod_PartitionedRiceContents;
typedef struct
{
  unsigned order;
  const FLAC__EntropyCodingMethod_PartitionedRiceContents *contents;
} FLAC__EntropyCodingMethod_PartitionedRice;
typedef struct
{
  FLAC__EntropyCodingMethodType type;
  union {
    FLAC__EntropyCodingMethod_PartitionedRice partitioned_rice;
  } data;
} FLAC__EntropyCodingMethod;
typedef struct
{
  FLAC__EntropyCodingMethod entropy_coding_method;
  unsigned order;
  int32_t warmup[FLAC__MAX_FIXED_ORDER];
  const int32_t *residual;
} FLAC__Subframe_Fixed;
typedef struct
{
  FLAC__EntropyCodingMethod entropy_coding_method;
  unsigned order;
  unsigned qlp_coeff_precision;
  int quantization_level;
  int32_t qlp_coeff[FLAC__MAX_LPC_ORDER];
  int32_t warmup[FLAC__MAX_LPC_ORDER];
  const int32_t *residual;
} FLAC__Subframe_LPC;
typedef struct
{
  const int32_t *data;
} FLAC__Subframe_Verbatim;
typedef struct
{
  FLAC__SubframeType type;
  union {
    FLAC__Subframe_Constant constant;
    FLAC__Subframe_Fixed fixed;
    FLAC__Subframe_LPC lpc;
    FLAC__Subframe_Verbatim verbatim;
  } data;
  unsigned wasted_bits;
} FLAC__Subframe;
typedef struct
{
  uint16_t crc;
} FLAC__FrameFooter;
typedef struct
{
  FLAC__FrameHeader header;
  FLAC__Subframe subframes[FLAC__MAX_CHANNELS];
  FLAC__FrameFooter footer;
} FLAC__Frame;

typedef FLAC__StreamDecoderReadStatus (*FLAC__StreamDecoderReadCallback)(const void*, uint8_t buffer[], size_t*, void*);
typedef FLAC__StreamDecoderWriteStatus (*FLAC__StreamDecoderWriteCallback)(const void*, const FLAC__Frame *frame, const int32_t *const buffer[], void*);
typedef int (*FLAC__StreamDecoderEofCallback)(const void*, void*);
typedef void (*FLAC__StreamDecoderMetadataCallback)(const void*, const FLAC__StreamMetadata *metadata, void*);
typedef void (*FLAC__StreamDecoderErrorCallback)(const void*, FLAC__StreamDecoderErrorStatus status, void*);

typedef void* (*FLAC__stream_decoder_new_proc)(void);
typedef int (*FLAC__stream_decoder_set_metadata_respond_proc)(void*, FLAC__MetadataType);
typedef int (*FLAC__stream_decoder_set_metadata_ignore_proc)(void*, FLAC__MetadataType type);
typedef FLAC__StreamDecoderInitStatus (*FLAC__stream_decoder_init_stream_proc)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);
typedef int (*FLAC__stream_decoder_process_single_proc)(void*);
typedef int (*FLAC__stream_decoder_finish_proc)(void*);
typedef void (*FLAC__stream_decoder_delete_proc)(void*);
typedef int (*FLAC__stream_decoder_flush_proc)(void*);

typedef FLAC__StreamDecoderState (*FLAC__stream_decoder_get_state_proc)(const void*);

#endif /* __FILE_EXT_FLAC_H */


