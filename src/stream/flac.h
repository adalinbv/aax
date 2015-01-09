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

#define	FLAC__MAX_METADATA_TYPE_CODE   (126u)

enum FLAC__MetadataType
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
}

typedef FLAC__StreamDecoderReadStatus(* FLAC__StreamDecoderReadCallback)(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
typedef FLAC__StreamDecoderWriteStatus(* FLAC__StreamDecoderWriteCallback)(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);
typedef FLAC__bool(* FLAC__StreamDecoderEofCallback)(const FLAC__StreamDecoder *decoder, void *client_data);
typedef void(* FLAC__StreamDecoderMetadataCallback)(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
typedef void(* FLAC__StreamDecoderErrorCallback)(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

typedef void* (*FLAC__stream_decoder_new_proc)(void);
typedef FLAC__bool (*FLAC__stream_decoder_set_metadata_respond_proc)(void*, FLAC__MetadataType);
typedef FLAC__bool (*FLAC__stream_decoder_set_metadata_ignore_proc)(void, FLAC__MetadataType type);
typedef FLAC__StreamDecoderInitStatus (*FLAC__stream_decoder_init_stream_proc)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void);
typedef FLAC__bool (*FLAC__stream_decoder_process_single_proc)(void*);
typedef FLAC__bool (*FLAC__stream_decoder_finish_proc)(void*);
typedef void (*FLAC__stream_decoder_delete_proc)(void*);

typedef unsigned (*FLAC__stream_decoder_get_channels_proc)(const void*);
typedef FLAC__ChannelAssignment (*FLAC__stream_decoder_get_channel_assignment_proc)(const void*);
typedef unsigned (*FLAC__stream_decoder_get_bits_per_sample_proc)(const void*);
typedef unsigned (*FLAC__stream_decoder_get_sample_rate_proc)(const void*);
typedef unsigned (*FLAC__stream_decoder_get_blocksize_proc)(const void*);

#endif /* __FILE_EXT_FLAC_H */


