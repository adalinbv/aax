/*
 * Copyright 2019 by Erik Hofman.
 * Copyright 2019 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef _EXT_PATCH
#define _EXT_PATCH 1

#if HAVE_CONFIG_H
#include "config.h"
#endif

#define SIZE2SAMPLES(h,a)		(8*(a)/(h)->bits_sample)
#define SIZE2TIME(h,a)			(SIZE2SAMPLES(h,(float)(a))/(h)->info.freq)

#define CVTSWEEP(a)			((a)/45.0f)
#define CVTRATE(a)			(0.05f + (a)/42.843f)
#define CVTDEPTH(a)			((a)/255.0f)
#define CVTDEPT2DB(a)			(16.0f*CVTDEPTH(a))
#define CVTDEPT2CENTS(a)		(1200.0f*CVTDEPTH(a))


#define PATCH_RESERVED_SIZE		36
#define PATCH_HEADER_SIZE		(93 + PATCH_RESERVED_SIZE)

#define INSTRUMENT_RESERVED_SIZE	40
#define INSTRUMENT_HEADER_SIZE		(23 + INSTRUMENT_RESERVED_SIZE)

#define	LAYER_RESERVED_SIZE		40
#define LAYER_HEADER_SIZE		(7 + LAYER_RESERVED_SIZE)

#define PATCH_DATA_RESERVED_SIZE	36
#define PACH_HEADER_SIZE		(60 + PATCH_DATA_RESERVED_SIZE)

#define FILE_HEADER_SIZE		(PATCH_HEADER_SIZE + INSTRUMENT_HEADER_SIZE + LAYER_HEADER_SIZE + PACH_HEADER_SIZE)

#define GF1_HEADER_TEXT			"GF1PATCH110"

#define MAX_LAYERS			4
#define ENVELOPES			6

#define HEADER_SIZE			12
#define ID_SIZE				10
#define DESC_SIZE			60
#define INST_NAME_SIZE			16
#define WAV_NAME_SIZE			7

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

typedef struct
{
   char header[HEADER_SIZE];
   char gravis_id[ID_SIZE];
   char description[DESC_SIZE];
   unsigned char instruments;
   char voices;
   char channels;
   unsigned short waveforms;
   unsigned short master_volume;
   unsigned int data_size;

} _patch_header_t;

typedef struct
{
   unsigned short instrument;
   char name[INST_NAME_SIZE];
   int size;
   char layers;
// char reserved[RESERVED_SIZE];

} _instrument_data_t;

typedef struct
{
   char layer_duplicate;
   char layer;
   int size;
   char samples;
// char reserved[LAYER_RESERVED_SIZE];

} _layer_data_t;

typedef struct
{
   char wave_name[WAV_NAME_SIZE];

   unsigned char fractions;
   int wave_size;
   int start_loop;
   int end_loop;

   unsigned short sample_rate;
   int low_frequency;
   int high_frequency;
   int root_frequency;
   short tune;

   unsigned char balance;

   unsigned char envelope_rate[ENVELOPES];
   unsigned char envelope_offset[ENVELOPES];

   unsigned char tremolo_sweep;
   unsigned char tremolo_rate;
   unsigned char tremolo_depth;

   unsigned char vibrato_sweep;
   unsigned char vibrato_rate;
   unsigned char vibrato_depth;

   /* bit 0 : 0 = 8-bit, 1 = 16-bit wave data			*/
   /* bit 1 : 0 = signed, 1 = unsigned data			*/
   /* bit 2 : 1 = looping enabled				*/
   /* bit 3 : 0 = uni-directional, 1 = bi-directional looping	*/
   /* bit 4 : 0 = play forwards, 1 = play backwards		*/
   /* bit 5 : 1 = turn sustaining on (envelope points 3)	*/
   /*             eneveloping stops at point 3 until note-off   */
   /* bit 6 : 0 = sample release after not-off message          */
   /*         1 = sample release after last envelope point      */
   /* bit 7 : 1 = the last three envelope points are ignored    */
   char modes;

   short scale_frequency;
   unsigned short scale_factor;

// char reserved[PATCH_HEADER_RESERVED_SIZE];

} _patch_data;

#endif /* _EXT_PATCH */

