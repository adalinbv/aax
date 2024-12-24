/*
 * SPDX-FileCopyrightText: Copyright © 2005-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2009-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RMALLOC_H
# include <rmalloc.h>
#else
# include <string.h>
#endif
#include <assert.h>

#include <dsp/filters.h>
#include <dsp/effects.h>

#include <api.h>
#include <arch.h>
#include <ringbuffer.h>

#include "rbuf_int.h"
#include "renderer.h"
#include "audio.h"


static void _aaxSubFramePostProcess(const _aaxRendererData*);
static void _aaxSensorPostProcess(const _aaxRendererData*);

static int
_aaxSoftwareMixerApplyTrackEffects(_aaxRingBuffer *rb, _aaxRendererData *renderer, UNUSED(_intBufferData *dptr_src), unsigned int track)
{
   const _aaxRendererData *data = (_aaxRendererData*)renderer;
   const _aaxDriverBackend *be = data->be;
   _aax2dProps *p2d = data->fp2d;
   _aaxRingBufferDelayEffectData* delay_effect, *delay_line;
   _aaxRingBufferFreqFilterData* freq_filter;
   _aaxRingBufferOcclusionData *occlusion;
   _aaxRingBufferReverbData *reverb;
   bool mono = data->mono;
   bool mixer_dsp;
   float maxgain, gain;
   int bps;

   assert(rb != 0);

   bps = rb->get_parami(rb, RB_BYTES_SAMPLE);
   assert(bps == sizeof(MIX_T));

   delay_effect = _EFFECT_GET_DATA(p2d, DELAY_EFFECT);
   delay_line = _EFFECT_GET_DATA(p2d, DELAY_LINE_EFFECT);
   freq_filter = _FILTER_GET_DATA(p2d, FREQUENCY_FILTER);
   occlusion = _FILTER_GET_DATA(p2d, VOLUME_FILTER);
   reverb = _EFFECT_GET_DATA(p2d, REVERB_EFFECT);
   mixer_dsp = _EFFECT_GET_STATE(p2d, DISTORTION_EFFECT);
   mixer_dsp |= _EFFECT_GET_STATE(p2d, RINGMODULATE_EFFECT);
   mixer_dsp |= _EFFECT_GET_STATE(p2d, FREQUENCY_SHIFT_EFFECT);
   if (mixer_dsp || delay_effect || delay_line || freq_filter ||
       occlusion || reverb)
   {
      _aaxRingBufferData *rbi = rb->handle;
      _aaxRingBufferSample *rbd = rbi->sample;
      MIX_T **scratch = data->scratch;
      MIX_T *scratch0 = scratch[2*track];
      MIX_T *scratch1 = scratch[2*track+1];
      size_t no_samples, ddesamps = 0;
      MIX_T **tracks, *dptr;

      assert (2*track+1 < MAX_SCRATCH_BUFFERS);

      if (reverb) {
         ddesamps = rb->get_parami(rb, RB_DDE_SAMPLES);
      } else if (delay_line) { // longer delay line
         ddesamps = delay_line->delay.sample_offs[track];
      } else if (delay_effect) { // phasing, chorus, flanging
         ddesamps = delay_effect->delay.sample_offs[track];
      }

      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
      tracks = (MIX_T**)rbd->track;

      dptr = (MIX_T*)tracks[track];
      memcpy(scratch0, dptr, no_samples*bps);
      rbi->effects(rbi->sample, dptr, scratch0, scratch1, 0, no_samples,
                   no_samples, ddesamps, track, p2d, mono);
   }

   /*
    * If the requested gain is larger than the maximum capabilities of
    * hardware volume support, adjust the difference here (before the
    * compressor/limiter)
    */
   maxgain = be->param(data->be_handle, DRIVER_MAX_VOLUME);
   gain = _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);
   if (gain > maxgain) gain = maxgain;
   if (fabsf(gain-1.0f) > LEVEL_96DB) {
      rb->data_multiply(rb, 0, 0, gain, 1.0f);
   }

   return true;
}

void
_aaxSoftwareMixerApplyEffects(const void *renderer)
{
   _aaxRendererData *data = (_aaxRendererData*)renderer;
   _aaxRenderer *render = data->be->render(data->be_handle);
   data->callback = _aaxSoftwareMixerApplyTrackEffects;
   render->process(render, data);
}

void
_aaxSoftwareMixerPostProcess(const void *renderer)
{
   const _aaxRendererData *data = (_aaxRendererData*)renderer;
   if (data->subframe) {
      _aaxSubFramePostProcess(data);
   } else if (data->sensor) {
      _aaxSensorPostProcess(data);
   }
}

/* -------------------------------------------------------------------------- */

static void
_aaxFrameProcessEqualizer(_aaxRingBuffer *rb, _sensor_t *sensor, _aaxAudioFrame *mixer, MIX_T **scratch)
{
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   bool parametric, graphic;

   rbi = rb->handle;
   rbd = rbi->sample;

   parametric = graphic = (_FILTER_GET_DATA(mixer, EQUALIZER_HF) != NULL);
   parametric &= (_FILTER_GET_DATA(mixer, EQUALIZER_LF) != NULL);
   graphic    &= (_FILTER_GET_DATA(mixer, EQUALIZER_LF) == NULL);

   if (parametric || graphic)
   {
      MIX_T **tracks = (MIX_T**)rbd->track;
      int t, no_tracks;
      size_t no_samples;

      no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
      no_tracks = rb->get_parami(rb, RB_NO_TRACKS);

      if (parametric)
      {
         _aaxRingBufferFreqFilterData *filter[_MAX_PARAM_EQ];

         filter[0] = _FILTER_GET_DATA(mixer, EQUALIZER_LF);
         filter[1] = _FILTER_GET_DATA(mixer, EQUALIZER_LMF);
         filter[2] = _FILTER_GET_DATA(mixer, EQUALIZER_HMF);
         filter[3] = _FILTER_GET_DATA(mixer, EQUALIZER_HF);
         for (t=0; t<no_tracks; t++) {
            _equalizer_run(rbi->sample, tracks[t], scratch[SCRATCH_BUFFER0],
                           0, no_samples, t, filter);
         }
      }
      else
      {
         _aaxRingBufferEqualizerData *eq;
         size_t track_len_bytes;

         eq = _FILTER_GET_DATA(mixer, EQUALIZER_HF);
         track_len_bytes = rb->get_parami(rb, RB_TRACKSIZE);
         for (t=0; t<no_tracks; t++)
         {
            _aax_memcpy(scratch[SCRATCH_BUFFER0], tracks[t], track_len_bytes);
            _grapheq_run(rbi->sample, tracks[t], scratch[SCRATCH_BUFFER0],
                         scratch[SCRATCH_BUFFER1], 0, no_samples, t, eq);

            if (sensor)
            {
               memcpy(sensor->rms[t], eq->rms, sizeof(float[_AAX_MAX_EQBANDS]));
               memcpy(sensor->peak[t],eq->peak,sizeof(float[_AAX_MAX_EQBANDS]));
            }
         }
      }
   }
}

// Apply the final filters like the equalizer
static void
_aaxSubFramePostProcess(const _aaxRendererData *data)
{
   _aaxRingBuffer *rb = data->drb;
   const _frame_t *subframe = data->subframe;

   assert(rb != 0);
   assert(rb->handle != 0);

   _aaxMutexLock(subframe->mutex);

   _aaxFrameProcessEqualizer(rb, NULL, subframe->submix, data->scratch);

   _aaxMutexUnLock(subframe->mutex);
}

// Apply the final filters like convolution, the equalizer and limitter
static void
_aaxSensorPostProcess(const _aaxRendererData *data)
{
   _aaxRingBufferConvolutionData *convolution;
   const _aaxMixerInfo *info = data->info;
   const unsigned char *router = info->router;
   _sensor_t *sensor = data->sensor;
   _aaxRingBuffer *rb = data->drb;
   unsigned char lfe_track, t, no_tracks;
   size_t no_samples, track_len_bytes;
   MIX_T **tracks, **scratch;
   _aaxRingBufferSample *rbd;
   _aaxRingBufferData *rbi;
   _aaxLFOData *compressor;
   bool crossover;

   assert(rb != 0);
   assert(rb->handle != 0);
   assert(sensor);

   rbi = rb->handle;

   assert(rbi->sample != 0);

   rbi = rb->handle;
   rbd = rbi->sample;

   /* set up this way because we always need to apply compression */
   track_len_bytes = rb->get_parami(rb, RB_TRACKSIZE);
   no_samples = rb->get_parami(rb, RB_NO_SAMPLES);
   no_tracks = rb->get_parami(rb, RB_NO_TRACKS);

   crossover = 0;
   convolution = NULL;
   lfe_track = router[AAX_TRACK_LFE];
   if (_EFFECT_GET_STATE(sensor->mixer->props2d, CONVOLUTION_EFFECT)) {
      convolution = _EFFECT_GET_DATA(sensor->mixer->props2d,
                                          CONVOLUTION_EFFECT);
   }
   compressor = _FILTER_GET_DATA(sensor->mixer->props2d, DYNAMIC_GAIN_FILTER);
   crossover = (_FILTER_GET_DATA(sensor->mixer, SURROUND_CROSSOVER_LP) != NULL);
   crossover &= (no_tracks >= lfe_track);

   tracks = (MIX_T**)rbd->track;
   if (crossover) {
      memset(tracks[lfe_track], 0, track_len_bytes);
   }

   scratch = data->scratch;

   if (convolution)
   {
      for (t=0; t<no_tracks; t++)
      {
         MIX_T *dptr = tracks[t];
         float *histx = rbd->freqfilter_history_x;
         float *histy = rbd->freqfilter_history_y;
         float xm1 = histx[t];
         float ym1 = histy[t];
         size_t i = no_samples;

         // remove any DC offset
         do
         {
            float x = *dptr;
            float y = x - xm1 + 0.995 * ym1;
            xm1 = x;
            ym1 = y;
            *dptr++ = y;
         }
         while (--i);
         histx[t] = xm1;
         histy[t] = ym1;
      }
   }

   _aaxMutexLock(sensor->mutex);
   if (convolution) {
      convolution->run(data->be, data->be_handle, rb, convolution);
   }

   if (compressor && compressor->envelope)
   {
      float g = 1e9f;
      for (t=0; t<no_tracks; t++)
      {
         float gain;

         gain = compressor->get(compressor, NULL, tracks[t], t, no_samples);
         if (compressor->inverse) gain = 1.0f/gain;
         if (gain < g) g = gain;
      }
      rb->data_multiply(rb, 0, 0, g, 1.0f);
   }

   if (sensor->filter[0])
   {
      for (t=0; t<no_tracks; t++) {
         rbd->freqfilter(tracks[t], tracks[t], t, no_samples,sensor->filter[0]);
         rbd->freqfilter(tracks[t], tracks[t], t, no_samples,sensor->filter[1]);
      }
   }

   _aaxFrameProcessEqualizer(rb, sensor, sensor->mixer, data->scratch);
   _aaxMutexUnLock(sensor->mutex);

   for (t=0; t<no_tracks; t++)
   {
      MIX_T *tmp = scratch[SCRATCH_BUFFER1];
      MIX_T *dptr = tracks[t];

      if (crossover && t != AAX_TRACK_FRONT_LEFT && t != AAX_TRACK_FRONT_RIGHT)
      {
         _aaxRingBufferFreqFilterData* filter;
         unsigned char stages;
         float *hist, k;

         filter = _FILTER_GET_DATA(sensor->mixer, SURROUND_CROSSOVER_LP);
         hist = filter->freqfilter->history[t];
         stages = filter->no_stages;
         k = filter->k;

         _batch_movingaverage_float(tmp, dptr, no_samples, hist++, k);
         _batch_movingaverage_float(tmp, tmp, no_samples, hist++, k);
         if (--stages)
         {
            _batch_movingaverage_float(tmp, tmp, no_samples, hist++, k);
            _batch_movingaverage_float(tmp, tmp, no_samples, hist++, k);
         }
         rbd->add(tracks[lfe_track], tmp, no_samples, 1.0f, 0.0f);
         rbd->add(dptr, tmp, no_samples, -1.0f, 0.0f);
      }
   }

   rb->limit(rb, RB_COMPRESS);
}

// Send the rendered audio to the backend driver.
int
_aaxSoftwareMixerPlay(void* rb, UNUSED(const void* devices), const void* ringbuffers, UNUSED(const void* frames), void* props2d, bool capturing, UNUSED(const void* sensor), const void* backend, const void* be_handle, const void* fbackend, const void* fbe_handle, bool batched)
{
   const _aaxDriverBackend* be = (const _aaxDriverBackend*)backend;
   const _aaxDriverBackend* fbe = (const _aaxDriverBackend*)fbackend;
   _aax2dProps *p2d = (_aax2dProps*)props2d;
   _aaxRingBuffer *dest_rb = (_aaxRingBuffer *)rb;
   float gain;
   int res;

   // NOTE: File backend must be first, it's the only backend that
   //       converts the buffer back to floats when done.
   gain = _FILTER_GET(p2d, VOLUME_FILTER, AAX_GAIN);
   if (fbe) {	/* tied file-out backend */
      fbe->play(fbe_handle, dest_rb, 1.0f, gain, batched);
   }

   /** play back all mixed audio */
   res = be->play(be_handle, dest_rb, 1.0f, gain, batched);

   /** create a new ringbuffer when capturing */
   if TEST_FOR_TRUE(capturing)
   {
      _intBuffers *mixer_ringbuffers = (_intBuffers*)ringbuffers;
      _aaxRingBuffer *new_rb;

      new_rb = dest_rb->duplicate(dest_rb, true, false);
      _intBufAddData(mixer_ringbuffers, _AAX_RINGBUFFER, new_rb);
   }
   return res;
}


/*
 * Main mixer update function:
 * mixes all assigned audio-frames, sensors and emitters
 */
int
_aaxSoftwareMixerThreadUpdate(void *config, void *drb)
{
   _aaxRingBuffer *rb = (_aaxRingBuffer*)drb;
   _handle_t *handle = (_handle_t *)config;
   const _aaxDriverBackend *be, *fbe = NULL;
   _intBufferData *dptr_sensor;
   bool batched;
   int res = 0;

   assert(handle);
   assert(handle->sensors);
   assert(handle->backend.ptr);
   assert(handle->info->no_tracks);

// FIXME: pipewire calls this function from a callback function
//        which messes things up; We might need to keep a timer for
//        every backend instead.
// _aaxTimerStart(handle->timer);

   batched = handle->batch_finished ? true : false;

   be = handle->backend.ptr;
   if (handle->file.driver && _IS_PLAYING((_handle_t*)handle->file.driver)) {
      fbe = handle->file.ptr;
   }

   dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
   if (dptr_sensor && (_IS_PLAYING(handle) || _IS_STANDBY(handle)))
   {
      void* be_handle = handle->backend.handle;
      void* fbe_handle = handle->file.handle;

      if (handle->info->mode == AAX_MODE_READ)
      {
         _aaxSensorsProcessSensor(handle, rb, NULL, 0, 0);
      }
      else if (_IS_PLAYING(handle))
      {
         // In the past all mixer registered audio frames rendered in their own
         // thread. Which included rendering all sub-frames, sensors and
         // emitters. This was replaced by worker threads which render all
         // emitters at the leaf-node audio-frame which spreads the load across
         // all availabe CPU's.
         // Copying all relevant data was de best way to hold the lock as short
         // as possible. Keeping this behavior (for now) doesn't do any harm
         // and makes it easier to render audio-threads in an thread again, if
         // desired.
         dptr_sensor = _intBufGetNoLock(handle->sensors, _AAX_SENSOR, 0);
         if (dptr_sensor)
         {
            _sensor_t *sensor = _intBufGetDataPtr(dptr_sensor);
            _aaxAudioFrame *smixer = sensor->mixer;

            if (smixer->emitters_3d || smixer->emitters_2d || smixer->frames)
            {
               _aaxDelayed3dProps sdp3d, *sdp3d_m;
               _aax2dProps sp2d;
               _aax3dProps sp3d;
               float ssv = 343.3f;
               float sdf = 1.0f;

               /**
                * Copying here prevents locking the listener the whole time
                * and it's used for just one time-frame anyhow.
                */
               dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               sdp3d_m = smixer->props3d->m_dprops3d;
               if (dptr_sensor)
               {
                  _aaxAudioFrameProcessDelayQueue(smixer);
                  ssv=_EFFECT_GETD3D(smixer,VELOCITY_EFFECT,AAX_SOUND_VELOCITY);
                  sdf=_EFFECT_GETD3D(smixer,VELOCITY_EFFECT,AAX_DOPPLER_FACTOR);

                  _aax_memcpy(&sp2d, smixer->props2d, sizeof(sp2d));
                  _aax_memcpy(&sp3d, smixer->props3d, sizeof(sp3d));
                  _aax_memcpy(&sdp3d, smixer->props3d->dprops3d,
                                      sizeof(sdp3d));
                  sp3d.root = &sp3d;
                  if (_PROP3D_MTX_HAS_CHANGED(smixer->props3d->dprops3d)) {
                     _aax_memcpy(sdp3d_m, &sdp3d, sizeof(sdp3d));
                  }
                  _PROP_CLEAR(smixer->props3d);
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }

               /* read-only data */
               _aax_memcpy(&sp2d.speaker, handle->info->speaker,
                                   sizeof(handle->info->speaker));
               _aax_memcpy(&sp2d.hrtf, handle->info->hrtf,
                                sizeof(handle->info->hrtf));

               /* update the modified properties */
               do {
                  mtx4f_t tmp;
                  mtx4dCopy(&sdp3d_m->matrix, &sdp3d.matrix);

                  // For velocity we're only interested in the normalized
                  // direction vector of the position matrix. Floats will do.
                  mtx4fFilld(tmp.m4, sdp3d.matrix.m4);
                  mtx4fMul(&sdp3d_m->velocity, &tmp, &sdp3d.velocity);
               } while (0);
               sdp3d_m->velocity.m4[VELOCITY][3] = 1.0f;

#if 0
 if (_PROP3D_MTX_HAS_CHANGED(sdp3d_m)) {
  printf("sensor modified matrix:\n");
  PRINT_MATRIX(sdp3d_m->matrix);
 }
#endif
#if 0
 if (_PROP3D_SPEED_HAS_CHANGED(sdp3d_m)) {
  printf("sensor modified velocity:\tsensor velocity:\n");
  PRINT_MATRICES(sdp3d_m->velocity, sdp3d.velocity);
 }
#endif
               /* clear the buffer for use by the subframe */
               rb->set_state(rb, RB_CLEARED);
               rb->set_state(rb, RB_STARTED);

               /* process registered emitters, audio-frames and sensors */
               res = _aaxAudioFrameProcess(rb, NULL, sensor, smixer, ssv, sdf,
                                           &sp2d, &sp3d, &sdp3d, be, be_handle,
                                           batched, false);
               _PROP3D_CLEAR(smixer->props3d->m_dprops3d);

               /*
                * if the final mixer actually did render something,
                * mix the data.
                */
               res = _aaxSoftwareMixerPlay(rb, smixer->devices,
                                           smixer->play_ringbuffers,
                                           smixer->frames, &sp2d,
                                           smixer->capturing, sensor,
                                           be, be_handle, fbe, fbe_handle,
                                           batched);

               if (handle->file.driver)
               {
                  _handle_t *fhandle = (_handle_t*)handle->file.driver;
                  if (_IS_PLAYING(fhandle))
                  {
                     _intBufferData *dptr;

                     dptr = _intBufGet(fhandle->sensors, _AAX_SENSOR, 0);
                     if (dptr)
                     {
                        _sensor_t* sensor = _intBufGetDataPtr(dptr);
                        _aaxAudioFrame *fmixer = sensor->mixer;
                        float dt = smixer->info->period_rate;

                        fmixer->curr_pos_sec += 1.0f/dt;
                        fmixer->curr_sample += res;
                        _intBufReleaseData(dptr, _AAX_SENSOR);
                     }
                  }
               }

               if (smixer->capturing) {
                  _aaxSignalTrigger(&handle->buffer_ready);
               }
            }
         }
      }
      else /* if (_IS_STANDBY(handle) */
      {
         if (handle->info->mode != AAX_MODE_READ)
         {
            _sensor_t *sensor = _intBufGetDataPtr(dptr_sensor);
            _aaxAudioFrame *smixer = sensor->mixer;
            if (smixer->emitters_3d || smixer->emitters_2d || smixer->frames)
            {
               dptr_sensor = _intBufGet(handle->sensors, _AAX_SENSOR, 0);
               if (dptr_sensor)
               {
                  _aaxNoneDriverProcessFrame(smixer);
                  _intBufReleaseData(dptr_sensor, _AAX_SENSOR);
               }
            }
         }
      }
   }

// handle->elapsed = _aaxTimerElapsed(handle->timer);
// _aaxTimerStop(handle->timer);

   return res;
}

