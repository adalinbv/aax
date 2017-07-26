/*
 * Copyright 2005-2017 by Erik Hofman.
 * Copyright 2009-2017 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_FILTERS_H
#define _AAX_FILTERS_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <base/geometry.h>

#include "driver.h"
#include "objects.h"
#include "common.h"
#include "api.h"

#define _AAX_MAX_FILTERS	2

void _aaxSetDefaultFilter2d(_aaxFilterInfo*, unsigned int);
void _aaxSetDefaultFilter3d(_aaxFilterInfo*, unsigned int);
void _aaxSetDefaultEqualizer(_aaxFilterInfo filter[EQUALIZER_MAX]);

typedef struct
{
  enum aaxFilterType type;
  int pos;

} _flt_cvt_tbl_t;

typedef struct
{
   fx4_t min;
   fx4_t max;

} _flt_minmax_tbl_t;

typedef struct
{
   unsigned int id;
   int pos;
   int state;
   enum aaxFilterType type;
   _aaxFilterInfo* slot[_MAX_FE_SLOTS];
   _aaxMixerInfo* info;
   _handle_t *handle;
} _filter_t;

// _filter_t* new_filter(_aaxMixerInfo*, enum aaxFilterType);
_filter_t* new_filter_handle(const aaxConfig, enum aaxFilterType, _aax2dProps*, _aax3dProps*);
_filter_t* get_filter(aaxFilter);

extern const _flt_cvt_tbl_t _flt_cvt_tbl[AAX_FILTER_MAX];


typedef aaxFilter _aaxFilterCreate(_aaxMixerInfo*, enum aaxFilterType);
typedef int _aaxFilterDestroy(_filter_t*);
typedef aaxFilter _aaxFilterSetState(_filter_t*, int);
typedef _filter_t* _aaxNewFilterHandle(const aaxConfig, enum aaxFilterType, _aax2dProps*, _aax3dProps*);
typedef float _aaxFilterConvert(float, int, unsigned char);

typedef struct
{
   char lite;
   const char *name;
   float version;
   _aaxFilterCreate *create;
   _aaxFilterDestroy *destroy;
   _aaxFilterSetState *state;
   _aaxNewFilterHandle *handle;

   _aaxFilterConvert *set;
   _aaxFilterConvert *get;
   _aaxFilterConvert *limit;

} _flt_function_tbl;

extern _flt_function_tbl _aaxEqualizer;
extern _flt_function_tbl _aaxGraphicEqualizer;
extern _flt_function_tbl _aaxCompressor;
extern _flt_function_tbl _aaxVolumeFilter;
extern _flt_function_tbl _aaxFrequencyFilter;
extern _flt_function_tbl _aaxDynamicGainFilter;
extern _flt_function_tbl _aaxTimedGainFilter;
extern _flt_function_tbl _aaxAngularFilter;
extern _flt_function_tbl _aaxDistanceFilter;
extern _flt_function_tbl *_aaxFilters[AAX_FILTER_MAX];

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

#endif /* _AAX_FILTERS_H */

