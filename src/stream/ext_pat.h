/*
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#ifndef _EXT_PATCH
#define _EXT_PATCH 1

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#include <dsp/common.h>

#define SIZE2SAMPLES(h,a)		(8*(a)/(h)->bits_sample)
#define SAMPLES2TIME(h,a)		((float)(a)/(h)->wave.sample_rate)
#define SIZE2TIME(h,a)			SAMPLES2TIME(SIZE2SAMPLES((h),(a)),(a))

#define CVTSWEEP(a)			((a)/45.0f)
#define CVTRATE(a)			(0.05f + (a)/42.843f)
#define CVTDEPTH(a)			(_db2lin(CVTDEPT2DB(a))-1.0f)
#define CVTDEPT2DB(a)			((a) ? 0.047f + (a)*12.0f/256.0f : 0.0f)
#define CVTDEPT2CENTS(a)		((a) ? 100.0f*CVTDEPT2DB(a) : 0.0f)
#define CVTCENTS2PITCH(a)		(powf(2.0f, 2.0f*(a)/1200.0f)-1.0f)
#define CVTDEPTH2PITCH(a)		(CVTCENTS2PITCH(CVTDEPT2CENTS(a)))

/* As per Ultrasound Software Development Kit (SDK) */
/* http://dk.toastednet.org/GUS/docs/UltraSound%20Lowlevel%20ToolKit%20v2.22%20(21%20December%201994).pdf */
#define GF1_HEADER_SIZE			12
#define GF1_HEADER_TEXT			"GF1PATCH110"
#define PATCH_ID_SIZE			10
#define PATCH_DESC_SIZE			60
#define PATCH_RESERVED_SIZE		36
#define PATCH_HEADER_SIZE		(93 + PATCH_RESERVED_SIZE)

#define INSTRUMENT_NAME_SIZE			16
#define INSTRUMENT_RESERVED_SIZE	40
#define INSTRUMENT_HEADER_SIZE		(23 + INSTRUMENT_RESERVED_SIZE)

#define	LAYER_RESERVED_SIZE		40
#define LAYER_HEADER_SIZE		(7 + LAYER_RESERVED_SIZE)

#define WAVE_NAME_SIZE			7
#define WAVE_RESERVED_SIZE		36
#define WAVE_HEADER_SIZE		(60 + WAVE_RESERVED_SIZE)

#define FILE_HEADER_SIZE		(PATCH_HEADER_SIZE + INSTRUMENT_HEADER_SIZE + LAYER_HEADER_SIZE + WAVE_HEADER_SIZE)
#define MAX_PATCH_SIZE			(1024*1024)

#define MAX_LAYERS			4
#define ENVELOPES			6

enum
{
   MODE_16BIT			= 0x01,
   MODE_UNSIGNED		= 0x02,
   MODE_FORMAT			= (MODE_16BIT|MODE_UNSIGNED),

   MODE_LOOPING			= 0x04,
   MODE_BIDIRECTIONAL		= 0x08,
   MODE_REVERSE			= 0x10,

   MODE_ENVELOPE_SUSTAIN	= 0x20,
   MODE_ENVELOPE_RELEASE	= 0x40,
   MODE_FAST_RELEASE		= 0x80

} _modes;

/*
 * Patch file layout:
 *
 * patch header (# instruments)
 *   instrument 1 header (# layers)
 *     layer 1 header (# waves)
 *       wave 1 header
 *       wave 1 data
 *       ...
 *       wave n header
 *       wave n data
 *     layer 2 header
 *       wave 1 header
 *       wave 1 data
 *       ...
 *       wave n header
 *       wave n data
 */

typedef struct
{
   char name[WAVE_NAME_SIZE+1];

   uint8_t fractions;
   int32_t size;
   int32_t start_loop;
   int32_t end_loop;

   uint16_t sample_rate;
   struct {
      int32_t low;
      int32_t high;
      int32_t root;
   } frequency;
   int16_t tune;

   uint8_t balance;

   uint8_t envelope_rate[ENVELOPES];
   uint8_t envelope_level[ENVELOPES];

   struct {
      uint8_t sweep;
      uint8_t rate;
      uint8_t depth;
   } tremolo, vibrato;

   /* bit 0 : 0 = 8-bit, 1 = 16-bit wave data			*/
   /* bit 1 : 0 = signed, 1 = unsigned data			*/
   /* bit 2 : 1 = looping enabled				*/
   /* bit 3 : 0 = uni-directional, 1 = bi-directional looping	*/
   /* bit 4 : 0 = play forwards, 1 = play backwards		*/
   /* bit 5 : 1 = turn sustaining on (envelope points 3)	*/
   /*             eneveloping stops at point32_t 3 until note-off   */
   /* bit 6 : 0 = sample release after not-off message          */
   /*         1 = sample release after last envelope point32_t      */
   /* bit 7 : 1 = the last three envelope points are ignored    */
   int8_t modes;

   int16_t scale_frequency;
   uint16_t scale_factor;

// char reserved[PATCH_HEADER_RESERVED_SIZE];

} _wave_t;

typedef struct
{
   int8_t layer_duplicate;
   int8_t layer;
   int32_t size;
   int8_t waves;
// char reserved[LAYER_RESERVED_SIZE];

} _layer_t;

typedef struct
{
   uint16_t instrument;
   char name[INSTRUMENT_NAME_SIZE+1];
   int32_t size;
   int8_t layers;
// char reserved[RESERVED_SIZE];

} _instrument_t;

typedef struct
{
   char header[GF1_HEADER_SIZE+1];
   char gravis_id[PATCH_ID_SIZE+1];
   char description[PATCH_DESC_SIZE+1];
   uint8_t instruments;
   int8_t voices;
   int8_t channels;
   uint16_t waveforms;
   uint16_t master_volume;
   uint32_t data_size;

} _patch_header_t;

#endif /* _EXT_PATCH */

