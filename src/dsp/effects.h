/*
 * Copyright 2005-2015 by Erik Hofman.
 * Copyright 2009-2015 by Adalin B.V.
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

#include <base/geometry.h>

#include "common.h"

#define DELAY_EFFECTS_TIME      0.070f
#define REVERB_EFFECTS_TIME     0.700f
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME      0.0f
#endif

typedef struct
{
   float param[4];
   int state;
   void* data;		/* effect specific interal data structure */

} _aaxEffectInfo;

void _aaxSetDefaultEffect2d(_aaxEffectInfo*, unsigned int);
void _aaxSetDefaultEffect3d(_aaxEffectInfo*, unsigned int);

typedef struct {
  enum aaxEffectType type;
  int pos;
} _eff_cvt_tbl_t;

typedef struct {
   vec4_t min;
   vec4_t max;
} _eff_minmax_tbl_t;

cvtfn_t effect_get_cvtfn(enum aaxEffectType, int, int, char);

extern const _eff_cvt_tbl_t _eff_cvt_tbl[AAX_EFFECT_MAX];
extern const _eff_minmax_tbl_t _eff_minmax_tbl[_MAX_FE_SLOTS][AAX_EFFECT_MAX];

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

