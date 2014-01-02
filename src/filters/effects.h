/*
 * Copyright 2005-2014 by Erik Hofman.
 * Copyright 2009-2014 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_EFFECTS_H
#define _AAX_EFFECTS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <driver.h>

#define DELAY_EFFECTS_TIME      0.070f
#define REVERB_EFFECTS_TIME     0.700f

#define _AAX_MAX_FILTERS        2
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME      0.0f
#endif

enum _aax3dFiltersEffects
{
    /* 3d filters */
    DISTANCE_FILTER = 0,        /* distance attennuation */
    ANGULAR_FILTER,             /* audio cone support    */
    MAX_3D_FILTER,

    /* 3d effects */
    VELOCITY_EFFECT = 0,        /* Doppler               */
    MAX_3D_EFFECT,
};

enum _aax2dFiltersEffects
{
    /* final mixer stage */
    EQUALIZER_LF = 0,
    EQUALIZER_HF,
    EQUALIZER_MAX,

    /* stereo filters */
    VOLUME_FILTER = 0,
    DYNAMIC_GAIN_FILTER,
    TIMED_GAIN_FILTER,
    FREQUENCY_FILTER,
    MAX_STEREO_FILTER,

    /* stereo effects */
    PITCH_EFFECT = 0,
    REVERB_EFFECT,
    DYNAMIC_PITCH_EFFECT,
    TIMED_PITCH_EFFECT,
    DISTORTION_EFFECT,
    DELAY_EFFECT,               /* phasing, chorus, flanging  */
    MAX_STEREO_EFFECT,
};

enum _aaxCompressionType
{
    RB_COMPRESS_ELECTRONIC = 0,
    RB_COMPRESS_DIGITAL,
    RB_COMPRESS_VALVE,

    RB_COMPRESS_MAX
};

typedef struct
{
   float param[4];
   int state;
   void* data;		/* filter specific interal data structure */

} _aaxFilterInfo;

typedef _aaxFilterInfo _aaxEffectInfo;

void _aaxSetDefaultFilter2d(_aaxFilterInfo*, unsigned int);
void _aaxSetDefaultFilter3d(_aaxFilterInfo*, unsigned int);
void _aaxSetDefaultEffect2d(_aaxFilterInfo*, unsigned int);
void _aaxSetDefaultEffect3d(_aaxFilterInfo*, unsigned int);
void _aaxSetDefaultEqualizer(_aaxFilterInfo filter[2]);

typedef float _aaxRingBufferPitchShiftFn(float, float, float);
extern _aaxRingBufferPitchShiftFn* _aaxRingBufferDopplerFn[];

typedef float _aaxRingBufferDistFn(float, float, float, float, float, float);
extern _aaxRingBufferDistFn* _aaxRingBufferDistanceFn[];
extern _aaxRingBufferDistFn* _aaxRingBufferALDistanceFn[];

void iir_compute_coefs(float, float, float*, float*, float);
void _aaxRingBufferDelaysAdd(void**, float, unsigned int, const float*, const float*, unsigned int, float, float, float);
void _aaxRingBufferDelaysRemove(void**);
void _aaxRingBufferCreateHistoryBuffer(void**, int32_t*[_AAX_MAX_SPEAKERS], float, int, float);

float _lin(float v);
float _lin2db(float v);
float _db2lin(float v);
float _square(float v);
float _lin2log(float v);
float _log2lin(float v);
float _rad2deg(float v);
float _deg2rad(float v);
float _cos_deg2rad_2(float v);
float _2acos_rad2deg(float v);
float _cos_2(float v);
float _2acos(float v);

/* filters */
#define _FILTER_GET_SLOT(F, s, p)       F->slot[s]->param[p]
#define _FILTER_GET_SLOT_STATE(F)       F->slot[0]->state
#define _FILTER_GET_SLOT_DATA(F, s)     F->slot[s]->data
#define _FILTER_SET_SLOT(F, s, p, v)    F->slot[s]->param[p] = v
#define _FILTER_SET_SLOT_DATA(F, s, v)  F->slot[s]->data = v

#define _FILTER_GET(P, f, p)            P->filter[f].param[p]
#define _FILTER_GET_STATE(P, f)         P->filter[f].state
#define _FILTER_GET_DATA(P, f)          P->filter[f].data
#define _FILTER_SET(P, f, p, v)         P->filter[f].param[p] = v
#define _FILTER_SET_STATE(P, f, v)      P->filter[f].state = v;
#define _FILTER_SET_DATA(P, f, v)       P->filter[f].data = v
#define _FILTER_COPY(P1, P2, f, p)      P1->filter[f].param[p] = P2->filter[f].param[p]
#define _FILTER_COPY_DATA(P1, P2, f)    P1->filter[f].data = P2->filter[f].data
#define _FILTER_COPY_STATE(P1, P2, f)   P1->filter[f].state = P2->filter[f].state

#define _FILTER_GET2D(G, f, p)          _FILTER_GET(G->props2d, f, p)
#define _FILTER_GET2D_DATA(G, f)        _FILTER_GET_DATA(G->props2d, f)
#define _FILTER_GET3D(G, f, p)          _FILTER_GET(G->dprops3d, f, p)
#define _FILTER_GET3D_DATA(G, f)        _FILTER_GET_DATA(G->dprops3d, f)
#define _FILTER_SET2D(G, f, p, v)       _FILTER_SET(G->props2d, f, p, v)
#define _FILTER_SET2D_DATA(G, f, v)     _FILTER_SET_DATA(G->props2d, f, v)
#define _FILTER_SET3D(G, f, p, v)       _FILTER_SET(G->dprops3d, f, p, v)
#define _FILTER_SET3D_DATA(G, f, v)     _FILTER_SET_DATA(G->dprops3d, f, v)
#define _FILTER_COPY2D_DATA(G1, G2, f)  _FILTER_COPY_DATA(G1->props2d, G2->props2d, f)
#define _FILTER_COPY3D_DATA(G1, G2, f)  _FILTER_COPY_DATA(G1->dprops3d, G2->dprops3d, f)

#define _FILTER_GETD3D(G, f, p)         _FILTER_GET(G->props3d, f, p)
#define _FILTER_SETD3D_DATA(G, f, v)    _FILTER_SET_DATA(G->props3d, f, v)
#define _FILTER_COPYD3D_DATA(G1, G2, f) _FILTER_COPY_DATA(G1->props3d, G2->props3d, f)

#define _FILTER_SWAP_SLOT_DATA(P, f, F, s)                              \
    do { void* ptr = P->filter[f].data;                                 \
    P->filter[f].data = F->slot[s]->data; F->slot[s]->data = ptr;       \
    if (!s) aaxFilterSetState(F, P->filter[f].state); } while (0);


/* effects */
#define _EFFECT_GET_SLOT                _FILTER_GET_SLOT
#define _EFFECT_GET_SLOT_STATE          _FILTER_GET_SLOT_STATE
#define _EFFECT_GET_SLOT_DATA           _FILTER_GET_SLOT_DATA

#define _EFFECT_GET(P, f, p)            P->effect[f].param[p]
#define _EFFECT_GET_STATE(P, f)         P->effect[f].state
#define _EFFECT_GET_DATA(P, f)          P->effect[f].data
#define _EFFECT_SET(P, f, p, v)         P->effect[f].param[p] = v
#define _EFFECT_SET_STATE(P, f, v)      P->effect[f].state = v;
#define _EFFECT_SET_DATA(P, f, v)       P->effect[f].data = v
#define _EFFECT_COPY(P1, P2, f, p)      \
                                P1->effect[f].param[p] = P2->effect[f].param[p]
#define _EFFECT_COPY_DATA(P1, P2, f)    P1->effect[f].data = P2->effect[f].data

#define _EFFECT_GET2D(G, f, p)          _EFFECT_GET(G->props2d, f, p)
#define _EFFECT_GET2D_DATA(G, f)        _EFFECT_GET_DATA(G->props2d, f)
#define _EFFECT_GET3D(G, f, p)          _EFFECT_GET(G->dprops3d, f, p)
#define _EFFECT_GET3D_DATA(G, f)        _EFFECT_GET_DATA(G->dprops3d, f)
#define _EFFECT_SET2D(G, f, p, v)       _EFFECT_SET(G->props2d, f, p, v)
#define _EFFECT_SET2D_DATA(G, f, v)     _EFFECT_SET_DATA(G->props2d, f, v)
#define _EFFECT_SET3D(G, f, p, v)       _EFFECT_SET(G->dprops3d, f, p, v)
#define _EFFECT_SET3D_DATA(G, f, v)     _EFFECT_SET_DATA(G->dprops3d, f, v)
#define _EFFECT_COPY2D(G1, G2, f, p)    _EFFECT_COPY(G1->props2d, G2->props2d, f, p)
#define _EFFECT_COPY3D(G1, G2, f, p)    _EFFECT_COPY(G1->dprops3d, G2->dprops3d, f, p)
#define _EFFECT_COPY2D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->props2d, G2->props2d, f)
#define _EFFECT_COPY3D_DATA(G1, G2, f)  _EFFECT_COPY_DATA(G1->dprops3d, G2->dprops3d, f)

#define _EFFECT_GETD3D(G, f, p)         _EFFECT_GET(G->props3d, f, p)
#define _EFFECT_SETD3D_DATA(G, f, v)    _EFFECT_SET_DATA(G->props3d, f, v)
#define _EFFECT_COPYD3D(G1, G2, f, p)   _EFFECT_COPY(G1->props3d, G2->props3d, f, p)
#define _EFFECT_COPYD3D_DATA(G1, G2, f) _EFFECT_COPY_DATA(G1->props3d, G2->props3d, f)

#define _EFFECT_SWAP_SLOT_DATA(P, f, F, s)                              \
    do { void* ptr = P->effect[f].data;                                 \
    P->effect[f].data = F->slot[s]->data; F->slot[s]->data = ptr;       \
    if (!s) aaxEffectSetState(F, P->effect[f].state); } while (0);



#endif /* _AAX_EFFECTS_H */

