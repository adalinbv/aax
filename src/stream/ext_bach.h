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
 * - File Signature: "BACH"
 * - Info Block(1)
 * - Resonator Block(2)
 * - Actuator Block (3)
 * - Body Block (4)
 *
 *
 * (1) Info Block
 *     * size <uint32_t>
 *     * version <uint8_t> (file format version)
 *     * bank_no <uint8_t>
 *     * program_no <uint8_t>
 *     * reserved <uint8_t>
 *     * note
 *        - polyphony <uint8_t>
 *        - min <uint8_t>
 *        - max <uint8_t>
 *        - reserved <uint8_t>
 *        - pitch-fraction <float32>
 *     * name <str_t>
 *     * license <str_t>
 *     * copyright <str_t>
 *     * author <str_t>
 *     * website <str_t>
 *
 * (2) Resonator Block:
 *     * size <uint32_t>
 *     * frequency <float32>
 *     * reserved[3] <uint8_t>
 *     * no_layers <uint8_t>
 *     * layer[2]
 *        - loop_start <float32>
 *        - loop_end <float32>
 *        - len <uint32_t>
 *        - waveform_data[] <float32>
 *
 * (3) Actuator Block:
 *     * size <uint32_t>
 *     * looping <uint8_t>
 *     * reserved[2] <uint8_t>
 *     * no_dsp <uint8_t>
 *     * dsp[](5)
 *
 * (4) Body Block:
 *     * size <uint32_t>
 *     * mode <uint32_t>
 *     * pan <float32>
 *     * reserved[3] <uint8_t>
 *     * no_dsp <uint8_t>
 *     * dsp[](5)
 *
 * (5) DSP Block
 *     * size <uint32_t>
 *     * type <enum aaxFilterType / enum aaxEffectType>
 *     * src <enum aaxSourceType>
 *     * reserved[3] <uint8_t>
 *     * no_slots <uint8_t>
 *     * slot[]
 *        - src <enum aaxSourceType>
 *        - param[4]
 *            + type <enum aaxType>
 *            + value <float32>
 *            + min <float32>
 *            + max <float32>
 *            + adjust <float32>
 *            + pitch <float32>
 *            + random <float32>
 *
 * The file data is little-endian.
 * enum types are represented as <int32_t>
 * <str_t> defines a String Block which is an UTF-8 encoded byte array.
 *
 * Every Block starts with an <uin32_t> specifying the length of the block
 * in bytes.
 */

#define BACH_MAX_COPYRIGHT_ENTRIES	8
typedef struct
{
   uint8_t version;
   uint8_t bank_no;
   uint8_t program_no;

   struct
   {
      uint8_t polyphony;
      uint8_t min, max;
      float pitch_fraction;
   } note;

   struct _meta_t meta;

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

   uint8_t no_layers;
   _layer_t layer[BACH_MAX_LAYERS];

} _resonator_t;

typedef struct
{
   enum aaxType type;
   float value;
   float min, max;
   float adjust; // auto adjust
   float pitch;
   float random;

} _param_t;

typedef struct
{
   enum aaxSourceType src;
   _param_t param[4];

} _slot_t;

typedef struct
{
   int32_t type; // positive is filter, negative is effect
   enum aaxSourceType src;

   uint8_t no_slots;
   _slot_t slot[BACH__MAX_FE_SLOTS];

} _dsp_t;

#define BACH_MAX_DSP_ENTRIES	16
typedef struct
{
   bool looping;

   uint8_t no_dsp;
   _dsp_t dsp[BACH_MAX_DSP_ENTRIES];

} _actuator_t;

typedef struct
{
   float pan;
   enum aaxProcessingType mode;

   uint8_t no_dsp;
   _dsp_t dsp[BACH_MAX_DSP_ENTRIES];

} _body_t;

typedef struct
{
   _info_t info;
   _resonator_t resonator;
   _actuator_t actuator;
   _framte_t body;

} _aaxs_t;

#endif /* _EXT_PATCH */

