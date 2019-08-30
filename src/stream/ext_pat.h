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

#define ENVELOPES			6
#define HEADER_SIZE			12
#define ID_SIZE				10
#define DESC_SIZE			60
#define RESERVED_SIZE			40
#define INSTRUMENT_HEADER_SIZE		(9 + RESERVED_SIZE)
#define	PATCH_RESERVED_SIZE		36
#define PATCH_HEADER_SIZE		(14 + PATCH_RESERVED_SIZE)
#define	LAYER_RESERVED_SIZE		40
#define LAYER_HEADER_SIZE		(8 + LAYER_RESERVED_SIZE)
#define PATCH_DATA_RESERVED_SIZE	36
#define PACH_HEADER_SIZE		(45 + PATCH_DATA_RESERVED_SIZE)
#define GF1_HEADER_TEXT			"GF1PATCH110"
#define MAX_LAYERS			4
#define INST_NAME_SIZE			16
#define FILE_HEADER_SIZE		(PATCH_HEADER_SIZE + INSTRUMENT_HEADER_SIZE + LAYER_HEADER_SIZE + PACH_HEADER_SIZE)


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
   char name[16];
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
   char wave_name[7];

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
   /* bit 4 : 0 = loop forwards, 1 = loop backward		*/
   /* bit 5 : 1 = turn sustaining on (envelope points 3)	*/
   /* bit 6 : 1 = enable envelope				*/
   char modes;

   short scale_frequency;
   unsigned short scale_factor;

// char reserved[PATCH_HEADER_RESERVED_SIZE];

} _patch_data;

#endif /* _EXT_PATCH */

