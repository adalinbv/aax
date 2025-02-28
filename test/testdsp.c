
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <aax/aax.h>

#define TEST_SLOT(s, d) { for (int n=0; n<4; ++n) { \
  if (fabsf(s[n]-d[n]) > 1e-5f) { printf("%i: p[%i]: %.2e != %.2e\n", __LINE__, n, s[n], d[n]); \
  exit(-1); } } \
}

#define TRY(a) if ((a) == 0) { \
  printf("%i: %s\n", __LINE__, aaxGetErrorString(aaxGetErrorNo())); \
  aaxDriverDestroy(config); exit(-1); \
}

static const char* aaxs = \
" <aeonwave>					\
    <sound frequency=\"440\">			\
      <waveform src=\"sine\"/>			\
    </sound>					\
    <emitter looping\"true\"/>                  \
  </aeonwave>";

static float filterSlotParams[AAX_FILTER_MAX][4] = {
 {  0.1f,  0.4f,  0.7f,  1.3f }, // AAX_FILTER_NONE
 { 100.f,  0.4f,  0.7f,  3.3f }, // AAX_EQUALIZER,
 {  1.3f,  0.0f,  1.0f,  0.1f }, // AAX_VOLUME_FILTER,
 {  0.3f, 33.0f,  0.7f,  0.2f }, // AAX_DYNAMIC_GAIN_FILTER,
 {  0.1f,  1.3f,  3.3f,  0.1f }, // AAX_TIMED_GAIN_FILTER,
 {  1.0f,  0.8f,  0.1f,  0.3f }, // AAX_DIRECTIONAL_FILTER,
 { 11.0f, 99.0f,  0.6f,  0.0f }, // AAX_DISTANCE_FILTER,
 {  1e4f,  0.2f,  1.2f,  2.2f }, // AAX_FREQUENCY_FILTER,
 {  0.1f, 40.0f,  0.5f,  0.4f }, // AAX_BITCRUSHER_FILTER,
 {  0.9f,  0.8f,  0.7f,  0.6f }, // AAX_GRAPHIC_EQUALIZER,
 {  0.1f,  1.1f,  0.6f,  0.3f }, // AAX_COMPRESSOR,
 {  0.3f,  5.1f,  0.9f,  0.1f }, // AAX_DYNAMIC_LAYER_FILTER,
 {  0.1f,  1.3f,  0.9f,  0.1f }  // AAX_TIMED_LAYER_FILTER,
};

static float effectSlotParams[AAX_EFFECT_MAX][4] = {
 {  0.1f,  0.2f,  0.3f,  0.4f }, // AAX_EFFECT_NONE
 {  1.1f,  2.2f,  0.6f,  3.1f }, // AAX_PITCH_EFFECT,
 {  0.2f, 11.0f,  0.2f,  0.9f }, // AAX_DYNAMIC_PITCH_EFFECT,
 {  0.9f,  0.1f,  1.4f,  0.9f }, // AAX_TIMED_PITCH_EFFECT,
 {  3.3f,  0.7f,  0.2f,  0.3f }, // AAX_DISTORTION_EFFECT,
 {  0.7f,  7.2f,  0.6f,  0.4f }, // AAX_PHASING_EFFECT,
 {  0.8f,  8.2f,  0.7f,  0.3f }, // AAX_CHORUS_EFFECT,
 {  0.9f,  9.2f,  0.8f,  0.2f }, // AAX_FLANGING_EFFECT,
 { 34e1f,  0.7f, 28e8f,  0.0f }, // AAX_VELOCITY_EFFECT,
 {  2e4f, 0.03f,  0.3f,  0.9f }, // AAX_REVERB_EFFECT,
 {  3e3f,  0.3f,  0.9f,  0.1f }, // AAX_CONVOLUTION_EFFECT,
 {  0.8f, 12.0f,  1e3f,  1e2f }, // AAX_RINGMODULATOR_EFFECT,
 {  0.7f,  9.2f,  0.8f,  0.2f }, // AAX_DELAY_EFFECT,
 { -1.0f,  7.1f,  0.9f,  0.3f }, // AAX_WAVEFOLD_EFFECT,
 {  0.0f,  0.2f,-20.0f, 99e1f }  // AAX_FREQUENCY_SHIFT_EFFECT,
};

int main()
{
   aaxConfig config;

   config = aaxDriverOpenDefault(AAX_MODE_WRITE_STEREO);
   if (config)
   {
      aaxEmitter emitter;
      aaxBuffer buffer;
      aaxFrame frame;
      int i;

      for (int i=AAX_EQUALIZER; i<AAX_FILTER_MAX; ++i) {
         aaxFilterDestroy(aaxMixerGetFilter(config, i));
      }
      for (int i=AAX_PITCH_EFFECT; i<AAX_EFFECT_MAX; ++i) {
         aaxEffectDestroy(aaxMixerGetEffect(config, i));
      }

      TRY( buffer = aaxBufferCreate(config, 1, 1, AAX_AAXS16S) );
      TRY( aaxBufferSetSetup(buffer, AAX_FREQUENCY, 22050.0f) );
      TRY( aaxBufferSetData(buffer, aaxs) );

      TRY( aaxMixerSetState(config, AAX_INITIALIZED) );
//    TRY( aaxMixerSetState(config, AAX_PLAYING) );

      TRY( emitter = aaxEmitterCreate() );
      TRY( aaxEmitterAddBuffer(emitter, buffer) );
      TRY( aaxEmitterSetState(emitter, AAX_PLAYING) );

      for (int i=AAX_EQUALIZER; i<AAX_FILTER_MAX; ++i) {
         aaxFilterDestroy(aaxEmitterGetFilter(emitter, i));
      }
      for (int i=AAX_PITCH_EFFECT; i<AAX_EFFECT_MAX; ++i) {
         aaxEffectDestroy(aaxEmitterGetEffect(emitter, i));
      }

      TRY( frame = aaxAudioFrameCreate(config) );
      TRY( aaxAudioFrameSetState(frame, AAX_PLAYING) );
      TRY( aaxMixerRegisterAudioFrame(config, frame) );
      TRY( aaxAudioFrameRegisterEmitter(frame, emitter) );

      for (int i=AAX_EQUALIZER; i<AAX_FILTER_MAX; ++i) {
         aaxFilterDestroy(aaxAudioFrameGetFilter(frame, i));
      }
      for (int i=AAX_PITCH_EFFECT; i<AAX_EFFECT_MAX; ++i) {
         aaxEffectDestroy(aaxAudioFrameGetEffect(frame, i));
      }

      for (i=AAX_VOLUME_FILTER; i<AAX_GRAPHIC_EQUALIZER; i++)
      {
          const char *s = aaxFilterGetNameByType(config, i);
          printf("supp. filter: %-32s: ", s);
          printf("%s\n", aaxIsFilterSupported(config, s) ? "ok" : "no");
      }

      for (i=AAX_PITCH_EFFECT; i<AAX_RINGMODULATOR_EFFECT; i++)
      {
          const char *s = aaxEffectGetNameByType(config, i);
          printf("supp. effect: %-32s: ", s);
          printf("%s\n", aaxIsEffectSupported(config, s) ? "ok" : "no");
      }

      /* emitters */
      for (i=AAX_VOLUME_FILTER; i<AAX_GRAPHIC_EQUALIZER; i++)
      {
         float slot[4];
         aaxFilter filter;

         printf("emitter filter: %-30s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlotParams(filter, 0, AAX_LINEAR, filterSlotParams[i]) );
         TRY( aaxEmitterSetFilter(emitter, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxEmitterGetFilter(emitter, i) );
         TRY( aaxFilterGetSlotParams(filter, 0, AAX_LINEAR, slot) );
         TEST_SLOT(filterSlotParams[i], slot);
         TRY( aaxFilterSetState(filter, AAX_TRUE) );
         TRY( aaxEmitterSetFilter(emitter, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxEmitterGetFilter(emitter, i) );
         TRY( aaxFilterSetState(filter, AAX_FALSE) );
         TRY( aaxEmitterSetFilter(emitter, filter) );
         TRY( aaxFilterDestroy(filter) );

         printf("ok\n");
      }

      for (i=AAX_PITCH_EFFECT; i<AAX_EFFECT_MAX; i++)
      {
         float slot[4];
         aaxEffect effect;

         if (i == AAX_REVERB_EFFECT) continue;
         if (i == AAX_CONVOLUTION_EFFECT) continue;
         if (i == AAX_DELAY_EFFECT) continue;

         printf("emitter effect: %-30s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlotParams(effect, 0, AAX_LINEAR, effectSlotParams[i]) );
         TRY( aaxEmitterSetEffect(emitter, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxEmitterGetEffect(emitter, i) );
         TRY( aaxEffectGetSlotParams(effect, 0, AAX_LINEAR, slot) );
         TEST_SLOT(effectSlotParams[i], slot);
         TRY( aaxEffectSetState(effect, AAX_TRUE) );
         TRY( aaxEmitterSetEffect(emitter, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxEmitterGetEffect(emitter, i) );
         TRY( aaxEffectSetState(effect, AAX_FALSE) );
         TRY( aaxEmitterSetEffect(emitter, effect) );
         TRY( aaxEffectDestroy(effect) );

         printf("ok\n");
      }
      printf("\n");

      /* audio-frames */
      for (i=AAX_EQUALIZER; i<AAX_FILTER_MAX; i++)
      {
         float slot[4];
         aaxFilter filter;

         if (i == AAX_GRAPHIC_EQUALIZER) continue;
         if (i == AAX_TIMED_GAIN_FILTER) continue;
         if (i == AAX_DYNAMIC_LAYER_FILTER) continue;
         if (i == AAX_TIMED_LAYER_FILTER) continue;

         printf("frame filter: %-32s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlotParams(filter, 0, AAX_LINEAR, filterSlotParams[i]) );
         TRY( aaxAudioFrameSetFilter(frame, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxAudioFrameGetFilter(frame, i) );
         TRY( aaxFilterGetSlotParams(filter, 0, AAX_LINEAR, slot) );
         TEST_SLOT(filterSlotParams[i], slot);
         TRY( aaxFilterSetState(filter, AAX_TRUE) );
         TRY( aaxAudioFrameSetFilter(frame, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxAudioFrameGetFilter(frame, i) );
         TRY( aaxFilterSetState(filter, AAX_FALSE) );
         TRY( aaxAudioFrameSetFilter(frame, filter) );
         TRY( aaxFilterDestroy(filter) );

         printf("ok\n");
      }

      for (i=AAX_PITCH_EFFECT; i<AAX_CONVOLUTION_EFFECT; i++)
      {
         float slot[4];
         aaxEffect effect;

         if (i == AAX_TIMED_PITCH_EFFECT) continue;
         if (i == AAX_VELOCITY_EFFECT) break;
         if (i == AAX_CONVOLUTION_EFFECT) break;

         printf("frame effect: %-32s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlotParams(effect, 0, AAX_LINEAR, effectSlotParams[i]) );
         TRY( aaxAudioFrameSetEffect(frame, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxAudioFrameGetEffect(frame, i) );
         TRY( aaxEffectGetSlotParams(effect, 0, AAX_LINEAR, slot) );
         TEST_SLOT(effectSlotParams[i], slot);
         TRY( aaxEffectSetState(effect, AAX_TRUE) );
         TRY( aaxAudioFrameSetEffect(frame, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxAudioFrameGetEffect(frame, i) );
         TRY( aaxEffectSetState(effect, AAX_FALSE) );
         TRY( aaxAudioFrameSetEffect(frame, effect) );
         TRY( aaxEffectDestroy(effect) );

         printf("ok\n");
      }
      printf("\n");

      /* scenery */
      for (i=AAX_DISTANCE_FILTER; i<AAX_BITCRUSHER_FILTER; i++)
      {
         float slot[4];
         aaxFilter filter;

         printf("scenery filter: %-30s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlotParams(filter, 0, AAX_LINEAR, filterSlotParams[i]) );
         TRY( aaxScenerySetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxSceneryGetFilter(config, i) );
         TRY( aaxFilterGetSlotParams(filter, 0, AAX_LINEAR, slot) );
         TEST_SLOT(filterSlotParams[i], slot);
         TRY( aaxFilterSetState(filter, AAX_TRUE) );
         TRY( aaxScenerySetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxSceneryGetFilter(config, i) );
         TRY( aaxFilterSetState(filter, AAX_FALSE) );
         TRY( aaxScenerySetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         printf("ok\n");
      }

      for (i=AAX_VELOCITY_EFFECT; i<AAX_REVERB_EFFECT; i++)
      {
         float slot[4];
         aaxEffect effect;

         printf("scenery effect: %-30s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlotParams(effect, 0, AAX_LINEAR, effectSlotParams[i]) );
         TRY( aaxScenerySetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxSceneryGetEffect(config, i) );
         TRY( aaxEffectGetSlotParams(effect, 0, AAX_LINEAR, slot) );
         TEST_SLOT(effectSlotParams[i], slot);
         TRY( aaxEffectSetState(effect, AAX_TRUE) );
         TRY( aaxScenerySetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxSceneryGetEffect(config, i) );
         TRY( aaxEffectSetState(effect, AAX_FALSE) );
         TRY( aaxScenerySetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         printf("ok\n");
      }
      printf("\n");

      /* mixer */
      for (i=AAX_EQUALIZER; i<AAX_FILTER_MAX; i++)
      {
         float slot[4];
         aaxFilter filter;

         if (i == AAX_TIMED_GAIN_FILTER) continue;
         if (i == AAX_DIRECTIONAL_FILTER) continue;
//       if (i == AAX_DISTANCE_FILTER) continue;
//       if (i == AAX_FREQUENCY_FILTER) continue;
         if (i == AAX_DYNAMIC_LAYER_FILTER) continue;
         if (i == AAX_TIMED_LAYER_FILTER) continue;

         printf("mixer filter: %-32s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlotParams(filter, 0, AAX_LINEAR, filterSlotParams[i]) );
         TRY( aaxMixerSetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxMixerGetFilter(config, i) );
         TRY( aaxFilterGetSlotParams(filter, 0, AAX_LINEAR, slot) );
         TEST_SLOT(filterSlotParams[i], slot);
         TRY( aaxFilterSetState(filter, AAX_TRUE) );
         TRY( aaxMixerSetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxMixerGetFilter(config, i) );
         TRY( aaxFilterSetState(filter, AAX_FALSE) );
         TRY( aaxMixerSetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         printf("ok\n");
      }

      for (i=AAX_PITCH_EFFECT; i<AAX_RINGMODULATOR_EFFECT; i++)
      {
         float slot[4];
         aaxEffect effect;

         if (i == AAX_TIMED_PITCH_EFFECT) continue;
         if (i == AAX_VELOCITY_EFFECT) continue;

         printf("mixer effect: %-32s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlotParams(effect, 0, AAX_LINEAR, effectSlotParams[i]) );
         TRY( aaxMixerSetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxMixerGetEffect(config, i) );
         TRY( aaxEffectGetSlotParams(effect, 0, AAX_LINEAR, slot) );
         TEST_SLOT(effectSlotParams[i], slot);
         TRY( aaxEffectSetState(effect, AAX_TRUE) );
         TRY( aaxMixerSetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxMixerGetEffect(config, i) );
         TRY( aaxEffectSetState(effect, AAX_FALSE) );
         TRY( aaxMixerSetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         printf("ok\n");
      }
      printf("\n");

      TRY( aaxEmitterSetState(emitter, AAX_PROCESSED) );
      TRY( aaxAudioFrameSetState(frame, AAX_STOPPED) );
//    TRY( aaxMixerSetState(config, AAX_STOPPED) );

      TRY( aaxAudioFrameDeregisterEmitter(frame, emitter) );
      TRY( aaxMixerDeregisterAudioFrame(config, frame) );

      TRY( aaxAudioFrameDestroy(frame) );
      TRY( aaxEmitterDestroy(emitter) );
      TRY( aaxBufferDestroy(buffer) );
   }

   TRY( aaxDriverClose(config) );
   TRY( aaxDriverDestroy(config) );
}
