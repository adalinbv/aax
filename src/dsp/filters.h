/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
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

#include <backends/driver.h>
#include "objects.h"
#include "common.h"
#include "api.h"

#define MIN_CUTOFF		20.0f
#define MAX_CUTOFF		20000.0f

aaxFilter _aaxFilterCreateHandle(_aaxMixerInfo*, enum aaxFilterType, unsigned, size_t);
bool _aaxFilterDestroy(aaxFilter*);

void _aaxSetDefaultFilter2d(_aaxFilterInfo*, unsigned int, unsigned slot);
void _aaxSetDefaultFilter3d(_aaxFilterInfo*, unsigned int, unsigned slot);
void _aaxSetDefaultEqualizer(_aaxFilterInfo filter[MAX_FILTERS]);

typedef struct
{
   fx4_t min;
   fx4_t max;

} _flt_minmax_tbl_t;

typedef struct
{
   unsigned int id;
   int pos;
   uint64_t state;
   enum aaxFilterType type;
   _aaxFilterInfo* slot[_MAX_FE_SLOTS];
   _aaxMixerInfo* info;
   _handle_t *handle;
} _filter_t;

_filter_t* new_filter_handle(const void*, enum aaxFilterType, _aax2dProps*, _aax3dProps*);
_filter_t* get_filter(aaxFilter);
void reset_filter(_aax2dProps*, enum _aax2dFiltersEffects);


typedef aaxFilter _aaxFilterCreateFn(_aaxMixerInfo*, enum aaxFilterType);
typedef bool _aaxFilterDestroyFn(aaxFilter*);
typedef void _aaxFilterResetFn(_filter_t*);
typedef aaxFilter _aaxFilterSetStateFn(_filter_t*, int);
typedef _filter_t* _aaxNewFilterHandleFn(const void*, enum aaxFilterType, _aax2dProps*, _aax3dProps*);
typedef float _aaxFilterConvertFn(float, int, unsigned char);

typedef struct
{
   const char *name;
   float version;
   _aaxFilterCreateFn *create;
   _aaxFilterDestroyFn *destroy;
   _aaxFilterResetFn *reset;
   _aaxFilterSetStateFn *state;
   _aaxNewFilterHandleFn *handle;

   _aaxFilterConvertFn *set;
   _aaxFilterConvertFn *get;
   _aaxFilterConvertFn *limit;

} _flt_function_tbl;

extern _flt_function_tbl _aaxEqualizer;
extern _flt_function_tbl _aaxGraphicEqualizer;
extern _flt_function_tbl _aaxCompressor;
extern _flt_function_tbl _aaxVolumeFilter;
extern _flt_function_tbl _aaxFrequencyFilter;
extern _flt_function_tbl _aaxDynamicGainFilter;
extern _flt_function_tbl _aaxBitCrusherFilter;
extern _flt_function_tbl _aaxTimedGainFilter;
extern _flt_function_tbl _aaxDirectionalFilter;
extern _flt_function_tbl _aaxDistanceFilter;
extern _flt_function_tbl _aaxDynamicTimbreFilter;
extern _flt_function_tbl *_aaxFilters[AAX_FILTER_MAX];

/* filters */
#define _FILTER_GET_SLOT(F, s, p)       F->slot[s]->param[p]
#define _FILTER_GET_SLOT_STATE(F)       F->slot[0]->state
#define _FILTER_GET_SLOT_UPDATED(E)     F->slot[0]->updated
#define _FILTER_GET_SLOT_DATA(F, s)     F->slot[s]->data
#define _FILTER_SET_SLOT(F, s, p, v)    F->slot[s]->param[p] = v
#define _FILTER_SET_SLOT_DATA(F, s, v)  F->slot[s]->data = v
#define _FILTER_SET_SLOT_UPDATED(E)     if (!F->slot[0]->updated) F->slot[0]->updated = 1

#define _FILTER_GET(P, f, p)            P->filter[f].param[p]
#define _FILTER_GET_STATE(P, f)         P->filter[f].state
#define _FILTER_GET_UPDATED(P, f)       P->filter[f].updated
#define _FILTER_GET_DATA(P, f)          P->filter[f].data
#define _FILTER_FREE_DATA(P, f)		if (P->filter[f].destroy) P->filter[f].destroy(P->filter[f].data)
#define _FILTER_SET(P, f, p, v)         P->filter[f].param[p] = v
#define _FILTER_SET_STATE(P, f, v)      P->filter[f].state = v
#define _FILTER_SET_UPDATED(P, f, v)    P->filter[f].updated = v
#define _FILTER_SET_DATA(P, f, v)       P->filter[f].data = v
#define _FILTER_COPY(P1, P2, f, p)      P1->filter[f].param[p] = P2->filter[f].param[p]
#define _FILTER_COPY_DATA(P1, P2, f)    P1->filter[f].data = P2->filter[f].data
#define _FILTER_COPY_STATE(P1, P2, f)   P1->filter[f].state = P2->filter[f].state

#define _FILTER_GET2D(G, f, p)          _FILTER_GET(G->props2d, f, p)
#define _FILTER_GET2D_DATA(G, f)        _FILTER_GET_DATA(G->props2d, f)
#define _FILTER_FREE2D_DATA(G, f)	_FILTER_FREE_DATA(G->props2d, f)
#define _FILTER_GET3D(G, f, p)          _FILTER_GET(G->dprops3d, f, p)
#define _FILTER_GET3D_DATA(G, f)        _FILTER_GET_DATA(G->dprops3d, f)
#define _FILTER_FREE3D_DATA(G, f)	_FILTER_FREE_DATA(G->props3d, f)
#define _FILTER_SET2D(G, f, p, v)       _FILTER_SET(G->props2d, f, p, v)
#define _FILTER_SET2D_DATA(G, f, v)     _FILTER_SET_DATA(G->props2d, f, v)
#define _FILTER_SET3D(G, f, p, v)       _FILTER_SET(G->dprops3d, f, p, v)
#define _FILTER_SET3D_DATA(G, f, v)     _FILTER_SET_DATA(G->dprops3d, f, v)
#define _FILTER_COPY2D_DATA(G1, G2, f)  _FILTER_COPY_DATA(G1->props2d, G2->props2d, f)
#define _FILTER_COPY3D_DATA(G1, G2, f)  _FILTER_COPY_DATA(G1->dprops3d, G2->dprops3d, f)

#define _FILTER_GETD3D(G, f, p)         _FILTER_GET(G->props3d, f, p)
#define _FILTER_SETD3D_DATA(G, f, v)    _FILTER_SET_DATA(G->props3d, f, v)
#define _FILTER_COPYD3D_DATA(G1, G2, f) _FILTER_COPY_DATA(G1->props3d, G2->props3d, f)

#define _FILTER_SWAP_SLOT_DATA(P, t, F, s) do {                                \
 if (F->slot[s]->swap) F->slot[s]->swap(&P->filter[t], F->slot[s]);            \
 P->filter[t].destroy = F->slot[s]->destroy;                                   \
 if (!s) aaxFilterSetState(F, P->filter[t].state); } while (0)

#define _FILTER_SWAP_SLOT(P, t, F, s)                                          \
 _FILTER_SET(P, t, 0, _FILTER_GET_SLOT(F, s, 0));                              \
 _FILTER_SET(P, t, 1, _FILTER_GET_SLOT(F, s, 1));                              \
 _FILTER_SET(P, t, 2, _FILTER_GET_SLOT(F, s, 2));                              \
 _FILTER_SET(P, t, 3, _FILTER_GET_SLOT(F, s, 3));                              \
 _FILTER_SET_STATE(P, t, _FILTER_GET_SLOT_STATE(F));                           \
 _FILTER_SWAP_SLOT_DATA(P, t, F, s)


// distance filter
void _distance_swap(void*,void*);
void _distance_destroy(void*);
float _distance_prepare(_aax2dProps*, _aax3dProps*, _aaxDelayed3dProps*, vec3f_ptr, float, const vec4f_ptr, const _aaxMixerInfo*);

// directional filter
float _directional_prepare(_aax3dProps*,  _aaxDelayed3dProps*, _aaxDelayed3dProps*);

#endif /* _AAX_FILTERS_H */
