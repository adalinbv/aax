/*
 * Copyright 2007-2011 by Erik Hofman.
 * Copyright 2009-2011 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <aax/aax.h>

#include <base/types.h>		/* for rintf */
#include <base/gmath.h>

#include "api.h"

#define WRITEFN		0
#define EPS		1e-5
#define READFN		!WRITEFN

typedef struct {
  enum aaxFilterType type;
  int pos;
} _flt_cvt_tbl_t;
typedef struct {
   vec4_t min;
   vec4_t max;
} _flt_minmax_tbl_t;
typedef float (*cvtfn_t)(float);

static cvtfn_t get_cvtfn(enum aaxFilterType, int, int, char);

static const _flt_cvt_tbl_t _flt_cvt_tbl[AAX_FILTER_MAX];
static const _flt_minmax_tbl_t _flt_minmax_tbl[_MAX_SLOTS][AAX_FILTER_MAX];

AAX_API aaxFilter AAX_APIENTRY
aaxFilterCreate(aaxConfig config, enum aaxFilterType type)
{
   _handle_t *handle = get_handle(config);
   aaxFilter rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_filter_t);
     _filter_t* flt;

      switch (type)
      {
      case AAX_TIMED_GAIN_FILTER:		/* three slots */
         size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_oalRingBufferFilterInfo);
         break;
      case AAX_EQUALIZER:			/* two or more slots */
      case AAX_GRAPHIC_EQUALIZER:
         size += EQUALIZER_MAX*sizeof(_oalRingBufferFilterInfo);
         break;
      case AAX_FREQUENCY_FILTER:		/* two slots */
         size += sizeof(_oalRingBufferFilterInfo);
         /* break not needed */
      default:					/* one slot */
         size += sizeof(_oalRingBufferFilterInfo);
         break;
      }

      flt = calloc(1, size);
      if (flt)
      {
         char *ptr;
         int i;

         flt->id = FILTER_ID;
         flt->state = AAX_FALSE;
         flt->info = handle->info;

         ptr = (char*)flt + sizeof(_filter_t);
         flt->slot[0] = (_oalRingBufferFilterInfo*)ptr;
         flt->pos = _flt_cvt_tbl[type].pos;
         flt->type = type;

         size = sizeof(_oalRingBufferFilterInfo);
         switch (type)
         {
         case AAX_GRAPHIC_EQUALIZER:
            flt->slot[1] = (_oalRingBufferFilterInfo*)(ptr + size);
            flt->slot[0]->param[0] = 1.0f; flt->slot[1]->param[0] = 1.0f;
            flt->slot[0]->param[1] = 1.0f; flt->slot[1]->param[1] = 1.0f;
            flt->slot[0]->param[2] = 1.0f; flt->slot[1]->param[2] = 1.0f;
            flt->slot[0]->param[3] = 1.0f; flt->slot[1]->param[3] = 1.0f;
            break;
         case AAX_EQUALIZER:
            flt->slot[1] = (_oalRingBufferFilterInfo*)(ptr + size);
            _aaxSetDefaultFilter2d(flt->slot[1], flt->pos);
            /* break not needed */
         case AAX_FREQUENCY_FILTER:
            flt->slot[1] = (_oalRingBufferFilterInfo*)(ptr + size);
            /* break not needed */
         case AAX_VOLUME_FILTER:
         case AAX_DYNAMIC_GAIN_FILTER:
            _aaxSetDefaultFilter2d(flt->slot[0], flt->pos);
            break;
         case AAX_TIMED_GAIN_FILTER:
            for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
            {
               flt->slot[i] = (_oalRingBufferFilterInfo*)(ptr + i*size);
               _aaxSetDefaultFilter2d(flt->slot[i], flt->pos);
            }
            break;
         case AAX_ANGULAR_FILTER:
         case AAX_DISTANCE_FILTER:
            _aaxSetDefaultFilter3d(flt->slot[0], flt->pos);
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            free(flt);
            flt = NULL;
         }
      }
      else {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
      rv = (aaxFilter)flt;
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterDestroy(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter)
   {
      /*
       * If a filter is applied to an emitter, the scenery or an audioframe
       * filter->slot[0]->data will be copied and set to NULL. If not applied
       * we need to free the located memory ourselves.
       */
      switch (filter->type)
      {
      case AAX_GRAPHIC_EQUALIZER:
         free(filter->slot[1]->data);
         /* break not needed */
      case AAX_EQUALIZER:
      case AAX_FREQUENCY_FILTER:
         filter->slot[1]->data = NULL;
         /* break not needed */
      case AAX_TIMED_GAIN_FILTER:
      case AAX_DYNAMIC_GAIN_FILTER:
         free(filter->slot[0]->data);
         filter->slot[0]->data = NULL;
         break;
      default:
         break;
      }
      free(filter);
      rv = AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterSetSlot(aaxFilter f, unsigned slot, int ptype, float p1, float p2, float p3, float p4)
{
   aaxVec4f v = { p1, p2, p3, p4 };
   return aaxFilterSetSlotParams(f, slot, ptype, v);
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterSetSlotParams(aaxFilter f, unsigned slot, int ptype, aaxVec4f p)
{
   _filter_t* filter = get_filter(f);
   aaxFilter rv = AAX_FALSE;
   if (filter && p)
   {
      if ((slot < _MAX_SLOTS) && filter->slot[slot])
      {
         int i, type = filter->type;
         for (i=0; i<4; i++)
         {
            if (!is_nan(p[i]))
            {
               float min = _flt_minmax_tbl[slot][type].min[i];
               float max = _flt_minmax_tbl[slot][type].max[i];
               cvtfn_t cvtfn = get_cvtfn(filter->type, ptype, WRITEFN, i);
               filter->slot[slot]->param[i] = _MINMAX(cvtfn(p[i]), min, max);
            }
         }
         if TEST_FOR_TRUE(filter->state) {
            rv = aaxFilterSetState(filter, filter->state);
         } else {
            rv = filter;
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterSetParam(const aaxFilter f, int param, int ptype, float value)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter && !is_nan(value))
   {
      unsigned slot = param >> 4;
      if ((slot < _MAX_SLOTS) && filter->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            cvtfn_t cvtfn = get_cvtfn(filter->type, ptype, WRITEFN, param);
            filter->slot[slot]->param[param] = cvtfn(value);
            if TEST_FOR_TRUE(filter->state) {
               aaxFilterSetState(filter, filter->state);
            }
            rv = AAX_TRUE;
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterSetState(aaxFilter f, int state)
{
   _filter_t* filter = get_filter(f);
   aaxFilter rv = NULL;
   if (filter)
   {
      filter->state = state;
      filter->slot[0]->state = state;
      switch(filter->type)
      {
      case AAX_GRAPHIC_EQUALIZER:
#if !ENABLE_LITE
         if EBF_VALID(filter)
         {
            if TEST_FOR_TRUE(state)
            {
               /* use EQUALIZER_HF to distinquish between GRAPHIC_EQUALIZER
                * and FREQ_FILTER (which only uses EQUALIZER_LF)
                */
               _oalRingBufferEqualizerInfo *eq=filter->slot[EQUALIZER_HF]->data;
               if (eq == NULL)
               {
                  eq = calloc(1, sizeof(_oalRingBufferEqualizerInfo));
                  filter->slot[EQUALIZER_LF]->data = NULL;
                  filter->slot[EQUALIZER_HF]->data = eq;

                  if (eq)	/* fill in the fixed frequencies */
                  {
                     float fband = logf(44000.0f/67.0f)/8.0f;
                     int pos = 7;
                     do
                     {
                        _oalRingBufferFreqFilterInfo *flt;
                        float *cptr, fc, k, Q;

                        flt = &eq->band[pos];
                        cptr = flt->coeff;

                        k = 1.0f;
                        Q = 2.0f;
                        fc = expf((float)pos*fband)*67.0f;
                        iir_compute_coefs(fc,filter->info->frequency,cptr,&k,Q);
                        flt->k = k;
                     }
                     while (--pos >= 0);
                  }
               }

               if (eq)		/* fill in the gains */
               {
                  float gain_hf=filter->slot[EQUALIZER_HF]->param[AAX_GAIN_BAND3];
                  float gain_lf=filter->slot[EQUALIZER_HF]->param[AAX_GAIN_BAND2];
                  _oalRingBufferFreqFilterInfo *flt = &eq->band[6];
                  int s = EQUALIZER_HF, b = AAX_GAIN_BAND2;

                  eq = filter->slot[EQUALIZER_HF]->data;
                  flt->hf_gain = gain_hf;
                  flt->lf_gain = gain_lf;
                  do
                  {
                     int pos = s*4+b;
                     float gain;

                     flt = &eq->band[pos-1];

                     gain_hf = gain_lf;
                     gain_lf = filter->slot[s]->param[--b];

                     /* gain_hf can never get below 0.001f */
                     gain = gain_lf - gain_hf;
                     flt->hf_gain = 0.0f;
                     flt->lf_gain = gain;

                     if (b == 0)
                     {
                        b += 4;
                        s--;
                     }
                  }
                  while (s >= 0);
               }
               else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
            else {
               filter->slot[0]->data = NULL;
            }
         }
#endif
         break;
      case AAX_EQUALIZER:
#if !ENABLE_LITE
         if EBF_VALID(filter)
         {
            if TEST_FOR_TRUE(state)
            {
               _oalRingBufferFreqFilterInfo *flt=filter->slot[EQUALIZER_LF]->data;
               if (flt == NULL)
               {
                  char *ptr;
                  flt=calloc(EQUALIZER_MAX,sizeof(_oalRingBufferFreqFilterInfo));
                  filter->slot[EQUALIZER_LF]->data = flt;

                  ptr = (char*)flt + sizeof(_oalRingBufferFreqFilterInfo);
                  flt = (_oalRingBufferFreqFilterInfo*)ptr;
                  filter->slot[EQUALIZER_HF]->data = flt;
               }

               if (flt)
               {
                  float *cptr, fcl, fch, k, Q;

                  fcl = filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY];
                  fch = filter->slot[EQUALIZER_HF]->param[AAX_CUTOFF_FREQUENCY];
                  if (fabsf(fch - fcl) < 200.0f) {
                     fcl *= 0.9f; fch *= 1.1f;
                  } else if (fch < fcl) {
                     float f = fch; fch = fcl; fcl = f;
                  }
                  filter->slot[EQUALIZER_LF]->param[AAX_CUTOFF_FREQUENCY] = fcl;

                  /* LF frequency setup */
                  flt = filter->slot[EQUALIZER_LF]->data;
                  cptr = flt->coeff;
                  k = 1.0f;
                  Q = filter->slot[EQUALIZER_LF]->param[AAX_RESONANCE];
                  iir_compute_coefs(fcl, filter->info->frequency, cptr, &k, Q);
                  flt->lf_gain = filter->slot[EQUALIZER_LF]->param[AAX_LF_GAIN];
                  flt->hf_gain = filter->slot[EQUALIZER_LF]->param[AAX_HF_GAIN];
                  flt->k = k;

                  /* HF frequency setup */
                  flt = filter->slot[EQUALIZER_HF]->data;
                  cptr = flt->coeff;
                  k = 1.0f;
                  Q = filter->slot[EQUALIZER_HF]->param[AAX_RESONANCE];
                  iir_compute_coefs(fch, filter->info->frequency, cptr, &k, Q);
                  flt->lf_gain = filter->slot[EQUALIZER_HF]->param[AAX_LF_GAIN];
                  flt->hf_gain = filter->slot[EQUALIZER_HF]->param[AAX_HF_GAIN];
                  flt->k = k;
               }
               else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
            else {
               filter->slot[0]->data = NULL;
            }
         }
#endif
         break;
      case AAX_TIMED_GAIN_FILTER:
#if !ENABLE_LITE
         if EBF_VALID(filter)
         {
            if TEST_FOR_TRUE(state)
            {
               _oalRingBufferEnvelopeInfo* env = filter->slot[0]->data;
               if (env == NULL)
               {
                  env =  calloc(1, sizeof(_oalRingBufferEnvelopeInfo));
                  filter->slot[0]->data = env;
               }

               if (env)
               {
                  float nextval = filter->slot[0]->param[AAX_LEVEL0];
                  float refresh = filter->info->refresh_rate;
                  float timestep = 1.0f / refresh;
                  int i;

                  env->value = nextval;
                  env->max_stages = _MAX_ENVELOPE_STAGES;
                  for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
                  {
                     float dt, value = nextval;
                     uint32_t max_pos;

                     max_pos = (uint32_t)-1;
                     dt = filter->slot[i]->param[AAX_TIME0];
                     if (dt != MAXFLOAT)
                     {
                        if (dt < timestep && dt > EPS) dt = timestep;
                        max_pos = rintf(dt * refresh);
                     }
                     if (max_pos == 0)
                     {
                        env->max_stages = 2*i;
                        break;
                     }

                     nextval = filter->slot[i]->param[AAX_LEVEL1];
                     if (nextval == 0.0f) nextval = -1e-2f;
                     env->step[2*i] = (nextval - value)/max_pos;
                     env->max_pos[2*i] = max_pos;

                     /* prevent a core dump for accessing an illegal slot */
                     if (i == (_MAX_ENVELOPE_STAGES/2)-1) break;
                 
                     max_pos = (uint32_t)-1;
                     dt = filter->slot[i]->param[AAX_TIME1];
                     if (dt != MAXFLOAT)
                     {
                        if (dt < timestep && dt > EPS) dt = timestep;
                        max_pos = rintf(dt * refresh);
                     } 
                     if (max_pos == 0)
                     {
                        env->max_stages = 2*i+1;
                        break;
                     }

                     value = nextval;
                     nextval = filter->slot[i+1]->param[AAX_LEVEL0];
                     if (nextval == 0.0f) nextval = -1e-2f;
                     env->step[2*i+1] = (nextval - value)/max_pos;
                     env->max_pos[2*i+1] = max_pos;
                  }
               }
               else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
            else {
               filter->slot[0]->data = NULL;
            }
         }
#endif
         break;
      case AAX_DYNAMIC_GAIN_FILTER:
#if !ENABLE_LITE
         if EBF_VALID(filter)
         {
            switch (state & ~AAX_INVERSE)
            {
            case AAX_CONSTANT_VALUE:
            case AAX_TRIANGLE_WAVE:
            case AAX_SINE_WAVE:
            case AAX_SQUARE_WAVE:
            case AAX_SAWTOOTH_WAVE:
            case AAX_ENVELOPE_FOLLOW:
            {
               _oalRingBufferLFOInfo* lfo = filter->slot[0]->data;
               if (lfo == NULL)
               {
                  lfo = malloc(sizeof(_oalRingBufferLFOInfo));
                  filter->slot[0]->data = lfo;
               }

               if (lfo)
               {
                  float depth = filter->slot[0]->param[AAX_LFO_DEPTH];
                  int t;

                  lfo->min = 0.0f;
                  lfo->max = 2.0f * depth;
                  lfo->envelope = AAX_FALSE;
                  lfo->f = filter->slot[0]->param[AAX_LFO_FREQUENCY];
                  lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
                  lfo->convert = _lin;

                  for (t=0; t<_AAX_MAX_SPEAKERS; t++)
                  {
                     lfo->step[t] = 2.0f*depth * lfo->f;
                     lfo->step[t] /= filter->info->refresh_rate;
                     lfo->value[t] = 1.0f;
                     switch (state & ~AAX_INVERSE)
                     {
                     case AAX_SAWTOOTH_WAVE:
                        lfo->step[t] *= 0.5f;
                        break;
                     case AAX_ENVELOPE_FOLLOW:
                        lfo->value[t] /= lfo->max;
                        lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
                        break;
                     default:
                        break;
                     }
                  }

                  if (depth > 0.01f)
                  {
                     switch (state & ~AAX_INVERSE)
                     {
                     case AAX_CONSTANT_VALUE: /* equals to AAX_TRUE */
                        lfo->get = _oalRingBufferLFOGetFixedValue;
                        break;
                     case AAX_TRIANGLE_WAVE:
                        lfo->get = _oalRingBufferLFOGetTriangle;
                        break;
                     case AAX_SINE_WAVE:
                        lfo->get = _oalRingBufferLFOGetSine;
                        break;
                     case AAX_SQUARE_WAVE:
                        lfo->get = _oalRingBufferLFOGetSquare;
                        break;
                     case AAX_SAWTOOTH_WAVE:
                        lfo->get = _oalRingBufferLFOGetSawtooth;
                        break;
                     case AAX_ENVELOPE_FOLLOW:
                        lfo->get = _oalRingBufferLFOGetEnvelopeFollow;
                        lfo->envelope = AAX_TRUE;
                        break;
                     default:
                        break;
                     }
                  } else {
                     lfo->get = _oalRingBufferLFOGetFixedValue;
                  }
               }
               else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
               break;
            }
            case AAX_FALSE:
               filter->slot[0]->data = NULL;
               break;
            default:
               _aaxErrorSet(AAX_INVALID_PARAMETER);
               break;
            }
         }
#endif
         break;
      case AAX_FREQUENCY_FILTER:
         switch (state & ~AAX_INVERSE)
         {
         case AAX_TRUE:
         case AAX_TRIANGLE_WAVE:
         case AAX_SINE_WAVE:
         case AAX_SQUARE_WAVE:
         case AAX_SAWTOOTH_WAVE:
         case AAX_ENVELOPE_FOLLOW:
         {
            _oalRingBufferFreqFilterInfo *flt = filter->slot[0]->data;
            if (flt == NULL)
            {
               flt = calloc(1, sizeof(_oalRingBufferFreqFilterInfo));
               filter->slot[0]->data = flt;
            }

            if (flt)
            {
               float fc = filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
               float Q = filter->slot[0]->param[AAX_RESONANCE];
               float *cptr = flt->coeff;
               float frequency = 48000.0f; 
               float k = 1.0f;

               if (filter->info) {
                  frequency = filter->info->frequency;
               }
               iir_compute_coefs(fc, frequency, cptr, &k, Q);
               flt->lf_gain = filter->slot[0]->param[AAX_LF_GAIN];
               flt->hf_gain = filter->slot[0]->param[AAX_HF_GAIN];
               flt->fs = frequency;
               flt->Q = Q;
               flt->k = k;

               if ((state & ~AAX_INVERSE) != AAX_TRUE && EBF_VALID(filter)
                   && filter->slot[1])
               {
                  _oalRingBufferLFOInfo* lfo = flt->lfo;

                  if (lfo == NULL) {
                     lfo = flt->lfo = malloc(sizeof(_oalRingBufferLFOInfo));
                  }

                  if (lfo)
                  {
                     int t;

                     lfo->min = filter->slot[0]->param[AAX_CUTOFF_FREQUENCY];
                     lfo->max = filter->slot[1]->param[AAX_CUTOFF_FREQUENCY];
                     if (fabsf(lfo->max - lfo->min) < 200.0f)
                     { 
                        lfo->min *= 0.9f;
                        lfo->max *= 1.1f;
                     }
                     else if (lfo->max < lfo->min)
                     {
                        float f = lfo->max;
                        lfo->max = lfo->min;
                        lfo->min = f;
                     }
                     filter->slot[0]->param[AAX_CUTOFF_FREQUENCY] = lfo->min;

                     /* sweeprate */
                     lfo->f = filter->slot[1]->param[AAX_RESONANCE];
                     lfo->inv = (state & AAX_INVERSE) ? AAX_TRUE : AAX_FALSE;
                     lfo->envelope = AAX_FALSE;
                     lfo->convert = _lin; // _log2lin;

                     for (t=0; t<_AAX_MAX_SPEAKERS; t++)
                     {
                        float step = 2.0f*(lfo->max - lfo->min) * lfo->f;
                        step /= filter->info->refresh_rate;
                        lfo->step[t] = step;
                        lfo->value[t] = lfo->max;
                        switch (state & ~AAX_INVERSE)
                        {
                        case AAX_SAWTOOTH_WAVE:
                           lfo->step[t] *= 0.5f;
                           break;
                        case AAX_ENVELOPE_FOLLOW:
                           lfo->value[t] /= lfo->max;
                           lfo->step[t] = ENVELOPE_FOLLOW_STEP_CVT(lfo->f);
                           break;
                        default:
                           break;
                        }
                     }

                     if ((lfo->max - lfo->min) > 0.01f)
                     {
                        switch (state & ~AAX_INVERSE)
                        {
                        case AAX_TRIANGLE_WAVE:
                           lfo->get = _oalRingBufferLFOGetTriangle;
                           break;
                        case AAX_SINE_WAVE:
                           lfo->get = _oalRingBufferLFOGetSine;
                           break;
                        case AAX_SQUARE_WAVE:
                           lfo->get = _oalRingBufferLFOGetSquare;
                           break;
                        case AAX_SAWTOOTH_WAVE:
                           lfo->get = _oalRingBufferLFOGetSawtooth;
                           break;
                        case AAX_ENVELOPE_FOLLOW:
                           lfo->get = _oalRingBufferLFOGetEnvelopeFollow;
                           lfo->envelope = AAX_TRUE;
                           break;
                        default:
                           break;
                        }
                     } else {
                        lfo->get = _oalRingBufferLFOGetFixedValue;
                     }
                  } /* flt->lfo */
               } /* flt */
            }
            else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            break;
         }
         case AAX_FALSE:
            filter->slot[0]->data = NULL;
            break;
         default:
            _aaxErrorSet(AAX_INVALID_PARAMETER);
            break;
         }
         break;
      case AAX_DISTANCE_FILTER:
         if (state < AAX_AL_DISTANCE_MODEL_MAX)
         {
            int pos = 0;
            if ((state >= AAX_AL_INVERSE_DISTANCE)
                && (state < AAX_AL_DISTANCE_MODEL_MAX))
            {
               pos = state - AAX_AL_INVERSE_DISTANCE;
               filter->slot[0]->data = _oalRingBufferALDistanceFunc[pos];
            }
            else if ((state & ~0x80) < AAX_DISTANCE_MODEL_MAX)
            {
               pos = state & ~0x80;
               if (state & 0x80) {	/* distance delay enabled */
                  filter->slot[0]->param[AAX_ROLLOFF_FACTOR] *= -1;
               }
               filter->slot[0]->data = _oalRingBufferDistanceFunc[pos];
            }
            else _aaxErrorSet(AAX_INVALID_PARAMETER);
         }
         else _aaxErrorSet(AAX_INVALID_PARAMETER);
         break;
      default:
         _aaxErrorSet(AAX_INVALID_STATE);
      }
      rv = filter;
   }
   else if (!filter) {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   } else { /* !filter->info */
      _aaxErrorSet(AAX_INVALID_STATE);
   }
   return rv;
}

AAX_API int AAX_APIENTRY
aaxFilterGetState(aaxFilter f)
{
   _filter_t* filter = get_filter(f);
   int rv = AAX_FALSE;
   if (filter) {
      rv = filter->state;
   }
   return rv;
}

AAX_API float AAX_APIENTRY
aaxFilterGetParam(const aaxFilter f, int param, int ptype)
{
   _filter_t* filter = get_filter(f);
   float rv = 0.0f;
   if (filter)
   {
      int slot = param >> 4;
      if ((slot < _MAX_SLOTS) && filter->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            cvtfn_t cvtfn = get_cvtfn(filter->type, ptype, READFN, param);
            rv = cvtfn(filter->slot[slot]->param[param]);
         }
         else {
            _aaxErrorSet(AAX_INVALID_PARAMETER + 1);
         }
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterGetSlot(const aaxFilter f, unsigned slot, int ptype, float* p1, float* p2, float* p3, float* p4)
{
   aaxVec4f v;
   aaxFilter rv = aaxEffectGetSlotParams(f, slot, ptype, v);
   if(p1) *p1 = v[0];
   if(p2) *p2 = v[1];
   if(p3) *p3 = v[2];
   if(p4) *p4 = v[3];
   return rv;
}

AAX_API aaxFilter AAX_APIENTRY
aaxFilterGetSlotParams(const aaxFilter f, unsigned slot, int ptype, aaxVec4f p)
{
   aaxFilter rv = AAX_FALSE;
   _filter_t* filter = get_filter(f);
   if (filter && p)
   {
      if ((slot < _MAX_SLOTS) && filter->slot[slot])
      {
         int i;
         for (i=0; i<4; i++)
         {
            cvtfn_t cvtfn = get_cvtfn(filter->type, ptype, READFN, i);
            p[i] = cvtfn(filter->slot[slot]->param[i]);
         }
         rv = filter;
      }
      else {
         _aaxErrorSet(AAX_INVALID_PARAMETER);
      }
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

/* -------------------------------------------------------------------------- */

static const _flt_cvt_tbl_t _flt_cvt_tbl[AAX_FILTER_MAX] = 
{
  { AAX_FILTER_NONE,		MAX_STEREO_FILTER },
  { AAX_EQUALIZER,		FREQUENCY_FILTER },
  { AAX_VOLUME_FILTER,		VOLUME_FILTER },
  { AAX_DYNAMIC_GAIN_FILTER,	DYNAMIC_GAIN_FILTER },
  { AAX_TIMED_GAIN_FILTER,	TIMED_GAIN_FILTER },
  { AAX_ANGULAR_FILTER,		ANGULAR_FILTER },
  { AAX_DISTANCE_FILTER,	DISTANCE_FILTER },
  { AAX_FREQUENCY_FILTER,	FREQUENCY_FILTER },
  { AAX_GRAPHIC_EQUALIZER,	FREQUENCY_FILTER }
};

/* see above for the proper sequence */
static const _flt_minmax_tbl_t _flt_minmax_tbl[_MAX_SLOTS][AAX_FILTER_MAX] =
{   /* min[4] */	  /* max[4] */
  {
    /* AAX_FILTER_NONE      */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,  0.0f,     0.0f } },
    /* AAX_EQUALIZER        */
    { { 20.0f,  0.0f, 0.0f, 1.0f }, { 22050.0f,    10.0f, 10.0f,   100.0f } },
    /* AAX_VOLUME_FILTER    */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {     1.0f,     1.0f, 10.0f,     0.0f } },
    /* AAX_DYNAMIC_GAIN_FILTER   */
    { {  0.0f, 0.01f, 0.0f, 0.0f }, {     0.0f,    10.0f, 10.0f,     1.0f } },
    /* AAX_TIMED_GAIN_FILTER */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT,  4.0f, MAXFLOAT } },
    /* AAX_ANGULAR_FILTER   */
    { { -1.0f, -1.0f, 0.0f, 0.0f }, {     1.0f,     1.0f,  1.0f,     0.0f } },
    /* AAX_DISTANCE_FILTER  */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, { MAXFLOAT, MAXFLOAT,  1.0f,     0.0f } },
    /* AAX_FREQUENCY_FILTER */
    { { 20.0f,  0.0f, 0.0f, 1.0f }, { 22050.0f,    10.0f, 10.0f,   100.0f } },
    /* AAX_GRAPHIC_EQUALIZER */
    { {  0.0f,  0.0f, 0.0f, 0.0f }, {    2.0f,      2.0f,  2.0f,     2.0f } },
  },
  {
     /* AAX_FILTER_NONE      */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_EQUALIZER        */
     { { 20.0f,  0.0f, 0.0f, 1.0f }, { 22050.0f,    10.0f,    10.0f, 100.0f } },
     /* AAX_VOLUME_FILTER    */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DYNAMIC_GAIN_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_TIMED_GAIN_FILTER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT,   4.0f, MAXFLOAT } },
     /* AAX_ANGULAR_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DISTANCE_FILTER  */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_FREQUENCY_FILTER */
     { { 20.0f, 0.0f, 0.0f, 0.01f }, { 22050.0f,     1.0f,     1.0f,  10.0f } },
     /* AAX_GRAPHIC_EQUALIZER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {    2.0f,      2.0f,     2.0f,   2.0f } }
  },
  {
     /* AAX_FILTER_NONE      */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_EQUALIZER        */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_VOLUME_FILTER    */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DYNAMIC_GAIN_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_TIMED_GAIN_FILTER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT,   4.0f, MAXFLOAT } },
     /* AAX_ANGULAR_FILTER   */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_DISTANCE_FILTER  */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_FREQUENCY_FILTER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } },
     /* AAX_GRAPHIC_EQUALIZER */
     { {  0.0f,  0.0f, 0.0f, 0.0f }, {     0.0f,     0.0f,     0.0f,   0.0f } }
  }
};

/* internal use only, used by aaxdefs.h */
AAX_API aaxFilter AAX_APIENTRY
aaxFilterApply(aaxFilterFn fn, void *handle, aaxFilter f)
{
   if (f)
   {
      if (!fn(handle, f))
      {
         aaxFilterDestroy(f);
         f = NULL;
      }
   }
   return f;
}

AAX_API float AAX_APIENTRY
aaxFilterApplyParam(const aaxFilter f, int s, int p, int ptype)
{
   float rv = 0.0f;
   if ((p >= 0) && (p < 4))
   {
      _filter_t* filter = get_filter(f);
      if (filter)
      {
         cvtfn_t cvtfn = get_cvtfn(filter->type, ptype, READFN, p);
         rv = cvtfn(filter->slot[0]->param[p]);
         free(filter);
      }
   }
   return rv;
}

_filter_t*
new_filter_handle(_aaxMixerInfo* info, enum aaxFilterType type, _oalRingBuffer2dProps* p2d, _oalRingBuffer3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type < AAX_FILTER_MAX)
   {
      unsigned int size = sizeof(_filter_t);

      switch (type)
      {
      case AAX_TIMED_GAIN_FILTER:		/* three slots */
         size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_oalRingBufferFilterInfo);
         break;
      case AAX_EQUALIZER:                       /* two slots */
      case AAX_GRAPHIC_EQUALIZER:
         size += EQUALIZER_MAX*sizeof(_oalRingBufferFilterInfo);
         break;
      case AAX_FREQUENCY_FILTER:
         size += sizeof(_oalRingBufferFilterInfo);
         /* break not needed */
      default:					/* one slot */
         size += sizeof(_oalRingBufferFilterInfo);
         break;
      }

      rv = calloc(1, size);
      if (rv)
      {
         char *ptr = (char*)rv + sizeof(_filter_t);

         rv->id = FILTER_ID;
         rv->info = info;
         rv->slot[0] = (_oalRingBufferFilterInfo*)ptr;
         rv->pos = _flt_cvt_tbl[type].pos;
         rv->state = p2d->filter[rv->pos].state;
         rv->type = type;

         size = sizeof(_oalRingBufferFilterInfo);
         switch (type)
         {
         case AAX_GRAPHIC_EQUALIZER:
            rv->slot[1] = (_oalRingBufferFilterInfo*)(ptr + size);
            rv->slot[0]->param[0] = 1.0f; rv->slot[1]->param[0] = 1.0f;
            rv->slot[0]->param[1] = 1.0f; rv->slot[1]->param[1] = 1.0f;
            rv->slot[0]->param[2] = 1.0f; rv->slot[1]->param[2] = 1.0f;
            rv->slot[0]->param[3] = 1.0f; rv->slot[1]->param[3] = 1.0f;
            rv->slot[0]->data = NULL;     rv->slot[1]->data = NULL;
            break;
         case AAX_EQUALIZER:
            rv->slot[1] = (_oalRingBufferFilterInfo*)(ptr + size);
            memcpy(rv->slot[1], &p2d->filter[rv->pos], size);
            rv->slot[1]->data = NULL;
            /* break not needed */
         case AAX_VOLUME_FILTER:
         case AAX_DYNAMIC_GAIN_FILTER:
            memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
            rv->slot[0]->data = NULL;
            break;
         case AAX_FREQUENCY_FILTER:
         {
            _oalRingBufferFreqFilterInfo *freq; 

            freq = (_oalRingBufferFreqFilterInfo *)p2d->filter[rv->pos].data;
            rv->slot[1] = (_oalRingBufferFilterInfo*)(ptr + size);
            /* reconstruct rv->slot[1] */
            if (freq && freq->lfo)
            {
               rv->slot[1]->param[AAX_RESONANCE] = freq->lfo->f;
               rv->slot[1]->param[AAX_CUTOFF_FREQUENCY] = freq->lfo->max;
            }
            else
            {
               int type = AAX_FREQUENCY_FILTER;
               memcpy(rv->slot[1], &_flt_minmax_tbl[1][type], size);
            }
            memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
            rv->slot[0]->data = NULL;
            break;
         }
         case AAX_TIMED_GAIN_FILTER:
         {
            _oalRingBufferEnvelopeInfo *env;
            unsigned int no_steps;
            float dt, value;
            int i, stages;

            env = (_oalRingBufferEnvelopeInfo*)p2d->filter[rv->pos].data;
            memcpy(rv->slot[0], &p2d->filter[rv->pos], size);
            rv->slot[0]->data = NULL;

            i = 0;
            if (env->max_pos[1] > env->max_pos[0]) i = 1;
            dt = p2d->filter[rv->pos].param[2*i+1] / env->max_pos[i];

            no_steps = env->max_pos[1];
            value = p2d->filter[rv->pos].param[AAX_LEVEL1];
            value += env->step[1] * no_steps;

            stages = _MIN(1+env->max_stages/2, _MAX_ENVELOPE_STAGES/2);
            for (i=1; i<stages; i++)
            {
               _oalRingBufferFilterInfo* slot;

               slot = (_oalRingBufferFilterInfo*)(ptr + i*size);
               rv->slot[i] = slot;

               no_steps = env->max_pos[2*i];
               slot->param[0] = value;
               slot->param[1] = no_steps * dt;

               value += env->step[2*i] * no_steps;
               no_steps = env->max_pos[2*i+1];
               slot->param[2] = value;
               slot->param[3] = no_steps * dt;

               value += env->step[2*i+1] * no_steps;
            }
            break;
         }
         case AAX_ANGULAR_FILTER:
         case AAX_DISTANCE_FILTER:
            memcpy(rv->slot[0], &p3d->filter[rv->pos], size);
            break;
         default:
            break;
         }
      }
   }
   return rv;
}

_filter_t*
get_filter(aaxFilter f)
{
   _filter_t* rv = (_filter_t*)f;
   if (rv && rv->id == FILTER_ID) {
      return rv;
   }
   return NULL;
}

static cvtfn_t
get_cvtfn(enum aaxFilterType type, int ptype, int mode, char param)
{
   cvtfn_t rv = _lin;
   switch (type)
   {
   case AAX_TIMED_GAIN_FILTER:
   case AAX_VOLUME_FILTER:
      if (ptype == AAX_LOGARITHMIC)
      {
         if (mode == WRITEFN) {
            rv = _lin2db;
         } else {
            rv = _db2lin;
         }
      }
      break;
   case AAX_FREQUENCY_FILTER:
      if (param > 0)
      {
         if (ptype == AAX_LOGARITHMIC)
         {
            if (mode == WRITEFN) {
               rv = _lin2db;
            } else {
               rv = _db2lin;
            }
         }
      }
      break;
   case AAX_ANGULAR_FILTER:
      if (param < 2)
      {
         if (ptype == AAX_DEGREES)
         {
            if (mode == WRITEFN) {
               rv = _cos_deg2rad_2;
            } else {
               rv = _2acos_rad2deg;
            }
         }
         else 
         {
            if (mode == WRITEFN) {
               rv = _cos_2;
            } else {
               rv = _2acos;
            }
         }
      }
      break;
   default:
      break;
   }
   return rv;
}
