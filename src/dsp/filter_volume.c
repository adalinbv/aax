/*
 * Copyright 2007-2018 by Erik Hofman.
 * Copyright 2009-2018 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <stdlib.h>
# include <malloc.h>
#endif

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include <software/rbuf_int.h>
#include "common.h"
#include "filters.h"
#include "effects.h"
#include "arch.h"
#include "api.h"

_aaxRingBufferOcclusionData* _occlusion_create(_aaxRingBufferOcclusionData*, _aaxFilterInfo*, int, float);
void _occlusion_prepare(_aaxEmitter*, _aax3dProps*, float);
void _occlusion_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, MIX_PTR_T, size_t, unsigned int, const void*);
void _freqfilter_run(void*, MIX_PTR_T, CONST_MIX_PTR_T, size_t, size_t, size_t, unsigned int, void*, void*, unsigned char);

static aaxFilter
_aaxVolumeFilterCreate(_aaxMixerInfo *info, enum aaxFilterType type)
{
   _filter_t* flt = _aaxFilterCreateHandle(info, type, 2);
   aaxFilter rv = NULL;

   if (flt)
   {
      _aaxSetDefaultFilter3d(flt->slot[0], flt->pos, 0);
      _aaxSetDefaultFilter3d(flt->slot[1], flt->pos, 1);
      flt->slot[0]->destroy = aligned_destroy;
      rv = (aaxFilter)flt;
   }
   return rv;
}

static int
_aaxVolumeFilterDestroy(_filter_t* filter)
{
   filter->slot[0]->destroy(filter->slot[0]->data);
   free(filter);

   return AAX_TRUE;
}

static aaxFilter
_aaxVolumeFilterSetState(_filter_t* filter, int state)
{
   float  fs = 48000.0f;

   if (filter->info) {
      fs = filter->info->frequency;
   }

   filter->slot[0]->data = _occlusion_create(filter->slot[0]->data, filter->slot[1], state, fs);

   return filter;
}

static _filter_t*
_aaxNewVolumeFilterHandle(const aaxConfig config, enum aaxFilterType type, UNUSED(_aax2dProps* p2d), _aax3dProps* p3d)
{
   _handle_t *handle = get_driver_handle(config);
   _aaxMixerInfo* info = handle ? handle->info : _info;
   _filter_t* rv = _aaxFilterCreateHandle(info, type, 2);

   if (rv)
   {
      unsigned int size = sizeof(_aaxFilterInfo);

      memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
      memcpy(rv->slot[1], &p3d->filter[rv->pos], size);
      rv->slot[0]->destroy = aligned_destroy;
      rv->slot[0]->data = NULL;

      rv->state = p3d->filter[rv->pos].state;
   }
   return rv;
}

static float
_aaxVolumeFilterSet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (ptype == AAX_LOGARITHMIC) {
      rv = _lin2db(val);
   }
   return rv;
}

static float
_aaxVolumeFilterGet(float val, int ptype, UNUSED(unsigned char param))
{
   float rv = val;
   if (param < 3 && ptype == AAX_LOGARITHMIC) {
      rv = _db2lin(val);
   }
   return rv;
}

static float
_aaxVolumeFilterMinMax(float val, int slot, unsigned char param)
{
  static const _flt_minmax_tbl_t _aaxVolumeRange[_MAX_FE_SLOTS] =
   {    /* min[4] */                  /* max[4] */
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {   10.0f,    1.0f,    10.0f, 10.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, { FLT_MAX, FLT_MAX,  FLT_MAX,  1.0f } }, 
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f,     0.0f,  0.0f } },
    { { 0.0f, 0.0f, 0.0f, 0.0f }, {    0.0f,    0.0f,     0.0f,  0.0f } }
   };
   
   assert(slot < _MAX_FE_SLOTS);
   assert(param < 4);
   
   return _MINMAX(val, _aaxVolumeRange[slot].min[param],
                       _aaxVolumeRange[slot].max[param]);
}

/* -------------------------------------------------------------------------- */

_flt_function_tbl _aaxVolumeFilter =
{
   AAX_TRUE,
   "AAX_volume_filter_1.1", 1.1f,
   (_aaxFilterCreate*)&_aaxVolumeFilterCreate,
   (_aaxFilterDestroy*)&_aaxVolumeFilterDestroy,
   (_aaxFilterSetState*)&_aaxVolumeFilterSetState,
   (_aaxNewFilterHandle*)&_aaxNewVolumeFilterHandle,
   (_aaxFilterConvert*)&_aaxVolumeFilterSet,
   (_aaxFilterConvert*)&_aaxVolumeFilterGet,
   (_aaxFilterConvert*)&_aaxVolumeFilterMinMax
};


#ifdef ARCH32
# define VEC3_T			vec3f_t
# define VEC4_T			vec4f_t
# define MTX4_T			mtx4f_t
# define VEC3COPY(a,b)		vec3fCopy(&a,b)
# define VEC3ALTITUDEVECTOR(a,b,c,d,e,f) vec3fAltitudeVector(a,b,c,d,e,f)
#else
# define VEC3_T			vec3d_t
# define VEC4_T			vec4d_t
# define MTX4_T			mtx4d_t
# define VEC3COPY(a,b)		vec3fFilld(a.v3,b.v3)
# define VEC3ALTITUDEVECTOR(a,b,c,d,e,f) vec3dAltitudeVector(a,b,c,d,e,f)
#endif

static float
_occlusion_lfo(void* data, UNUSED(void *env), UNUSED(const void *ptr), UNUSED(unsigned track), UNUSED(size_t num))
{
  _aaxLFOData* lfo = (_aaxLFOData*)data;
  _aaxRingBufferOcclusionData *occlusion;
  float rv, level;

  occlusion = lfo->data;

  level = occlusion->level*occlusion->level;
  occlusion->freq_filter.low_gain = 1.0f - level;
  
  rv = _linear(level, lfo->max-lfo->min);
  rv = lfo->inv ? lfo->max-rv : lfo->min+rv;

  return rv;
}


_aaxRingBufferOcclusionData*
_occlusion_create(_aaxRingBufferOcclusionData *occlusion, _aaxFilterInfo* slot,
                  int state, float fs)
{
   if (state != AAX_FALSE &&
       ((slot->param[0] >= 0.1f && slot->param[1] >= 0.1f) ||
        (slot->param[0] >= 0.1f && slot->param[2] >= 0.1f) ||
        (slot->param[1] >= 0.1f && slot->param[2] >= 0.1f)) &&
       slot->param[3] > LEVEL_64DB)
   {
      if (!occlusion) {
         occlusion = _aax_aligned_alloc(sizeof(_aaxRingBufferOcclusionData));
      }

      if (occlusion)
      {
         occlusion->prepare = _occlusion_prepare;
         occlusion->run = _occlusion_run;

         occlusion->occlusion.v4[0] = 0.5f*slot->param[0];
         occlusion->occlusion.v4[1] = 0.5f*slot->param[1];
         occlusion->occlusion.v4[2] = 0.5f*slot->param[2];
         occlusion->occlusion.v4[3] = slot->param[3];
         occlusion->magnitude_sq = vec3fMagnitudeSquared(&occlusion->occlusion.v3);
         occlusion->fc = 22000.0f;

         occlusion->level = 0.0f;
         occlusion->olevel = 0.0f;
         occlusion->inverse = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;

         memset(&occlusion->freq_filter, 0, sizeof(_aaxRingBufferFreqFilterData));
         occlusion->freq_filter.run = _freqfilter_run;
         occlusion->freq_filter.lfo = 0;
         occlusion->freq_filter.fs = fs;
         occlusion->freq_filter.Q = 0.6f;
         occlusion->freq_filter.low_gain = 1.0f;

         // 6db/Oct low-pass Bessel filter
         occlusion->freq_filter.type = LOWPASS;
         occlusion->freq_filter.no_stages = 0;
         occlusion->freq_filter.state = AAX_BESSEL;
         _aax_bessel_compute(occlusion->fc, &occlusion->freq_filter);

         occlusion->freq_filter.lfo = _lfo_create();
         if (occlusion->freq_filter.lfo)
         {
            _aaxLFOData* lfo = occlusion->freq_filter.lfo;

            lfo->data = occlusion;
            lfo->state = AAX_TRUE;
            lfo->fs = fs;
            lfo->period_rate = 1.0f/fs;
         
            lfo->min = 100.0f;
            lfo->max = 20000.0f;
         
            lfo->min_sec = lfo->min/lfo->fs;
            lfo->max_sec = lfo->max/lfo->fs;
            lfo->depth = 1.0f;
            lfo->offset = 0.0f;
            lfo->f = 0.6f;
            lfo->inv = AAX_TRUE;
            lfo->stereo_lnk = AAX_TRUE;
            lfo->envelope = AAX_FALSE;

            lfo->get = _occlusion_lfo;
         }
      }
   }
   return occlusion;
}

void
_occlusion_prepare(_aaxEmitter *src, _aax3dProps *fp3d, float vs)
{
   _aaxRingBufferOcclusionData *occlusion;
   _aaxDelayed3dProps *pdp3d_m, *fdp3d_m;
   _aaxDelayed3dProps *edp3d_m;
   _aax3dProps *ep3d;

   ep3d = src->props3d;
   edp3d_m = ep3d->m_dprops3d;
   fdp3d_m = fp3d->m_dprops3d;
   pdp3d_m = fp3d->parent ? fp3d->parent->m_dprops3d : NULL;

   if (pdp3d_m)
   {
      occlusion = _EFFECT_GET_DATA(fp3d, REVERB_EFFECT);
      if (!occlusion) occlusion = _FILTER_GET_DATA(fp3d, VOLUME_FILTER);
      if (occlusion)
      {
         vec3f_t afevec, altvec, fpvec, cfpvec, dim;
         _aaxRingBufferOcclusionData *cpath = occlusion;
         _aaxRingBufferOcclusionData *npath = NULL;
         _aaxDelayed3dProps *ndp3d_m;
         _aax3dProps *nfp3d;
         VEC3_T *e, *p;
         MTX4_T *f;
         int hit;

         nfp3d = fp3d->parent;
         assert(nfp3d->m_dprops3d == pdp3d_m);

         e = &edp3d_m->matrix.v34[LOCATION];
         f = &fdp3d_m->imatrix;

         hit = 0;
         occlusion->level = 0.0f;
         vec3fZero(&fpvec);
         do
         {
            // If the audio-frame has occlusion defined with a density
            // factor larger than zero then process it.
            if (cpath && (cpath->occlusion.v4[3] > 0.01f))
            {
               float density = cpath->occlusion.v4[3];
               int ahead;

               // calculate the sum of the current dimension vector and the
               // parent dimension vector, and test fpvec against that.
               vec3fCopy(&dim, &cpath->occlusion.v3);
               if (nfp3d)
               {
                  npath = _EFFECT_GET_DATA(nfp3d, REVERB_EFFECT);
                  if (!npath) npath = _FILTER_GET_DATA(nfp3d, VOLUME_FILTER);
                  if (npath) {
                     vec3fAdd(&dim, &dim, &npath->occlusion.v3);
                  }
               }

               ndp3d_m = nfp3d->m_dprops3d;
               p = (nfp3d==nfp3d->root) ? NULL : &ndp3d_m->matrix.v34[LOCATION];
#if 0
 printf("#   inverse frame:\t\tparent:\n");
 PRINT_MATRICES(fdp3d_m->imatrix, ndp3d_m->matrix);
 printf("# emitter:\n");
 PRINT_MATRIX(edp3d_m->matrix);
#endif

               // hit is true when the direct cpath does interstect
               // with the defined obstruction/cavity.
               ahead = VEC3ALTITUDEVECTOR(&altvec, f, p, e, &afevec, &cfpvec);
               vec3fSub(&fpvec, &cfpvec, &fpvec);
               hit = vec3fLessThan(&altvec, &cpath->occlusion.v3);
               if (hit)
               {
                  // In case cpath->inverse is true the real obstruction is
                  // everything but the defined dimensions (meaning a cavity).
                  if (cpath->inverse)
                  {
                     // Is the emitter inside the cavity?
                     hit = vec3fLessThan(&afevec, &cpath->occlusion.v3);
                     if (hit) {
                        hit = vec3fLessThan(&fpvec, &dim);
                     }
                     hit = !hit;
                  }
                  else if (ahead) {
                     hit = vec3fLessThan(&cfpvec, &dim);
                  }
               }
               else if (cpath->inverse) {
                  hit = !hit;
               }

               if (hit)
               {
                  float level, mag;

                  mag = sqrtf(cpath->magnitude_sq); // cpath->magnitude*100.0f/vs;
                  level = 1.0f - _MIN(vec3fMagnitude(&altvec)/mag, 1.0f);
                  if (cpath->inverse) level = _MIN(1.0f/level, 1.0f);

                  level *= density;
                  if (level > occlusion->level) {
                     occlusion->level = level;
                  }
#if 0
                  if (occlusion->level > (1.0f-LEVEL_64DB)) break;
               }
            }
#else
               }
 printf("       altitude vector:\t"); PRINT_VEC3(altvec);
 if (cpath->inverse) {
   printf("         frame-emitter:\t"); PRINT_VEC3(afevec);
 }
 if (ahead) {
   printf("   frame-parent vector:\t"); PRINT_VEC3(cfpvec);
 }
 printf("obstruction dimensions:\t\x1b[33m"); PRINT_VEC3(cpath->occlusion.v3);
 printf("\x1b[0m");
 if (cpath->inverse) {
   printf("#  frame-parent vector:\t"); PRINT_VEC3(fpvec);
   printf("#  combined dimensions:\t\x1b[33m"); PRINT_VEC3(dim);
   printf("\x1b[0m");
 }
 printf("           obstruction: hit: \x1b[31m%s\x1b[0m, level: \x1b[31m%f\x1b[0m, inverse?: \x1b[31m%i\x1b[0m\n\n", (hit^cpath->inverse)?"yes":"no ", occlusion->level, cpath->inverse);
               if (occlusion->level > (1.0f-LEVEL_64DB)) break;
            }
#endif
            nfp3d = nfp3d->parent;
            cpath = npath;
         }
         while (nfp3d);
      } /* occlusion != NULL */
   } /* pdp3d_m != NULL */
}

void
_occlusion_run(void *rb, MIX_PTR_T dptr, CONST_MIX_PTR_T sptr, UNUSED(MIX_PTR_T scratch), size_t samples, unsigned int track, const void *data)
{
   _aaxRingBufferOcclusionData *occlusion = (_aaxRingBufferOcclusionData*)data;
   _aaxRingBufferSample *rbd = (_aaxRingBufferSample*)rb;
   _aaxRingBufferFreqFilterData *freq_flt;

   /* add the direct path */
   assert(occlusion);

   freq_flt = &occlusion->freq_filter;
   freq_flt->run(rbd, scratch, sptr, 0, samples, 0, track, freq_flt, NULL, 0);
   rbd->add(dptr, scratch, samples, 1.0f, 0.0f);
}
