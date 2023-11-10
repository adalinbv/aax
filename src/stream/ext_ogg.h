/*
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2016-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef __FILE_EXT_OGG_H
#define __FILE_EXT_OGG_H 1

#include <sys/types.h>

#if 0
enum oggFormat
{
   UNSUPPORTED = 0,
   PCM_OGG_FILE,
   VORBIS_OGG_FILE,
   SPEEX_OGG_FILE,
   OPUS_OGG_FILE,
   FLAC_OGG_FILE
};
#endif


// https://www.ietf.org/rfc/rfc3533.txt
// https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-630004.2.2
enum oggPacketType
{
   PACKET_NEW = 0x00,
   PACKET_CONTINUED = 0x01,
   PACKET_FIRST_PAGE = 0x02,
   PACKET_LAST_PAGE = 0x04
};

enum oggVorbisHeaderType
{
   HEADER_UNKNOWN = 0,
   HEADER_IDENTIFICATION = 1,
   HEADER_COMMENT = 3,
   HEADER_SETUP = 5
};



// https://wiki.xiph.org/OggPCM
enum oggpcm_type
{
   OGGPCM_FMT_S8     = 0x00000000,
   OGGPCM_FMT_U8     = 0x00000001,
   OGGPCM_FMT_S16_LE = 0x00000002,
   OGGPCM_FMT_S16_BE = 0x00000003,
   OGGPCM_FMT_S24_LE = 0x00000004,
   OGGPCM_FMT_S24_BE = 0x00000005,
   OGGPCM_FMT_S32_LE = 0x00000006,
   OGGPCM_FMT_S32_BE = 0x00000007,

   OGGPCM_FMT_ULAW   = 0x00000010,	// G.711 u-law encoding (8 bit)
   OGGPCM_FMT_ALAW   = 0x00000011,	// G.711 A-law encoding (8 bit)

   OGGPCM_FMT_FLT32_LE = 0x00000020,
   OGGPCM_FMT_FLT32_BE = 0x00000021,
   OGGPCM_FMT_FLT64_LE = 0x00000022,
   OGGPCM_FMT_FLT64_BE = 0x00000023
};

/*
 * Files containing precisely one channel and no explicit channel map are
 * assumed to contain plain mono.
 *
 * Files containing precisely two channels and no explicit channel map are
 * assumed to contain plain stereo.
 *
 * Files containing precisely three channels and no explicit channel map are
 * assumed to contain 1st order pantophonic Ambisonics (W, X and Y).
 *
 * Files containing precisely four channels and no explicit channel map are
 * assumed to contain 1st order periphonic Ambisonics (W, X, Y and Z).
 *
 * Files containing precisely six channels and no explicit channel map are
 * assumed to contain 5.1 in the ITU-R BS.775-1 layout.
 *
 * Files containing precisely seven channels and no explicit channel map are
 * assumed to contain 6.1 in the ITU+back channel layout.
 *
 * Files containing precisely eight channels and no explicit channel map are
 * assumed to contain 7.1 in the Dolby/DTS discrete layout.
 *
 * Files containing some other number of channels and no explicit channel map
 * are assumed to contain channels tagged with OGG_CHANNEL_UNUSED
 */
enum oggpcm_channel_map
{
   // front left/right
   OGG_CHANNEL_STEREO_LEFT = 0x00000000, // 30 degrees left
   OGG_CHANNEL_STEREO_RIGHT = 0x00000001, // 30 degrees right
   OGG_CHANNEL_QUAD_FRONT_LEFT = 0x00000002, // 45 degrees left
   OGG_CHANNEL_QUAD_FRONT_RIGHT = 0x00000003, // 45 degrees right
   OGG_CHANNEL_BLUMLEIN_LEFT = 0x00000004, // figure of eight response 45 degrees to the left
   OGG_CHANNEL_BLUMLEIN_RIGHT = 0x00000005, // figure of eight response 45 degrees to the right
   OGG_CHANNEL_WALL_FRONT_LEFT = 0x00000006, // 55 degrees left
   OGG_CHANNEL_WALL_FRONT_RIGHT = 0x00000007, // 55 degrees right
   OGG_CHANNEL_HEX_FRONT_LEFT = 0x00000008, // 60 degrees left
   OGG_CHANNEL_HEX_FRONT_RIGHT = 0x00000009, // 60 degrees right
   OGG_CHANNEL_PENTAGONAL_FRONT_LEFT = 0x0000000A, // 72 degrees left
   OGG_CHANNEL_PENTAGONAL_FRONT_RIGHT = 0x0000000B, // 72 degrees right
   OGG_CHANNEL_BINAURAL_LEFT = 0x0000000C, // fed directly into the left ear canal, or front stereo dipole with crosstalk cancellation
   OGG_CHANNEL_BINAURAL_RIGHT = 0x0000000D, // fed directly into the right ear canal, or front stereo dipole with crosstalk cancellation
   OGG_CHANNEL_FRONT_STEREO_DIPOLE_LEFT = 0x0000000E, // 5 degrees left
   OGG_CHANNEL_FRONT_STEREO_DIPOLE_RIGHT = 0x0000000F, // 5 degrees right
   OGG_CHANNEL_UHJ_L = 0x00000010, // ambisonics UHJ left
   OGG_CHANNEL_UHJ_R = 0x00000011, // ambisonics UHJ right
   OGG_CHANNEL_DOLBY_STEREO_LEFT = 0x00000012, // dolby stereo/surround left total
   OGG_CHANNEL_DOLBY_STEREO_RIGHT = 0x00000013, // dolby stereo/surround right total
   OGG_CHANNEL_XY_LEFT = 0x00000014, // cardioid response 45 degrees to the left
   OGG_CHANNEL_XY_RIGHT = 0x00000015, // cardioid response 45 degrees to the right

   // front center/mono
   OGG_CHANNEL_SCREEN_CENTER = 0x00000100, // ear level, straight ahead, at screen distance
   OGG_CHANNEL_MS_MID = 0x00000101, // cardioid response, straight ahead
   OGG_CHANNEL_FRONT_CENTER = 0x00000102, // ear level, straight ahead

   // lfe
   OGG_CHANNEL_LFE = 0x00000200, // omnidirectional, bandlimited to 120Hz, 10dB louder than the reference level
   OGG_CHANNEL_LFE_SIDE_LEFT = 0x00000201, // 90 degrees left, bandlimited to 120Hz, 10dB louder than the reference level
   OGG_CHANNEL_LFE_SIDE_RIGHT = 0x00000202, // 90 degrees right, bandlimited to 120Hz, 10dB louder than the reference level
   OGG_CHANNEL_LFE_FRONT_CENTER_LEFT = 0x00000203, // 22.5 degrees left, bandlimited to 120Hz, 10dB louder than the reference level
   OGG_CHANNEL_LFE_FRONT_CENTER_RIGHT = 0x00000204, // 22.5 degrees right, bandlimited to 120Hz, 10dB louder than the reference level
   OGG_CHANNEL_LFE_FRONT_BOTTOM_CENTER_LEFT = 0x00000205, // 45 degrees lowered, 22.5 degrees left, bandlimited to 120Hz, 10dB louder than the reference level
   OGG_CHANNEL_LFE_FRONT_BOTTOM_CENTER_RIGHT = 0x00000206, // 45 degrees lowered, 22.5 degrees right, bandlimited to 120Hz, 10dB louder than the reference level

   // back left/right
   OGG_CHANNEL_ITU_BACK_LEFT = 0x00000300, // back, 70 degrees left
   OGG_CHANNEL_ITU_BACK_RIGHT = 0x00000301, // back, 70 degrees right
   OGG_CHANNEL_ITU_BACK_LEFT_SURROUND = 0x00000302, // back, 70 degrees left
   OGG_CHANNEL_ITU_BACK_RIGHT_SURROUND = 0x00000303, // back, 70 degrees right
   OGG_CHANNEL_HEX_BACK_LEFT = 0x00000304, // back, 60 degrees left
   OGG_CHANNEL_HEX_BACK_RIGHT = 0x00000305, // back, 60 degrees right
   OGG_CHANNEL_QUAD_BACK_LEFT = 0x00000306, // back, 45 degrees left
   OGG_CHANNEL_QUAD_BACK_RIGHT = 0x00000307, // back, 45 degrees right
   OGG_CHANNEL_PENTAGONAL_BACK_LEFT = 0x00000308, // back, 36 degrees left
   OGG_CHANNEL_PENTAGONAL_BACK_RIGHT = 0x00000309, // back, 36 degrees right
   OGG_CHANNEL_BACK_STEREO_LEFT = 0x0000030A, // back, 30 degrees left
   OGG_CHANNEL_BACK_STEREO_RIGHT = 0x0000030B, // back, 30 degrees right
   OGG_CHANNEL_BACK_STEREO_DIPOLE_LEFT = 0x0000030C, // back, 5 degrees left
   OGG_CHANNEL_BACK_STEREO_DIPOLE_RIGHT = 0x0000020E, // back, 5 degrees right

   // front center left/right
   OGG_CHANNEL_FRONT_CENTER_LEFT = 0x00000400, // 22.5 degrees left
   OGG_CHANNEL_FRONT_CENTER_RIGHT = 0x00000401, // 22.5 degrees right

   // back center
   OGG_CHANNEL_BACK_CENTER = 0x00000500, // straight back
   OGG_CHANNEL_BACK_CENTER_SURROUND = 0x00000501, // straight back, diffuse
   OGG_CHANNEL_SURROUND = 0x00000502, // back and sides, diffuse

   // side left/right
   OGG_CHANNEL_SIDE_LEFT = 0x00000600, // 90 degrees left
   OGG_CHANNEL_SIDE_RIGHT = 0x00000601, // 90 degrees right
   OGG_CHANNEL_SIDE_LEFT_SURROUND = 0x00000602, // 90 degrees left, diffuse
   OGG_CHANNEL_SIDE_RIGHT_SURROUND = 0x00000603, // 90 degrees right, diffuse

   // rest of the wav/usb/caf mask types
   OGG_CHANNEL_TOP_CENTER = 0x00000700, // 90 degrees elevated
   OGG_CHANNEL_FRONT_TOP_LEFT = 0x00000701, // 45 degrees elevated, 45 degrees left
   OGG_CHANNEL_FRONT_TOP_CENTER = 0x00000702, // 45 degrees elevated
   OGG_CHANNEL_FRONT_TOP_RIGHT = 0x00000703, // 45 degrees elevated, 45 degrees left
   OGG_CHANNEL_BACK_TOP_LEFT = 0x00000704, // back, 45 degrees elevated, 45 degrees left
   OGG_CHANNEL_BACK_TOP_CENTER = 0x00000705, // back, 45 degrees elevated
   OGG_CHANNEL_BACK_TOP_RIGHT = 0x00000706, // back, 45 degrees elevated, 45 degrees right

   // rest of the cube
   OGG_CHANNEL_SIDE_TOP_LEFT = 0x00000800, // (45 degrees elevated, 90 degrees left
   OGG_CHANNEL_SIDE_TOP_RIGHT = 0x00000801, // (45 degrees elevated, 90 degrees right
   OGG_CHANNEL_FRONT_BOTTOM_LEFT = 0x00000802, // (45 degrees lowered, 45 degrees left
   OGG_CHANNEL_FRONT_BOTTOM_CENTER = 0x00000803, // (45 degrees lowered
   OGG_CHANNEL_FRONT_BOTTOM_RIGHT = 0x00000804, // (45 degrees lowered, 45 degrees right
   OGG_CHANNEL_SIDE_BOTTOM_LEFT = 0x00000805, // (45 degrees lowered, 90 degrees left
   OGG_CHANNEL_BOTTOM_CENTER = 0x00000806, // (90 degrees lowered
   OGG_CHANNEL_SIDE_BOTTOM_RIGHT = 0x00000807, // (45 degrees lowered, 90 degrees left
   OGG_CHANNEL_BACK_BOTTOM_CENTER = 0x00000808, // (back, 45 degrees lowered
   OGG_CHANNEL_BACK_BOTTOM_LEFT = 0x00000809, // (back, 45 degrees lowered, 45 degrees left
   OGG_CHANNEL_BACK_BOTTOM_RIGHT = 0x0000080A, // (back, 45 degrees lowered, 45 degrees right

   // ambisonics
   OGG_CHANNEL_AMBISONICS_W = 0x00000900, // (0th order
   OGG_CHANNEL_AMBISONICS_X = 0x00000901, // (1st order
   OGG_CHANNEL_AMBISONICS_Y = 0x00000902, // (1st order, also used for mid/side side
   OGG_CHANNEL_AMBISONICS_Z = 0x00000903, // (1st order
   OGG_CHANNEL_AMBISONICS_R = 0x00000904, // (2nd order
   OGG_CHANNEL_AMBISONICS_S = 0x00000905, // (2nd order
   OGG_CHANNEL_AMBISONICS_T = 0x00000906, // (2nd order
   OGG_CHANNEL_AMBISONICS_U = 0x00000907, // (2nd order
   OGG_CHANNEL_AMBISONICS_V = 0x00000908, // (2nd order
   OGG_CHANNEL_AMBISONICS_K = 0x00000909, // (3rd order
   OGG_CHANNEL_AMBISONICS_L = 0x0000090A, // (3rd order
   OGG_CHANNEL_AMBISONICS_M = 0x0000090B, // (3rd order
   OGG_CHANNEL_AMBISONICS_N = 0x0000090C, // (3rd order
   OGG_CHANNEL_AMBISONICS_O = 0x0000090D, // (3rd order
   OGG_CHANNEL_AMBISONICS_P = 0x0000090E, // (3rd order
   OGG_CHANNEL_AMBISONICS_Q = 0x0000090F, // (3rd order

   // passive matrix additions
   OGG_CHANNEL_MS_SIDE = 0x00000902, // (figure of eight response left to right, same as Ambisonics Y
   OGG_CHANNEL_UHJ_T = 0x00000A01, // (ambisonics UHJ addition for pantophony
   OGG_CHANNEL_UHJ_Q = 0x00000A02, // (ambisonics UHJ addition for periphony

   // specials
   OGG_CHANNEL_UNUSED = 0x00000B00, // (the channel is unused and should not be rendered
};

enum oggpcm_xheader
{
    OGG_CHANNEL_MAPPING = 0x00000000,
    OGG_CHANNEL_CONVERSION = 0x00000001
};


typedef struct {
  unsigned char *packet;
  long bytes;
  long b_o_s;
  long e_o_s;
  int64_t granulepos;
  int64_t packetno;
} ogg_packet;

typedef struct {
  long endbyte;
  int endbit;
  unsigned char *buffer;
  unsigned char *ptr;
  long storage;
} oggpack_buffer;

typedef struct {
  unsigned char *header;
  long header_len;
  unsigned char *body;
  long body_len;
} ogg_page;

typedef struct {
  unsigned char *body_data;
  long body_storage;
  long body_fill;
  long body_returned;
  int *lacing_vals;
  int64_t *granule_vals;
  long lacing_storage;
  long lacing_fill;
  long lacing_packet;
  long lacing_returned;
  unsigned char header[282];
  int header_fill;
  int e_o_s;
  int b_o_s;
  long serialno;
  long pageno;
  int64_t packetno;
  int64_t granulepos;
} ogg_stream_state;

typedef void (*ogg_stream_init_proc)(ogg_stream_state*, int);
typedef void (*ogg_stream_packetin_proc)(ogg_stream_state*, ogg_packet*);
typedef int (*ogg_stream_pageout_proc)(ogg_stream_state*, ogg_page*);
typedef int (*ogg_stream_flush_proc)(ogg_stream_state*, ogg_page*);
typedef int (*ogg_page_eos_proc)(ogg_page*);
typedef int (*ogg_stream_clear_proc)(ogg_stream_state*);

#endif /* __FILE_EXT_OGG_H */

