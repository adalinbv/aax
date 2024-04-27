
#include <stdio.h>
#include <stdlib.h>
#include <aax/aax.h>

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
         aaxFilter filter;

         printf("emitter filter: %-30s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxEmitterSetFilter(emitter, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxEmitterGetFilter(emitter, i) );
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
         aaxEffect effect;

         if (i == AAX_REVERB_EFFECT) continue;
         if (i == AAX_CONVOLUTION_EFFECT) continue;
         if (i == AAX_DELAY_EFFECT) continue;

         printf("emitter effect: %-30s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxEmitterSetEffect(emitter, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxEmitterGetEffect(emitter, i) );
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
         aaxFilter filter;

         if (i == AAX_GRAPHIC_EQUALIZER) continue;
         if (i == AAX_TIMED_GAIN_FILTER) continue;
         if (i == AAX_DYNAMIC_LAYER_FILTER) continue;

         printf("frame filter: %-32s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxAudioFrameSetFilter(frame, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxAudioFrameGetFilter(frame, i) );
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
         aaxEffect effect;

         if (i == AAX_TIMED_PITCH_EFFECT) continue;
         if (i == AAX_VELOCITY_EFFECT) break;
         if (i == AAX_CONVOLUTION_EFFECT) break;

         printf("frame effect: %-32s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxAudioFrameSetEffect(frame, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxAudioFrameGetEffect(frame, i) );
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
         aaxFilter filter;

         printf("scenery filter: %-30s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxScenerySetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxSceneryGetFilter(config, i) );
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
         aaxEffect effect;

         printf("scenery effect: %-30s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxScenerySetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxSceneryGetEffect(config, i) );
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
         aaxFilter filter;

         if (i == AAX_TIMED_GAIN_FILTER) continue;
         if (i == AAX_DIRECTIONAL_FILTER) continue;
         if (i == AAX_DISTANCE_FILTER) continue;
         if (i == AAX_FREQUENCY_FILTER) continue;
         if (i == AAX_DYNAMIC_LAYER_FILTER) continue;

         printf("mixer filter: %-32s: ", aaxFilterGetNameByType(config, i));
         TRY( filter = aaxFilterCreate(config, i) );
         TRY( aaxFilterSetSlot(filter, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxMixerSetFilter(config, filter) );
         TRY( aaxFilterDestroy(filter) );

         TRY( filter = aaxMixerGetFilter(config, i) );
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
         aaxEffect effect;

         if (i == AAX_TIMED_PITCH_EFFECT) continue;
         if (i == AAX_VELOCITY_EFFECT) continue;

         printf("mixer effect: %-32s: ", aaxEffectGetNameByType(config, i));
         TRY( effect = aaxEffectCreate(config, i) );
         TRY( aaxEffectSetSlot(effect, 0, AAX_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f) );
         TRY( aaxMixerSetEffect(config, effect) );
         TRY( aaxEffectDestroy(effect) );

         TRY( effect = aaxMixerGetEffect(config, i) );
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
