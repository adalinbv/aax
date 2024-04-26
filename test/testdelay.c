
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

      TRY( buffer = aaxBufferCreate(config, 1, 1, AAX_AAXS16S) );
      TRY( aaxBufferSetSetup(buffer, AAX_FREQUENCY, 22050.0f) );
      TRY( aaxBufferSetData(buffer, aaxs) );

      TRY( aaxMixerSetState(config, AAX_INITIALIZED) );
//    TRY( aaxMixerSetState(config, AAX_PLAYING) );

      TRY( emitter = aaxEmitterCreate() );
      TRY( aaxEmitterAddBuffer(emitter, buffer) );
      TRY( aaxEmitterSetState(emitter, AAX_PLAYING) );

      TRY( frame = aaxAudioFrameCreate(config) );
      TRY( aaxAudioFrameSetState(frame, AAX_PLAYING) );
      TRY( aaxMixerRegisterAudioFrame(config, frame) );
      TRY( aaxAudioFrameRegisterEmitter(frame, emitter) );

      {
         int i = AAX_DELAY_EFFECT;
         aaxEffect effect;

         printf("emitter effect: %-30s: ", aaxEffectGetNameByType(config, i));
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
