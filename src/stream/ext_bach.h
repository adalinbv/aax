/*
 * SPDX-FileCopyrightText: Copyright © 2023-2024 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2023-2024 by Adalin B.V.
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

#include <aax/aax.h>

#include <dsp/common.h>

/* BACH: Binary-encoded Aeonwave patCH
 *
 * File layout:
 *
 * Info Block(1)
 * Sound Block(2)
 * Emitter: looping, filters(3), effects(3)
 * AudioFrame: mode, pan, filters(3), effects(3)
 *
 * (1) Info Block
 *     * version <uint8_t> (file format version)
 *     * bank_no <uint8_t>
 *     * program_no <uint8_t>
 *     * note
 *        - polyphony <uint8_t>
 *        - min <uint8_t>
 *        - max <uint8_t>
 *        - reserved[2] <uint8_t>
 *        - pitch-fraction <float32>
 *     * name <str_t>
 *     * license <str_t>
 *     * copyright[] <str_t>
 *     * cotact: author, website <str_t>
 *
 * (2) Sound Block:
 *     * frequency <float32>
 *     * layer[2]
 *        - loop_start <float32>
 *        - loop_end <float32>
 *        - len <uint32_t>
 *        - waveform_data <float32>
 *
 * (3) DSP Block
 *     * type <enum aaxFilterType / enum aaxEffectType>
 *     * src <enum aaxSourceType>
 *     * slot[4]
 *        - src <enum aaxSourceType>
 *        - param[4]
 *            + type <enum aaxType>
 *            + value <float32>
 *            + min <float32>
 *            + max <float32>
 *            + auto/pitch <float32>
 *            + random <float32>
 *
 * enum types are represented as <int32_t>
 * <str_t> defines a String Block which is an UTF-8 encoded byte array.
 *
 * Every Block starts with an <uin32_t> specifying the length of the block
 * in bytes.
 */

typedef struct
{
   uint32_t len;
   uint8_t *s;
} str_t;

#define BACH_MAX_COPYRIGHT_ENTRIES	8
typedef struct
{
   uint8_t version;
   uint8_t bank_no;
   uint8_t program_no;

   // note
   uint8_t polyphony;
   uint8_t min, max;
   uint8_t reserved[2];
   float pitch_fraction;

   str_t name;
   str_t license;
   str_t copyright[BACH_MAX_COPYRIGHT_ENTRIES];
   // contact:
   str_t author;
   str_t website;

} _info_t;

typedef struct
{
   float loop_start;
   float loop_end;

   uint32_t len;
   float *data;

} _layer_t;

#define BACH_MAX_LAYERS
typedef struct
{
   float frequency;
   _layer_t layer[BACH_MAX_LAYERS];

} _sound_t;

typedef struct
{
   enum aaxType type;
   float value;
   float min, max;
   float auto_pitch;
   float random;

} _param_t;

#define BACH_MAX_SLOTS	4
typedef struct
{
   enum aaxSourceType src;
   _param_t param[BACH_MAX_SLOTS];

} _slot_t;

typedef struct
{
   int32_t type; // positive is filter, negative is effect
   enum aaxSourceType src;
   _slot_t slot[4];

} _dsp_t;

#define BACH_MAX_DSP_ENTRIES	16
typedef struct
{
   bool looping;
   _dsp_t filter[BACH_MAX_DSP_ENTRIES];
   _dsp_t effect[BACH_MAX_DSP_ENTRIES];

} _emitter_t;

typedef struct
{
   enum aaxProcessingType mode;
   float pan;
   _dsp_t filter[BACH_MAX_DSP_ENTRIES];
   _dsp_t effect[BACH_MAX_DSP_ENTRIES];

} _frame_t;

#endif /* _EXT_PATCH */

