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

#include "objects.h"
#include "common.h"
#include "api.h"

#define DELAY_EFFECTS_TIME      0.070f
#define REVERB_EFFECTS_TIME     0.700f
#if 0
#define NO_DELAY_EFFECTS_TIME
#undef DELAY_EFFECTS_TIME
#define DELAY_EFFECTS_TIME      0.0f
#endif

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

typedef struct
{
   int id;
   int pos;
   int state;
   enum aaxEffectType type;
   _aaxEffectInfo* slot[_MAX_FE_SLOTS];
   _aaxMixerInfo* info;
} _effect_t;

_effect_t* new_effect(_aaxMixerInfo*, enum aaxEffectType);
_effect_t* new_effect_handle(_aaxMixerInfo*, enum aaxEffectType, _aax2dProps*, _aax3dProps*);
_effect_t* get_effect(aaxEffect);
_effect_t* get_effect(aaxEffect);

extern const _eff_cvt_tbl_t _eff_cvt_tbl[AAX_EFFECT_MAX];
extern const _eff_minmax_tbl_t _eff_minmax_tbl[_MAX_FE_SLOTS][AAX_EFFECT_MAX];


typedef aaxEffect _aaxEffectCreate(_handle_t*, enum aaxEffectType);
typedef int _aaxEffectDestroy(_effect_t*);
typedef aaxEffect _aaxEffectSetState(_effect_t*, int);
typedef _effect_t* _aaxNewEffectHandle(_aaxMixerInfo*, enum aaxEffectType, _aax2dProps*, _aax3dProps*);

typedef struct
{
   char lite;
   const char *name;
   float version;
   _aaxEffectCreate *create;
   _aaxEffectDestroy *destroy;
   _aaxEffectSetState *state;
   _aaxNewEffectHandle *handle;

} _eff_function_tbl;

extern _eff_function_tbl _aaxPitchEffect;
extern _eff_function_tbl _aaxDynamicPitchEffect;
extern _eff_function_tbl _aaxTimedPitchEffect;
extern _eff_function_tbl _aaxDistortionEffect;
extern _eff_function_tbl _aaxPhasingEffect;
extern _eff_function_tbl _aaxChorusEffect;
extern _eff_function_tbl _aaxFlangingEffect;
extern _eff_function_tbl _aaxVelocityEffect;
extern _eff_function_tbl _aaxReverbEffect;
extern _eff_function_tbl *_aaxEffects[AAX_EFFECT_MAX];

/* effects */
#define _EFFECT_GET_SLOT(E, s, p)       E->slot[s]->param[p]
#define _EFFECT_GET_SLOT_STATE(E)       E->slot[0]->state
#define _EFFECT_GET_SLOT_DATA(E, s)     E->slot[s]->data
#define _EFFECT_SET_SLOT(E, s, p, v)    E->slot[s]->param[p] = v
#define _EFFECT_SET_SLOT_DATA(E, s, v)  E->slot[s]->data = v

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

