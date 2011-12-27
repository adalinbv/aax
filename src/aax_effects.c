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

#include <math.h>
#include <assert.h>

#include <aax.h>

#include "arch.h"
#include "api.h"

#define WRITEFN		0
#define EPS		1e-5
#define READFN		!WRITEFN

typedef struct {
  enum aaxEffectType type;
  int pos;
} _eff_cvt_tbl_t;
typedef struct {
   vec4 min;
   vec4 max;
} _eff_minmax_tbl_t;
typedef float (*cvtfn_t)(float);

static const _eff_cvt_tbl_t _eff_cvt_tbl[AAX_EFFECT_MAX];
static const _eff_minmax_tbl_t _eff_minmax_tbl[AAX_EFFECT_MAX];
static cvtfn_t get_cvtfn(enum aaxEffectType, int, int, char);

aaxEffect
aaxEffectCreate(aaxConfig config, enum aaxEffectType type)
{
   _handle_t *handle = get_handle(config);
   aaxEffect rv = NULL;
   if (handle)
   {
      unsigned int size = sizeof(_filter_t);
     _filter_t* eff;

      switch (type)
      {
      case AAX_TIMED_PITCH_EFFECT:
         size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_oalRingBufferFilterInfo);
         break;
      default:
         size += sizeof(_oalRingBufferFilterInfo);
         break;
      }

      eff = calloc(1, size);
      if (eff)
      {
         char *ptr;
         int i;

         eff->id = EFFECT_ID;
         eff->state = AAX_FALSE;
         if VALID_HANDLE(handle) eff->info = handle->info;

         ptr = (char*)eff + sizeof(_filter_t);
         eff->slot[0] = (_oalRingBufferFilterInfo*)ptr;
         eff->pos = _eff_cvt_tbl[type].pos;
         eff->type = type;

         size = sizeof(_oalRingBufferFilterInfo);
         switch (type)
         {
         case AAX_PITCH_EFFECT:
         case AAX_VIBRATO_EFFECT:
         case AAX_PHASING_EFFECT:
         case AAX_CHORUS_EFFECT:
         case AAX_FLANGING_EFFECT:
         case AAX_DISTORTION_EFFECT:
            memcpy(eff->slot[0], &_aaxDefault2dProps.effect[eff->pos], size);
            break;
         case AAX_TIMED_PITCH_EFFECT:
            for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
            {
               eff->slot[i] = (_oalRingBufferFilterInfo*)(ptr + i*size);
               memcpy(eff->slot[i], &_aaxDefault2dProps.filter[eff->pos], size);
            }
            break;
         case AAX_VELOCITY_EFFECT:
            memcpy(eff->slot[0], &_aaxDefault3dProps.effect[eff->pos], size);
            break;
         default:
            _aaxErrorSet(AAX_INVALID_ENUM);
            free(eff);
            eff = NULL;
            break;
         }
      }
      else {
         _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
      }
      rv = (aaxEffect)eff;
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

int
aaxEffectDestroy(aaxEffect f)
{
   int rv = AAX_FALSE;
   _filter_t* effect = get_effect(f);
   if (effect)
   {
      /*
       * If an effect is applied to an emitter, the scenery or an audioframe
       * effect->slot[0]->data will be copied and set to NULL. If not applied
       * we need to free the located memory ourselves.
       */
      switch (effect->type)
      {
      case AAX_FLANGING_EFFECT:
      {
         _oalRingBufferDelayEffectData* data = effect->slot[0]->data;
         if (data) free(data->history_ptr);
         /* break is not needed */
      }
      case AAX_TIMED_PITCH_EFFECT:
      case AAX_VIBRATO_EFFECT:
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
         free(effect->slot[0]->data);
         effect->slot[0]->data = NULL;
         break;
      default:
         break;
      }
      free(effect);
      rv = AAX_TRUE;
   }
   else {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   }
   return rv;
}

aaxEffect
aaxEffectSetSlot(aaxEffect e, unsigned slot, int ptype, float p1, float p2, float p3, float p4)
{
   aaxVec4f v;
   v[0] = p1, v[1] = p2; v[2] = p3, v[3] = p4;
   return aaxEffectSetSlotParams(e, slot, ptype, v);
}

aaxEffect
aaxEffectSetSlotParams(aaxEffect f, unsigned slot, int ptype, aaxVec4f p)
{
   aaxEffect rv = AAX_FALSE;
   _filter_t* effect = get_effect(f);
   if (effect && p)
   {
      if ((slot < _MAX_SLOTS) && effect->slot[slot])
      {
         int i, type = effect->type;
         for(i=0; i<4; i++)
         {
            if (!is_nan(p[i]))
            {
               float min = _eff_minmax_tbl[type].min[i];
               float max = _eff_minmax_tbl[type].max[i];
               cvtfn_t cvtfn = get_cvtfn(effect->type, ptype, WRITEFN, i);
               effect->slot[slot]->param[i] = _MINMAX(cvtfn(p[i]), min, max);
            }
         }
         if TEST_FOR_TRUE(effect->state) {
            rv = aaxEffectSetState(effect, AAX_TRUE);
         } else {
            rv = effect;
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

int
aaxEffectSetParam(const aaxEffect e, int param, int ptype, float value)
{
   _filter_t* effect = get_effect(e);
   int rv = AAX_FALSE;
   if (effect && !is_nan(value))
   {
      unsigned slot = param >> 4;
      if ((slot < _MAX_SLOTS) && effect->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            cvtfn_t cvtfn = get_cvtfn(effect->type, ptype, WRITEFN, param);
            effect->slot[slot]->param[param] = cvtfn(value);
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


aaxEffect
aaxEffectSetState(aaxEffect e, int state)
{
   _filter_t* effect = get_effect(e);
   aaxEffect rv = AAX_FALSE;
   if (effect && effect->info)
   {
      effect->state = state;
      switch(effect->type)
      {
      case AAX_PITCH_EFFECT:
      case AAX_VELOCITY_EFFECT:
         break;
      case AAX_DISTORTION_EFFECT:
         if TEST_FOR_TRUE(state) {
            effect->slot[0]->param[0] = powf(effect->slot[0]->param[0], 2.5f);
            effect->slot[0]->data = &effect->slot[0]->param[0];
         } else {
            effect->slot[0]->param[0] = powf(effect->slot[0]->param[0], 0.4f);
            effect->slot[0]->data = NULL;
         }
         break;
      case AAX_VIBRATO_EFFECT:
#if !ENABLE_LITE
         if EBF_VALID(effect)
         {
            switch (state)
            {
            case AAX_TRIANGLE_WAVE:
            case AAX_SINE_WAVE:
            case AAX_SQUARE_WAVE:
            case AAX_SAWTOOTH_WAVE:
            {
                _oalRingBufferLFOInfo* lfo = effect->slot[0]->data;
               if (lfo == NULL)
               {
                  lfo = malloc(sizeof(_oalRingBufferLFOInfo));
                  effect->slot[0]->data = lfo;
               }

               if (lfo)
               {
                  float depth = effect->slot[0]->param[AAX_LFO_DEPTH];
                  int t;
                  for(t=0; t<_AAX_MAX_SPEAKERS; t++)
                  {
                     lfo->step[t] = 2.0f*depth;
                     lfo->step[t] *= effect->slot[0]->param[AAX_LFO_FREQUENCY];
                     lfo->step[t] /= effect->info->refresh_rate;
                     lfo->value[t] = 1.0f;
                     if (state == AAX_SAWTOOTH_WAVE) {
                        lfo->step[t] *= 0.5f;
                     }
                  }
                  lfo->min = 1.0f - depth/2.0f;
                  lfo->max = 1.0f + depth/2.0f;
                  if (depth > 0.01f)
                  {
                     switch (state)
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
               effect->slot[0]->data = NULL;
               break;
            default:
               _aaxErrorSet(AAX_INVALID_PARAMETER);
               break;
            }
         }
#endif
         break;
      case AAX_TIMED_PITCH_EFFECT:
#if !ENABLE_LITE
         if EBF_VALID(effect)
         {
            if TEST_FOR_TRUE(state)
            {
               _oalRingBufferEnvelopeInfo* env = effect->slot[0]->data;
               if (env == NULL)
               {
                  env =  calloc(1, sizeof(_oalRingBufferEnvelopeInfo));
                  effect->slot[0]->data = env;
               }

               if (env)
               {
                  float nextval = effect->slot[0]->param[AAX_LEVEL0];
                  float refresh = effect->info->refresh_rate;
                  float timestep = 1.0f / refresh;
                  int i;

                  env->value = nextval;

                  env->max_stages = _MAX_ENVELOPE_STAGES;
                  for (i=0; i<_MAX_ENVELOPE_STAGES/2; i++)
                  {
                     float dt, value = nextval;
                     uint32_t max_pos;

                     max_pos = (uint32_t)-1;
                     dt = effect->slot[i]->param[AAX_TIME0];
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

                     nextval = effect->slot[i]->param[AAX_LEVEL1];
                     if (nextval == 0.0f) nextval = -1e-2;
                     env->step[2*i] = (nextval - value)/max_pos;
                     env->max_pos[2*i] = max_pos;

                     /* prevent a core dump for accessing an illegal slot */
                     if (i == (_MAX_ENVELOPE_STAGES/2)-1) break;

                     max_pos = (uint32_t)-1;
                     dt = effect->slot[i]->param[AAX_TIME1];
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
                     nextval = effect->slot[i+1]->param[AAX_LEVEL0];
                     if (nextval == 0.0f) nextval = -1e-2;
                     env->step[2*i+1] = (nextval - value)/max_pos;
                     env->max_pos[2*i+1] = max_pos;
                  }
               }
               else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
            }
            else {
               effect->slot[0]->data = NULL;
            }
         }
#endif
         break;
      case AAX_PHASING_EFFECT:
      case AAX_CHORUS_EFFECT:
      case AAX_FLANGING_EFFECT:
#if !ENABLE_LITE
         if EBF_VALID(effect)
         {
            switch (state)
            {
            case AAX_TRIANGLE_WAVE:
            case AAX_SINE_WAVE:
            case AAX_SQUARE_WAVE:
            case AAX_SAWTOOTH_WAVE:
            {
               _oalRingBufferDelayEffectData* data = effect->slot[0]->data;
               if (data == NULL)
               {
                  int t;
                  data  = malloc(sizeof(_oalRingBufferDelayEffectData));
                  effect->slot[0]->data = data;
                  data->history_ptr = 0;
                  for (t=0; t<_AAX_MAX_SPEAKERS; t++)
                  {
                     data->lfo.value[t] = 0.0f;
                     data->lfo.step[t] = 0.0f;
                  }
               }

               if (data)
               {
                  unsigned int tracks = effect->info->no_tracks;
                  float frequency = effect->info->frequency;
                  float depth = effect->slot[0]->param[AAX_LFO_DEPTH];
                  float offset = effect->slot[0]->param[AAX_LFO_OFFSET];
                  float sign, range, step;
                  int t;

                  for (t=0; t<_AAX_MAX_SPEAKERS; t++)
                  {
                     step = data->lfo.step[t];
                     sign = step ? (step/fabs(step)) : 1.0f;
                     data->lfo.step[t] = sign * effect->info->refresh_rate;
                     data->lfo.step[t]
                               *= effect->slot[0]->param[AAX_LFO_FREQUENCY]/2.0;
                  }
                  data->delay.gain = effect->slot[0]->param[AAX_DELAY_GAIN];

                  if ((offset + depth) > 1.0f) {
                     depth = 1.0f - offset;
                  }

                  switch (effect->type)
                  {
                  case AAX_PHASING_EFFECT:
                     range = (10e-3f - 50e-6f);		// 50us .. 10ms
                     depth *= range * frequency;	// cvt to samples
                     data->lfo.min = (range * offset + 50e-6f)*frequency;
                     break;
                  case AAX_FLANGING_EFFECT:
                     _oalRingBufferCreateHistoryBuffer(data, frequency, tracks);
                     /* break not needed */
                  case AAX_CHORUS_EFFECT:
                     range = (60e-3f - 10e-3f);		// 10ms .. 60ms
                     depth *= range * frequency; 	// cvt to samples
                     data->lfo.min = (range * offset + 10e-3f)*frequency;
                     break;
                  default:
                     break;
                  }

                  data->lfo.max = data->lfo.min + depth;

                  if (depth > 0.05f)
                  {
                     int t;
                     for (t=0; t<_AAX_MAX_SPEAKERS; t++)
                     {
                        if (data->lfo.value[t] == 0) {
                           data->lfo.value[t] = data->lfo.min;
                        }
                        data->delay.sample_offs[t] = data->lfo.value[t];
                        if (state == AAX_SAWTOOTH_WAVE) {
                           data->lfo.step[t] *= 0.5f;
                        }
                     }

                     switch (state)
                     {
                     case AAX_TRIANGLE_WAVE:
                        data->lfo.get = _oalRingBufferLFOGetTriangle;
                        break;
                     case AAX_SINE_WAVE:
                        data->lfo.get = _oalRingBufferLFOGetSine;
                        break;
                     case AAX_SQUARE_WAVE:
                        data->lfo.get = _oalRingBufferLFOGetSquare;
                        break;
                     case AAX_SAWTOOTH_WAVE:
                        data->lfo.get = _oalRingBufferLFOGetSawtooth;
                        break;
                     default:
                        _aaxErrorSet(AAX_INVALID_PARAMETER);
                        break;
                     }
                  }
                  else
                  {
                     int t;
                     for (t=0; t<_AAX_MAX_SPEAKERS; t++)
                     {
                        data->lfo.value[t] = data->lfo.min;
                        data->delay.sample_offs[t] = data->lfo.value[t];
                     }
                     data->lfo.get = _oalRingBufferLFOGetFixedValue;
               }
               }
               else _aaxErrorSet(AAX_INSUFFICIENT_RESOURCES);
               break;
            }
            case AAX_FALSE:
               effect->slot[0]->data = NULL;
               break;
            default:
               _aaxErrorSet(AAX_INVALID_PARAMETER);
               break;
            }
         }
#endif
         break;
      default:
         _aaxErrorSet(AAX_INVALID_ENUM);
      }
      rv = effect;
   }
   else if (!effect) {
      _aaxErrorSet(AAX_INVALID_HANDLE);
   } else { /* !effect->info */
      _aaxErrorSet(AAX_INVALID_STATE);
   }
   return rv;
}

float
aaxEffectGetParam(const aaxEffect e, int param, int ptype)
{
   _filter_t* effect = get_effect(e);
   float rv = 0.0f;
   if (effect)
   {
      unsigned slot = param >> 4;
      if ((slot < _MAX_SLOTS) && effect->slot[slot])
      {
         param &= 0xF;
         if ((param >= 0) && (param < 4))
         {
            cvtfn_t cvtfn = get_cvtfn(effect->type, ptype, READFN, param);
            rv = cvtfn(effect->slot[slot]->param[param]);
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

aaxEffect
aaxEffectGetSlot(const aaxEffect e, unsigned slot, int ptype, float* p1, float* p2, float* p3, float* p4)
{
   aaxVec4f v;
   aaxEffect rv = aaxEffectGetSlotParams(e, slot, ptype, v);
   if(p1) *p1 = v[0];
   if(p2) *p2 = v[1];
   if(p3) *p3 = v[2];
   if(p4) *p4 = v[3];
   return rv;
}

aaxEffect
aaxEffectGetSlotParams(const aaxEffect e, unsigned slot, int ptype, aaxVec4f p)
{
   _filter_t* effect = get_effect(e);
   aaxEffect rv = AAX_FALSE;
   if (effect && p)
   {
      if ((slot < _MAX_SLOTS) && effect->slot[slot])
      {
         int i;
         for (i=0; i<4; i++)
         {
            cvtfn_t cvtfn = get_cvtfn(effect->type, ptype, READFN, i);
            p[i] = cvtfn(effect->slot[slot]->param[i]);
         }
         rv = effect;
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

static const _eff_cvt_tbl_t _eff_cvt_tbl[AAX_EFFECT_MAX] =
{
  { AAX_EFFECT_NONE,       MAX_STEREO_EFFECT },
  { AAX_PITCH_EFFECT,      PITCH_EFFECT },
  { AAX_VIBRATO_EFFECT,    VIBRATO_EFFECT },
  { AAX_TIMED_PITCH_EFFECT,TIMED_PITCH_EFFECT },
  { AAX_DISTORTION_EFFECT, DISTORTION_EFFECT},
  { AAX_PHASING_EFFECT,    DELAY_EFFECT },
  { AAX_CHORUS_EFFECT,     DELAY_EFFECT },
  { AAX_FLANGING_EFFECT,   DELAY_EFFECT },
  { AAX_VELOCITY_EFFECT,   VELOCITY_EFFECT }
};

static const _eff_minmax_tbl_t _eff_minmax_tbl[AAX_EFFECT_MAX] =
{    /* min[4] */		   /* max[4] */
  /* AAX_EFFECT_NONE      */
  { { 0.0f, 0.0f,  0.0f, 0.0f }, {     0.0f,     0.0f, 0.0f,     0.0f } },
  /* AAX_PITCH_EFFECT     */
  { { 0.0f, 0.0f,  0.0f, 0.0f }, {    1.99f,    1.99f, 0.0f,     0.0f } },
  /* AAX_VIBRATO_EFFECT   */
  { { 1.0f, 0.01f, 0.0f, 0.0f }, {     1.0f,    10.0f, 1.0f,     0.0f } },
  /* AAX_TIMED_PITCH_EFFECT */
  { {  0.0f, 0.0f, 0.0f, 0.0f }, {     4.0f, MAXFLOAT, 4.0f, MAXFLOAT } },
  /* AAX_DISTORTION_EFFECT */
  { {  0.0f, 0.0f, 0.0f, 0.0f }, {     2.0f,     1.0f, 1.0f,     0.0f } },
  /* AAX_PHASING_EFFECT   */
  { { 0.0f, 0.01f, 0.0f, 0.0f }, {    0.99f,    10.0f, 1.0f,     0.0f } },
  /* AAX_CHORUS_EFFECT    */
  { { 0.0f, 0.01f, 0.0f, 0.0f }, {    0.99f,    10.0f, 1.0f,     0.0f } },
  /* AAX_FLANGING_EFFECT  */
  { { 0.0f, 0.01f, 0.0f, 0.0f }, {    0.99f,    10.0f, 1.0f,     0.0f } },
  /* AAX_VELOCITY_EFFECT  */
  { { 0.0f, 0.0f,  0.0f, 0.0f }, { MAXFLOAT,    10.0f, 0.0f,     0.0f } }
};

/* internal use only, used by aaxdefs.h */
aaxEffect
aaxEffectApply(aaxEffectFn fn, void *handle, aaxEffect f)
{
   if (f)
   {
      if (!fn(handle, f))
      {
         aaxEffectDestroy(f);
         f = NULL;
      }
   }
   return f;
}

float
aaxEffectApplyParam(const aaxEffect f, int s, int p, int ptype)
{
   float rv = 0.0f;
   if ((p >= 0) && (p < 4))
   {
      _filter_t* effect = get_effect(f);
      if (effect)
      {
         cvtfn_t cvtfn = get_cvtfn(effect->type, ptype, READFN, p);
         rv = cvtfn(effect->slot[0]->param[p]);
         free(effect);
      }
   }
   return rv;
}

static float _lin(float v) { return v; }
static float _lin2db(float v) { return 20.0f*log(v); }
static float _db2lin(float v) { return _MINMAX(pow(10.0f,v/20.0f),0.0f,10.0f); }
// static float _rad2deg(float v) { return v*GMATH_RAD_TO_DEG; }
// static float _deg2rad(float v) { return fmod(v, 360.0f)*GMATH_DEG_TO_RAD; }

_filter_t*
new_effect_handle(_aaxMixerInfo* info, enum aaxEffectType type, _oalRingBuffer2dProps* p2d, _oalRingBuffer3dProps* p3d)
{
   _filter_t* rv = NULL;
   if (type < AAX_EFFECT_MAX)
   {
      unsigned int size = sizeof(_filter_t);

      switch (type)
      {
      case AAX_TIMED_PITCH_EFFECT:
         size += (_MAX_ENVELOPE_STAGES/2)*sizeof(_oalRingBufferFilterInfo);
         break;
      default:
         size += sizeof(_oalRingBufferFilterInfo);
         break;
      }

      rv = calloc(1, size);
      if (rv)
      {
         char *ptr = (char*)rv + sizeof(_filter_t);

         rv->id = EFFECT_ID;
         rv->info = info;
         rv->slot[0] = (_oalRingBufferFilterInfo*)ptr;
         rv->pos = _eff_cvt_tbl[type].pos;
         rv->type = type;

         size = sizeof(_oalRingBufferFilterInfo);
         switch (type)
         {
         case AAX_PITCH_EFFECT:
         case AAX_VIBRATO_EFFECT:
         case AAX_PHASING_EFFECT:
         case AAX_CHORUS_EFFECT:
         case AAX_FLANGING_EFFECT:
         case AAX_DISTORTION_EFFECT:
            memcpy(rv->slot[0], &p2d->effect[rv->pos], size);
            rv->slot[0]->data = NULL;
            break;
         case AAX_TIMED_PITCH_EFFECT:
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
         case AAX_VELOCITY_EFFECT:
            memcpy(rv->slot[0], &p3d->effect[rv->pos], size);
            break;
         default:
            break;
         }

#if 0
         /* Force the effect to create a new data section when changing one of
          * the parameters, that way the effect doesn't alter the settings of
          * the running effect unprotected by a lock.
          */
//       rv->slot[0]->data = NULL;
#endif
      }
   }
   return rv;
}

_filter_t*
get_effect(aaxEffect f)
{
   _filter_t* rv = (_filter_t*)f;
   if (rv && rv->id == EFFECT_ID) {
      return rv;
   }
   return NULL;
}

static cvtfn_t
get_cvtfn(enum aaxEffectType type, int ptype, int mode, char param)
{
   cvtfn_t rv = _lin;
   switch (type)
   {
   case AAX_PHASING_EFFECT:
   case AAX_CHORUS_EFFECT:
   case AAX_FLANGING_EFFECT:
      if ((param == 0) && (ptype == AAX_LOGARITHMIC))
      {
         if (mode == WRITEFN) {
            rv = _lin2db;
         } else {
            rv = _db2lin;
         }
      }
      break;
   default:
      break;
   }
   return rv;
}
